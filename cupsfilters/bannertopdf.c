//
// Copyright 2012 Canonical Ltd.
// Copyright 2013 ALT Linux, Andrew V. Stepanov <stanv@altlinux.com>
// Copyright 2018 Sahil Arora <sahilarora.535@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <cupsfilters/ipp.h>
#include <cupsfilters/filter.h>
#include <cupsfilters/pdf.h>
#include <cupsfilters/raster.h>
#include <cupsfilters/libcups2-private.h>

#ifdef HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

#ifndef HAVE_OPEN_MEMSTREAM
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <cups/cups.h>
#include <cups/pwg.h>
#include <pdfio-content.h>

// Banner information flags.
typedef enum banner_info_e
{
  INFO_IMAGEABLE_AREA = 1,            // I - Imageable area
  INFO_JOB_BILLING = 1 << 1,          // I - Job billing
  INFO_JOB_ID = 1 << 2,               // I - Job ID
  INFO_JOB_NAME = 1 << 3,             // I - Job name
  INFO_JOB_ORIGINATING_HOST_NAME = 1 << 4, // I - Job originating host name
  INFO_JOB_ORIGINATING_USER_NAME = 1 << 5, // I - Job originating user name
  INFO_JOB_UUID = 1 << 6,             // I - Job UUID
  INFO_OPTIONS = 1 << 7,              // I - Options  
  INFO_PAPER_NAME = 1 << 8,           // I - Paper name
  INFO_PAPER_SIZE = 1 << 9,            // I - Paper size
  INFO_PRINTER_DRIVER_NAME = 1 << 10,   // I - Printer driver name
  INFO_PRINTER_DRIVER_VERSION = 1 << 11,  // I - Printer driver version
  INFO_PRINTER_INFO = 1 << 12,            // I - Printer info
  INFO_PRINTER_LOCATION = 1 << 13,        // I - Printer location
  INFO_PRINTER_MAKE_AND_MODEL = 1 << 14,    // I - Printer make and model
  INFO_PRINTER_NAME = 1 << 15,            // I - Printer name
  INFO_TIME_AT_CREATION = 1 << 16,        // I - Time at creation
  INFO_TIME_AT_PROCESSING = 1 << 17       // I - Time at processing
} banner_info_t;

typedef struct banner_s         // Banner data structure
{
  char *template_file;      // I - Template file path
  char *header, *footer;    // I - Header and footer text
  unsigned infos;           // I - Banner information flags
} banner_t;

typedef struct banner_font_s
{
#ifdef HAVE_FONTCONFIG
  FcPattern *pattern;
#endif
  char resource[32];
} banner_font_t;

#ifdef HAVE_FONTCONFIG
#  define BANNER_MAX_FONTS 32
#endif

typedef struct banner_fonts_s
{
  iterate_data_t *iterate_helper;
  cf_logfunc_t log;
  void *log_data;
  int warned_missing_font;
  int warned_supplementary;
#ifdef HAVE_FONTCONFIG
  banner_font_t fonts[BANNER_MAX_FONTS];
#else
  banner_font_t fonts[1];
#endif
  size_t num_fonts;
} banner_fonts_t;

static void
banner_free(banner_t *banner)
{
  if (banner)
  {
    if (banner->template_file)
      free(banner->template_file);
    if (banner->header)
      free(banner->header);
    if (banner->footer)
      free(banner->footer);
    free(banner);
  }
}

static int
parse_line(char *line,
	   char **key,
	   char **value)
{
  char *p = line;

  *key = *value = NULL;

  while (isspace(*p))
    p++;
  if (!*p || *p == '#')
    return (0);

  *key = p;
  while (*p && !isspace(*p))
    p++;
  if (!*p)
    return (1);

  *p++ = '\0';

  while (isspace(*p))
    p++;
  if (!*p)
    return (1);

  *value = p;

  // remove trailing space
  while (*p)
    p++;
  while (isspace(*--p))
    *p = '\0';

  return (1);
}

static unsigned
parse_show(char *s,
	   cf_logfunc_t log,
	   void *ld)
{
  unsigned info = 0;
  char *tok;

  for (tok = strtok(s, " \t"); tok; tok = strtok(NULL, " \t"))
  {
    if (!strcasecmp(tok, "imageable-area"))
      info |= INFO_IMAGEABLE_AREA;
    else if (!strcasecmp(tok, "job-billing"))
      info |= INFO_JOB_BILLING;
    else if (!strcasecmp(tok, "job-id"))
      info |= INFO_JOB_ID;
    else if (!strcasecmp(tok, "job-name"))
      info |= INFO_JOB_NAME;
    else if (!strcasecmp(tok, "job-originating-host-name"))
      info |= INFO_JOB_ORIGINATING_HOST_NAME;
    else if (!strcasecmp(tok, "job-originating-user-name"))
      info |= INFO_JOB_ORIGINATING_USER_NAME;
    else if (!strcasecmp(tok, "job-uuid"))
      info |= INFO_JOB_UUID;
    else if (!strcasecmp(tok, "options"))
      info |= INFO_OPTIONS;
    else if (!strcasecmp(tok, "paper-name"))
      info |= INFO_PAPER_NAME;
    else if (!strcasecmp(tok, "paper-size"))
      info |= INFO_PAPER_SIZE;
    else if (!strcasecmp(tok, "printer-driver-name"))
      info |= INFO_PRINTER_DRIVER_NAME;
    else if (!strcasecmp(tok, "printer-driver-version"))
      info |= INFO_PRINTER_DRIVER_VERSION;
    else if (!strcasecmp(tok, "printer-info"))
      info |= INFO_PRINTER_INFO;
    else if (!strcasecmp(tok, "printer-location"))
      info |= INFO_PRINTER_LOCATION;
    else if (!strcasecmp(tok, "printer-make-and-model"))
      info |= INFO_PRINTER_MAKE_AND_MODEL;
    else if (!strcasecmp(tok, "printer-name"))
      info |= INFO_PRINTER_NAME;
    else if (!strcasecmp(tok, "time-at-creation"))
      info |= INFO_TIME_AT_CREATION;
    else if (!strcasecmp(tok, "time-at-processing"))
      info |= INFO_TIME_AT_PROCESSING;
    else if (log)
      log(ld, CF_LOGLEVEL_ERROR, "cfFilterBannerToPDF: error: unknown value for 'Show': %s\n", tok);
  }

  return (info);
}

static char *
template_path(const char *name,
	      const char *datadir)
{
  char *result;

  if (name[0] == '/')
    return (strdup(name));

  result = malloc(strlen(datadir) + strlen(name) + 2);
  sprintf(result, "%s/%s", datadir, name);

  return (result);
}

static banner_t *
banner_new_from_file(const char *filename,
		     int *num_options,
		     cups_option_t **options,
		     const char *datadir,
		     cf_logfunc_t log,
		     void *ld)
{
  FILE *f;
  char *line = NULL;
  size_t len = 0, bytes_read;
  int linenr = 0;
  int ispdf = 0;
  int gotinfos = 0;
  banner_t *banner = NULL;

  if (!(f = fopen(filename, "r")))
  {
    if (log)
      log(ld, CF_LOGLEVEL_ERROR,
	  "cfFilterBannerToPDF: Error opening temporary file with input stream");
    goto out;
  }

  while ((bytes_read = getline(&line, &len, f)) != -1)
  {
    char *start = line;

    linenr ++;

    if (bytes_read == -1)
    {
      if (log)
	log(ld, CF_LOGLEVEL_ERROR,
	    "cfFilterBannerToPDF: No banner instructions found in input stream");
      goto out;
    }

    if (strncmp(line, "%PDF-", 5) == 0)
      ispdf = 1;
    while(start < line + len &&
	  ((ispdf && *start == '%') || isspace(*start)))
      start ++;
    if (strncasecmp(start, "#PDF-BANNER", 11) == 0 ||
	strncasecmp(start, "PDF-BANNER", 10) == 0)
      break;
  }

  if ((banner = calloc(1, sizeof *banner)) == NULL)
  {
    if (log)
      log(ld, CF_LOGLEVEL_ERROR, "cfFilterBannerToPDF: Not enough memory");
    goto out;
  }

  while (getline(&line, &len, f) != -1)
  {
    char *key, *value;

    linenr++;
    if (!parse_line(line, &key, &value))
      continue;

    if (!value)
    {
      if (ispdf)
	break;
      if (log)
	log(ld, CF_LOGLEVEL_ERROR,
	    "cfFilterBannerToPDF: Line %d is missing a value", linenr);
      continue;
    }

    if (ispdf)
      while (*key == '%')
	key ++;

    if (!banner->template_file && !strcasecmp(key, "template"))
      banner->template_file = template_path(value, datadir);
    else if (!banner->header && !strcasecmp(key, "header"))
      banner->header = strdup(value);
    else if (!banner->footer && !strcasecmp(key, "footer"))
      banner->footer = strdup(value);
    else if (!strcasecmp(key, "font"))
      *num_options = cupsAddOption("banner-font",
				   strdup(value), *num_options, options);
    else if (!strcasecmp(key, "font-size"))
      *num_options = cupsAddOption("banner-font-size",
				   strdup(value), *num_options, options);
    else if (!strcasecmp(key, "show"))
    {
      banner->infos = parse_show(value, log, ld);
      gotinfos = 1;
    }
    else if (!strcasecmp(key, "image") ||
	     !strcasecmp(key, "notice"))
    {
      if (log)
	log(ld, CF_LOGLEVEL_ERROR,
	    "cfFilterBannerToPDF: Note: %d: bannertopdf does not support '%s'",
	    linenr, key);
    }
    else
    {
      if (ispdf)
	break;
      if (log)
	log(ld, CF_LOGLEVEL_ERROR,
	    "cfFilterBannerToPDF: Error: %d: unknown keyword '%s'",
	    linenr, key);
    }
  }

  // load default template if none was specified
  if (!banner->template_file)
  {
    if (ispdf)
      banner->template_file = strdup(filename);
    else
      banner->template_file = template_path("default.pdf", datadir);
  }
  if (!gotinfos)
  {
    char *value = strdup("printer-name printer-info printer-location printer-make-and-model printer-driver-name printer-driver-version paper-size imageable-area job-id options time-at-creation time-at-processing");
    banner->infos = parse_show(value, log, ld);
    free(value);
  }

 out:
  free(line);
  if (f)
    fclose(f);
  // cppcheck-suppress memleak
  return (banner);
}

static int
get_int_option(const char *name,
	       int num_options,
	       cups_option_t *options,
	       int def)
{
  const char *value = cupsGetOption(name, num_options, options);
  return (value ? atoi(value) : def);
}

static int
duplex_marked(cf_filter_data_t *data)
{
  const char *val; // Pointer into value
  if ((val = cupsGetOption("Duplex",
			   data->num_options, data->options)) != NULL)
    return (strncasecmp(val, "Duplex", 6) == 0);
  else if ((val = cupsGetOption("sides",
				data->num_options, data->options)) != NULL)
    return (strncasecmp(val, "two-sided-", 10) == 0);
  else if ((val = cfIPPAttrEnumValForPrinter(data->printer_attrs,
					     data->job_attrs,
					     "sides")) != NULL)
    return (strncasecmp(val, "two-sided-", 10) == 0);
  else if (data->header)
    return (data->header->Duplex);
  return (0);
}

static unsigned int
utf8_next(const unsigned char **s)
{
  const unsigned char *p = *s;

  if (*p < 0x80)
  {
    *s = p + 1;
    return *p;
  }
  if ((*p & 0xe0) == 0xc0 && *p >= 0xc2 &&
      (p[1] & 0xc0) == 0x80)
  {
    *s = p + 2;
    return ((*p & 0x1f) << 6) | (p[1] & 0x3f);
  }
  if ((*p & 0xf0) == 0xe0 &&
      (p[1] & 0xc0) == 0x80 &&
      (p[2] & 0xc0) == 0x80 &&
      !(*p == 0xe0 && p[1] < 0xa0) &&
      !(*p == 0xed && p[1] >= 0xa0))
  {
    *s = p + 3;
    return ((*p & 0x0f) << 12) |
           ((p[1] & 0x3f) << 6) |
           (p[2] & 0x3f);
  }
  if ((*p & 0xf8) == 0xf0 && *p <= 0xf4 &&
      (p[1] & 0xc0) == 0x80 &&
      (p[2] & 0xc0) == 0x80 &&
      (p[3] & 0xc0) == 0x80 &&
      !(*p == 0xf0 && p[1] < 0x90) &&
      !(*p == 0xf4 && p[1] >= 0x90))
  {
    *s = p + 4;
    return ((*p & 0x07) << 18) |
           ((p[1] & 0x3f) << 12) |
           ((p[2] & 0x3f) << 6) |
           (p[3] & 0x3f);
  }

  *s = p + 1;
  return 0xfffd;
}

#ifdef HAVE_FONTCONFIG
static int
banner_add_font_file(banner_fonts_t *font_context,
		     const char *resource_name,
		     const char *filename)
{
  pdfio_obj_t *font;

  font = pdfioFileCreateFontObjFromFile(
      (pdfio_file_t *)font_context->iterate_helper->pdf, filename, true);
  if (!font)
    return 1;

  return pdfioPageDictAddFont(font_context->iterate_helper->page_dict,
			      resource_name, font) ? 0 : 1;
}

static int
banner_font_for_char(banner_fonts_t *font_context, unsigned int ch)
{
  FcCharSet *charset = NULL;
  FcFontSet *matches;
  FcPattern *query;
  FcResult result;
  size_t i;
  int match_num;

  for (i = 0; i < font_context->num_fonts; i ++)
    if (FcPatternGetCharSet(font_context->fonts[i].pattern, FC_CHARSET, 0,
			    &charset) == FcResultMatch &&
	FcCharSetHasChar(charset, ch))
      return (int)i;

  if (!FcInit() || font_context->num_fonts >= BANNER_MAX_FONTS ||
      (charset = FcCharSetCreate()) == NULL)
    return -1;
  if ((query = FcPatternCreate()) == NULL)
  {
    FcCharSetDestroy(charset);
    return -1;
  }

  FcCharSetAddChar(charset, ch);
  FcPatternAddCharSet(query, FC_CHARSET, charset);
  FcPatternAddBool(query, FC_SCALABLE, FcTrue);
  FcPatternAddBool(query, FC_OUTLINE, FcTrue);
  FcCharSetDestroy(charset);
  FcConfigSubstitute(NULL, query, FcMatchPattern);
  FcDefaultSubstitute(query);
  matches = FcFontSort(NULL, query, FcFalse, NULL, &result);
  FcPatternDestroy(query);
  if (!matches)
    return -1;

  for (match_num = 0; match_num < matches->nfont; match_num ++)
  {
    FcChar8 *filename;
    int index = 0;
    banner_font_t *font;

    if (FcPatternGetCharSet(matches->fonts[match_num], FC_CHARSET, 0,
			    &charset) !=
	    FcResultMatch || !FcCharSetHasChar(charset, ch) ||
	FcPatternGetString(matches->fonts[match_num], FC_FILE, 0, &filename) !=
	    FcResultMatch)
      continue;

    // PDFio opens the first face in a TrueType collection.
    FcPatternGetInteger(matches->fonts[match_num], FC_INDEX, 0, &index);
    if (index != 0)
      continue;

    font = font_context->fonts + font_context->num_fonts;
    snprintf(font->resource, sizeof(font->resource),
	     "bannertopdf-unicode-%u", (unsigned)font_context->num_fonts);
    font->pattern = FcPatternDuplicate(matches->fonts[match_num]);
    if (!font->pattern)
      continue;
    if (banner_add_font_file(font_context, font->resource,
			     (const char *)filename) != 0)
    {
      FcPatternDestroy(font->pattern);
      font->pattern = NULL;
      continue;
    }

    font_context->num_fonts ++;
    FcFontSetDestroy(matches);
    return (int)(font_context->num_fonts - 1);
  }

  FcFontSetDestroy(matches);
  return -1;
}

static void
banner_fonts_free(banner_fonts_t *font_context)
{
  size_t i;

  for (i = 0; i < font_context->num_fonts; i ++)
    FcPatternDestroy(font_context->fonts[i].pattern);
}
#else
static int
banner_font_for_char(banner_fonts_t *font_context, unsigned int ch)
{
  (void)font_context;
  (void)ch;
  return -1;
}

static void
banner_fonts_free(banner_fonts_t *font_context)
{
  (void)font_context;
}
#endif

static void
pdf_write_ascii_char(FILE *s, unsigned int ch)
{
  switch (ch)
  {
    case '\n' : fputs("\\n", s); break;
    case '\r' : fputs("\\r", s); break;
    case '\t' : fputs("\\t", s); break;
    case '\b' : fputs("\\b", s); break;
    case '\f' : fputs("\\f", s); break;
    case '('  : fputs("\\(", s); break;
    case ')'  : fputs("\\)", s); break;
    case '\\' : fputs("\\\\", s); break;
    default   :
        if (ch < 0x20 || ch == 0x7f)
          fprintf(s, "\\%03o", ch);
        else
          fputc((int)ch, s);
        break;
  }
}

static void
info_linef(FILE *s,
	   banner_fonts_t *font_context,
	   const char *key,
	   const char *valuefmt, ...)
{
  va_list ap, ap_copy;
  char *line;
  size_t keylen;
  int valuelen, current_font = -2;
  const unsigned char *p;

  va_start(ap, valuefmt);
  va_copy(ap_copy, ap);
  valuelen = vsnprintf(NULL, 0, valuefmt, ap_copy);
  va_end(ap_copy);
  keylen = strlen(key);
  if (valuelen < 0 ||
      (line = malloc(keylen + (size_t)valuelen + 3)) == NULL)
  {
    va_end(ap);
    return;
  }

  memcpy(line, key, keylen);
  line[keylen] = ':';
  line[keylen + 1] = ' ';
  vsnprintf(line + keylen + 2, (size_t)valuelen + 1, valuefmt, ap);
  va_end(ap);

  for (p = (const unsigned char *)line; *p;)
  {
    unsigned int ch = utf8_next(&p);
    int font_num;

    if (ch < 0x80)
    {
      if (current_font != -1)
      {
        fputs("/bannertopdf-font 14 Tf\n", s);
        current_font = -1;
      }
      fputc('(', s);
      pdf_write_ascii_char(s, ch);
      while (*p && *p < 0x80)
        pdf_write_ascii_char(s, *p ++);
      fputs(") Tj\n", s);
      continue;
    }

    if (ch > 0xffff)
    {
      if (!font_context->warned_supplementary && font_context->log)
      {
        font_context->log(font_context->log_data, CF_LOGLEVEL_WARN,
			  "cfFilterBannerToPDF: PDFio 1.6 cannot map "
			  "supplementary Unicode character U+%06X", ch);
        font_context->warned_supplementary = 1;
      }
      ch = 0xfffd;
    }

    font_num = banner_font_for_char(font_context, ch);
    if (font_num < 0)
    {
      if (!font_context->warned_missing_font && font_context->log)
      {
        font_context->log(font_context->log_data, CF_LOGLEVEL_WARN,
			  "cfFilterBannerToPDF: No embeddable font found "
			  "for Unicode character U+%04X", ch);
        font_context->warned_missing_font = 1;
      }
      if (current_font != -1)
      {
        fputs("/bannertopdf-font 14 Tf\n", s);
        current_font = -1;
      }
      fputs("(?) Tj\n", s);
      continue;
    }

    if (current_font != font_num)
    {
      fprintf(s, "/%s 14 Tf\n", font_context->fonts[font_num].resource);
      current_font = font_num;
    }
    fprintf(s, "<%04X> Tj\n", ch);
  }

  if (current_font != -1)
    fputs("/bannertopdf-font 14 Tf\n", s);
  fputs("T*\n", s);
  free(line);
}

static void
info_line(FILE *s,
	  banner_fonts_t *font_context,
	  const char *key,
	  const char *value)
{
  info_linef(s, font_context, key, "%s", value);
}

static void
info_line_time(FILE *s,
	       banner_fonts_t *font_context,
	       const char *key,
	       const char *timestamp)
{
  char buf[40];
  time_t time;

  if (timestamp)
  {
    time = (time_t)atoll(timestamp);
    strftime(buf, sizeof buf, "%c", localtime(&time));
    info_line(s, font_context, key, buf);
  }
  else
    info_line(s, font_context, key, "unknown");
}

static const char *
human_time(const char *timestamp)
{
  time_t time;
  int size = sizeof(char) * 40;
  char *buf = malloc(size);
  strcpy(buf, "unknown");

  if (timestamp)
  {
    time = (time_t)atoll(timestamp);
    strftime(buf, size, "%c", localtime(&time));
  }

  return (buf);
}

//
// Add new key & value.
//

static cf_opt_t *
add_opt(cf_opt_t *in_opt,
	const char *key,
	const char *val)
{
  if (!key || !val)
    return (in_opt);

  if (!strlen(key) || !strlen(val))
    return (in_opt);

  cf_opt_t *entry = malloc(sizeof(cf_opt_t));
  if (!entry)
    return (in_opt);

  entry->key = key;
  entry->val = val;
  entry->next = in_opt;

  return (entry);
}

//
// Collect all known info about current task.
// Bond PDF form field name with collected info.
//
// Create PDF form's field names according above.
//

static
cf_opt_t *get_known_opts(cf_filter_data_t *data,
			 const char *jobid,
			 const char *user,
			 const char *jobtitle,
			 int num_options,
			 cups_option_t *options)
{

  cf_opt_t *opt = NULL;
  ipp_t *printer_attrs = data->printer_attrs;
  ipp_attribute_t *ipp_attr;
  char buf[1024];
  const char *value = NULL;

  // Job ID
  opt = add_opt(opt, "job-id", jobid);

  // Job title
  opt = add_opt(opt, "job-title", jobtitle);

  // Printed by
  opt = add_opt(opt, "user", user);

  // Printer name
  opt = add_opt(opt, "printer-name", data->printer);

  // Printer info
  if ((value = cupsGetOption("printer-info",
			     num_options, options)) == NULL || !value[0])
    value =  getenv("PRINTER_INFO");
  opt = add_opt(opt, "printer-info", value);

  // Time at creation
  opt = add_opt(opt, "time-at-creation",
		human_time(cupsGetOption("time-at-creation", num_options,
					 options)));

  // Processing time
  opt = add_opt(opt, "time-at-processing",
		human_time(cupsGetOption("time-at-processing", num_options,
					 options)));

  // Billing information
  opt = add_opt(opt, "job-billing",
		cupsGetOption("job-billing", num_options, options));

  // Source hostname
  opt = add_opt(opt, "job-originating-host-name",
		cupsGetOption("job-originating-host-name", num_options,
			      options));

  // Banner font
  opt = add_opt(opt, "banner-font",
		cupsGetOption("banner-font", num_options, options));

  // Banner font size
  opt = add_opt(opt, "banner-font-size",
		cupsGetOption("banner-font-size", num_options, options));

  // Job UUID
  opt = add_opt(opt, "job-uuid",
		cupsGetOption("job-uuid", num_options, options));

  // Security context
  opt = add_opt(opt, "security-context",
		cupsGetOption("security-context", num_options, options));

  // Security context range part
  opt = add_opt(opt, "security-context-range",
		cupsGetOption("security-context-range", num_options, options));

  // Security context current range part
  const char *full_range = cupsGetOption("security-context-range", num_options,
					 options);
  if (full_range)
  {
    size_t cur_size = strcspn(full_range, "-");
    char *cur_range = strndup(full_range, cur_size);
    opt = add_opt(opt, "security-context-range-cur", cur_range);
  }

  // Security context type part
  opt = add_opt(opt, "security-context-type",
		cupsGetOption("security-context-type", num_options, options));

  // Security context role part
  opt = add_opt(opt, "security-context-role",
		cupsGetOption("security-context-role", num_options, options));

  // Security context user part
  opt = add_opt(opt, "security-context-user",
		cupsGetOption("security-context-user", num_options, options));

#if 0
  // Driver
  opt = add_opt(opt, "driver", "driverless");

  // Driver version
  opt = add_opt(opt, "driver-version", VERSION);
#endif

  // Make and model
  int is_fax = 0;
  char make[256];
  char *model;
  if ((ipp_attr = ippFindAttribute(printer_attrs, "ipp-features-supported",
				   IPP_TAG_ZERO)) != NULL &&
      ippContainsString(ipp_attr, "faxout"))
  {
    ipp_attr = ippFindAttribute(printer_attrs, "printer-uri-supported",
				IPP_TAG_ZERO);
    if (ipp_attr)
    {
      ippAttributeString(ipp_attr, buf, sizeof(buf));
      if (strcasestr(buf, "faxout"))
	is_fax = 1;
    }
  }

  if ((ipp_attr = ippFindAttribute(printer_attrs, "printer-info",
				   IPP_TAG_ZERO)) != NULL || 
      (ipp_attr = ippFindAttribute(printer_attrs, "printer-make-and-model",
				   IPP_TAG_ZERO)) != NULL)
    snprintf(make, sizeof(make), "%s", ippGetString(ipp_attr, 0, NULL));
  else
    snprintf(make, sizeof(make), "%s", "Unknown Printer");
  if (!strncasecmp(make, "Hewlett Packard ", 16) ||
      !strncasecmp(make, "Hewlett-Packard ", 16))
  {
    model = make + 16;
    snprintf(make, sizeof(make), "%s", "HP");
  }
  else if ((model = strchr(make, ' ')) != NULL)
    *model++ = '\0';
  else
    model = make;
  snprintf(buf, sizeof(buf), "%s %s%s",
	   make, model, (is_fax ? ", Fax" : ""));
  char *nickname = buf;
  opt = add_opt(opt, "make-and-model", nickname);

  return (opt);
}

static int
generate_banner_pdf(banner_t *banner,
		    cf_filter_data_t *data,
		    const char *jobid,
		    const char *user,
		    const char *jobtitle,
		    int num_options,
		    cups_option_t *options,
		    cf_logfunc_t log,
		    void *ld,
		    FILE *outputfp)
{
  int i;
  char *buf;
  size_t len;
  FILE *s;
  cf_pdf_t *input_doc, *output_doc;
  float page_width = 0.0, page_length = 0.0;
  float media_limits[4];
  float page_scale;
  unsigned copies;
  ipp_t *printer_attrs = data ? data->printer_attrs : NULL;
  ipp_attribute_t *ipp_attr;
  char buf2[1024];
  const char *value;
  banner_fonts_t font_context = { 0 };
#ifndef HAVE_OPEN_MEMSTREAM
  struct stat st;
#endif

  if (!(input_doc = cfPDFLoadTemplate(banner->template_file)))
  {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "PDF template must exist and contain exactly 1 page: %s",
		 banner->template_file);
    return (1);
  }

  iterate_data_t *iterate_helper = (iterate_data_t *)malloc(sizeof(iterate_data_t));
  output_doc = cfCopyPDFdoc(input_doc, outputfp, iterate_helper);
  font_context.iterate_helper = iterate_helper;
  font_context.log = log;
  font_context.log_data = ld;

  for (i = 0; i < 4; i ++)
    media_limits[i] = -1.0;
  if (data != NULL)
  {
    cfGetPageDimensions(data->printer_attrs, data->job_attrs,
			num_options, options,
			data->header, 0,
			&(page_width), &(page_length),
			&(media_limits[0]), &(media_limits[1]),
			&(media_limits[2]), &(media_limits[3]),
			NULL, NULL);

    cfSetPageDimensionsToDefault(&(page_width), &(page_length),
				 &(media_limits[0]), &(media_limits[1]),
				 &(media_limits[2]), &(media_limits[3]),
				 log, ld);

    media_limits[2] = page_width - media_limits[2];
    media_limits[3] = page_length - media_limits[3];
  }

  if (cfPDFResizePage1(output_doc, iterate_helper, 1, page_width, page_length, &page_scale) != 0)
  {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "Unable to resize requested PDF page");
    cfPDFFree(input_doc);
    cfPDFFree(output_doc);
    return (1);
  }

  if (cfPDFAddType1Font1(output_doc, iterate_helper, 1, "Courier") != 0)
  {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "Unable to add type1 font to requested PDF page");
    cfPDFFree(input_doc);
    cfPDFFree(output_doc);
    return (1);
  }

#ifdef HAVE_OPEN_MEMSTREAM
  s = open_memstream(&buf, &len);
#else
  if ((s = tmpfile()) == NULL)
  {
    if (log)
      log(ld, CF_LOGLEVEL_ERROR, "cfFilterBannerToPDF: Cannot create temp file: %s\n", strerror(errno));
    cfPDFFree(input_doc);
    cfPDFFree(output_doc);
    return (1);
  }
#endif

  if (banner->infos & INFO_IMAGEABLE_AREA)
  {
    fprintf(s, "q\n");
    fprintf(s, "0 0 0 RG\n");
    fprintf(s, "%f %f %f %f re S\n", media_limits[0] + 1.0,
	    media_limits[1] + 1.0,
	    media_limits[2] - media_limits[0] - 2.0,
	    media_limits[3] - media_limits[1] - 2.0);
    fprintf(s, "Q\n");
  }

  fprintf(s, "%f 0 0 %f 0 0 cm\n", page_scale, page_scale);

  fprintf(s, "0 0 0 rg\n");
  fprintf(s, "BT\n");
  fprintf(s, "/bannertopdf-font 14 Tf\n");
  fprintf(s, "83.662 335.0 Td\n");
  fprintf(s, "17 TL\n");

  if ((banner->infos & INFO_PRINTER_NAME) &&
      data->printer && data->printer[0] && data->printer[0] != '/')
    info_line(s, &font_context, "Printer", data->printer);

  if ((banner->infos & INFO_PRINTER_INFO) &&
      (((value = cupsGetOption("printer-info",
			       num_options, options)) != NULL && value[0]) ||
       ((value = getenv("PRINTER_INFO")) != NULL && value[0])))
    info_line(s, &font_context, "Description", value);

  if ((banner->infos & INFO_PRINTER_LOCATION) &&
      (((value = cupsGetOption("printer-location",
			       num_options, options)) != NULL && value[0]) ||
       ((value = getenv("PRINTER_LOCATION")) != NULL && value[0])))
    info_line(s, &font_context, "Location", value);

  if ((banner->infos & INFO_JOB_ID) &&
      data->printer && data->printer[0] && data->printer[0] != '/' &&
      jobid && jobid[0])
    info_linef(s, &font_context, "Job ID", "%s-%s", data->printer, jobid);

  if ((banner->infos & INFO_JOB_NAME) && jobtitle && jobtitle[0])
    info_line(s, &font_context, "Job Title", jobtitle);

  if ((banner->infos & INFO_JOB_ORIGINATING_HOST_NAME) &&
      (value = cupsGetOption("job-originating-host-name",
			     num_options, options)) != NULL && value[0])
    info_line(s, &font_context, "Printed from", value);

  if ((banner->infos & INFO_JOB_ORIGINATING_USER_NAME) && user && user[0])
    info_line(s, &font_context, "Printed by", user);

  if ((banner->infos & INFO_TIME_AT_CREATION) &&
      (value =
       cupsGetOption("time-at-creation", num_options, options)) != NULL &&
      value[0])
    info_line_time(s, &font_context, "Created at", value);

  if ((banner->infos & INFO_TIME_AT_PROCESSING) &&
      (value =
       cupsGetOption("time-at-processing", num_options, options)) != NULL &&
      value[0])
    info_line_time(s, &font_context, "Printed at", value);

  if ((banner->infos & INFO_JOB_BILLING) &&
      (value = cupsGetOption("job-billing", num_options, options)) != NULL &&
      value[0])
    info_line(s, &font_context, "Billing Information\n", value);

  if ((banner->infos & INFO_JOB_UUID) &&
      (value = cupsGetOption("job-uuid", num_options, options)) != NULL &&
      value[0])
    info_line(s, &font_context, "Job UUID", value);

  if ((banner->infos & INFO_PRINTER_DRIVER_NAME) ||
      (banner->infos & INFO_PRINTER_MAKE_AND_MODEL))
  {
    int is_fax = 0;
    char make[256];
    char *model;
    if ((ipp_attr = ippFindAttribute(printer_attrs,
				     "ipp-features-supported",
				     IPP_TAG_ZERO)) != NULL &&
	ippContainsString(ipp_attr, "faxout"))
    {
      ipp_attr = ippFindAttribute(printer_attrs, "printer-uri-supported",
				  IPP_TAG_ZERO);
      if (ipp_attr)
      {
	ippAttributeString(ipp_attr, buf2, sizeof(buf2));
	if (strcasestr(buf2, "faxout"))
	  is_fax = 1;
      }
    }

    if ((ipp_attr = ippFindAttribute(printer_attrs,
				     "printer-info",
				     IPP_TAG_ZERO)) != NULL ||
	(ipp_attr = ippFindAttribute(printer_attrs,
				     "printer-make-and-model",
				     IPP_TAG_ZERO)) != NULL)
    {
      snprintf(make, sizeof(make), "%s", ippGetString(ipp_attr, 0, NULL));

      if (!strncasecmp(make, "Hewlett Packard ", 16) ||
	  !strncasecmp(make, "Hewlett-Packard ", 16))
      {
	model = make + 16;
	snprintf(make, sizeof(make), "%s", "HP");
      }
      else if ((model = strchr(make, ' ')) != NULL)
	*model++ = '\0';
      else
	model = make;
      snprintf(buf2, sizeof(buf2), "%s %s%s",
	       make, model, (is_fax ? " (Fax)" : ""));
      char *nickname = buf2;
      info_line(s, &font_context, "Make and Model", nickname);
    }
  }

  if (banner->infos & INFO_IMAGEABLE_AREA)
  {
    info_linef(s, &font_context, "Media Limits", "%.2f x %.2f to %.2f x %.2f inches",
	       media_limits[0] / 72.0,
	       media_limits[1] / 72.0,
	       media_limits[2] / 72.0,
	       media_limits[3] / 72.0);
    info_linef(s, &font_context, "Media Limits", "%.2f x %.2f to %.2f x %.2f cm",
	       media_limits[0] / 72.0 * 2.54,
	       media_limits[1] / 72.0 * 2.54,
	       media_limits[2] / 72.0 * 2.54,
	       media_limits[3] / 72.0 * 2.54);
  }

  fprintf(s, "ET\n");
  banner_fonts_free(&font_context);
#ifndef HAVE_OPEN_MEMSTREAM
  fflush(s);
  if (fstat(fileno(s), &st) < 0)
  {
    if (log)
      log(ld, CF_LOGLEVEL_ERROR, "cfFilterBannerToPDF: Cannot fstat(): %s\n", , strerror(errno));
    return (1);
  }
  fseek(s, 0L, SEEK_SET);
  if ((buf = malloc(st.st_size + 1)) == NULL)
  {
    if (log)
      log(ld, CF_LOGLEVEL_ERROR, "cfFilterBannerToPDF: Cannot malloc(): %s\n", , strerror(errno));
    return (1);
  }
  size_t nbytes = fread(buf, 1, st.st_size, s);
  buf[st.st_size] = '\0';
  len = strlen(buf);
#endif // !HAVE_OPEN_MEMSTREAM
  fclose(s);

  cf_opt_t *known_opts = get_known_opts(data,
					jobid,
					user,
					jobtitle,
					num_options,
					options);

  //
  // Try to find a PDF form in PDF template and fill it.
  //

  if (cfPDFFillForm(output_doc, known_opts) != 0)
  {
    //
    // Could we fill a PDF form? If no, just add PDF stream.
    //

    if (cfPDFPrependStream1(input_doc, iterate_helper, 1, buf, len) != 0)
    {
      if (log) log(ld, CF_LOGLEVEL_ERROR,
		   "Unable to prepend stream to requested PDF page");
    }
  }

  copies = get_int_option("number-up", num_options, options, 1);

  if (duplex_marked(data))
    copies *= 2;

  if (copies > 1)
  {
    if (cfPDFDuplicatePage(output_doc, 1, copies - 1) != 0)
    {
      if (log) log(ld, CF_LOGLEVEL_ERROR,
		   "Unable to duplicate requested PDF page");
    }
  }

  cfPDFWrite(output_doc, outputfp);

  cf_opt_t *opt_current = known_opts;
  cf_opt_t *opt_next = NULL;
  while (opt_current != NULL)
  {
    opt_next = opt_current->next;
    free(opt_current);
    opt_current = opt_next;
  }

  free(buf);
  free(iterate_helper);
  cfPDFFree(input_doc);
  cfPDFFree(output_doc);
  return (0);
}

//
// 'cfFilterBannerToPDF()' - Convert banner instructions to PDF.
//

int                                       // O - 0 on success, 1 on error
cfFilterBannerToPDF(int inputfd,         // I - File descriptor input stream
		    int outputfd,        // I - File descriptor output stream
		    int inputseekable,   // I - Is input stream seekable? (unused)
		    cf_filter_data_t *data, // I - Job and printer data
		    void *parameters)    // I - Filter-specific parameters (Template/Banner data directory)
{
  banner_t *banner;
  int num_options = 0;
  int ret;
  FILE *inputfp = NULL;
  FILE *outputfp = NULL;
  int tempfd;
  cups_option_t *options = NULL;
  char tempfile[1024], buffer[1024];
  size_t bytes;
  const char *datadir = (const char *)parameters;

  cf_logfunc_t log = data->logfunc;
  void *ld = data->logdata;
  cf_filter_iscanceledfunc_t iscanceled = data->iscanceledfunc;
  void *icd = data->iscanceleddata;
  char jobid[50];

  num_options = cfJoinJobOptionsAndAttrs(data, num_options, &options);

  //
  // Open the input data stream specified by the inputfd...
  //

  if ((inputfp = fdopen(inputfd, "rb")) == NULL)
  {
    if (!iscanceled || !iscanceled(icd))
    {
      if (log)
	log(ld, CF_LOGLEVEL_DEBUG,
	    "cfFilterBannerToPDF: Unable to open input data stream.");
    }
    return (1);
  }

  //
  // Copy the input data stream into a temporary file...
  //

  if ((tempfd = cupsCreateTempFd(NULL, NULL, tempfile, sizeof(tempfile))) < 0)
  {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "cfFilterBannerToPDF: Unable to copy input file: %s",
		 strerror(errno));
    if (inputfp)
      fclose(inputfp);
    return (1);
  }

  if (log) log(ld, CF_LOGLEVEL_DEBUG,
	       "cfFilterBannerToPDF: Copying input to temp file \"%s\"",
	       tempfile);

  while ((bytes = fread(buffer, 1, sizeof(buffer), inputfp)) > 0)
    bytes = write(tempfd, buffer, bytes);

  if (inputfd)
  {
    fclose(inputfp);
    inputfp = NULL;
    close(inputfd);
  }
  close(tempfd);

  //
  // Open the output data stream specified by the outputfd...
  //

  if ((outputfp = fdopen(outputfd, "w")) == NULL)
  {
    if (!iscanceled || !iscanceled(icd))
    {
      if (log)
	log(ld, CF_LOGLEVEL_DEBUG,
	    "cfFilterBannerToPDF: Unable to open output data stream.");
    }

    if (inputfp)
      fclose(inputfp);

    return (1);
  }

  //
  // Parse the instructions...
  //

  banner = banner_new_from_file(tempfile, &num_options, &options, datadir,
				log, ld);

  if (!banner)
  {
    if (log)
      log(ld, CF_LOGLEVEL_ERROR, "cfFilterBannerToPDF: Could not read banner file");

    if (inputfp)
      fclose(inputfp);

    if (outputfp)
      fclose(outputfp);

    return (1);
  }

  sprintf(jobid, "%d", data->job_id);

  //
  // Create the banner/test page
  //

  ret = generate_banner_pdf(banner,
			    data,
			    jobid,
			    data->job_user,
			    data->job_title,
			    num_options,
			    options,
			    log,
			    ld,
			    outputfp);

  //
  // Clean up...
  //

  banner_free(banner);
  if (options) cupsFreeOptions(num_options, options);
  unlink(tempfile);
  if (inputfp)
    fclose(inputfp);
  // The output stream has to be flushed and closed here. When this filter
  // function is called from cfFilterChain() (as the "universal" filter does),
  // the parent closes the raw output file descriptor and exits right after
  // this function returns, so a still buffered tail of the generated PDF
  // (cross-reference table and "%%EOF") would be lost and the following
  // filter in the chain would fail to parse the file.
  if (outputfp)
    fclose(outputfp);

  return (ret);
}
