//
// Option support functions for the IPP tools.
//
// Copyright © 2024-2025 by Uddhav Phatak <uddhavphatak@gmail.com>
// Copyright © 2023-2024 by OpenPrinting.
// Copyright © 2022-2023 by the Printer Working Group.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "ipp-options-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#    define _CUPS_INLINE static inline

#  ifdef DEBUG
#    define DEBUG_puts(x) _cups_debug_puts(x)
#    define DEBUG_printf(...) _cups_debug_printf(__VA_ARGS__)
#  else
#    define DEBUG_puts(x)
#    define DEBUG_printf(...)
#  endif // DEBUG

//
// Constants...
//

#define DEFAULT_COLOR		"white"	// Default "media-color" value
#define DEFAULT_MARGIN_BOTTOM_TOP 1250	// Default bottom/top margin of 1/2"
#define DEFAULT_MARGIN_LEFT_RIGHT 625	// Default left/right margin of 1/4"
#define DEFAULT_SIZE_NAME	"iso_a4_210x297mm"
					// Default "media-size-name" value
#define DEFAULT_SOURCE		"auto"	// Default "media-source" value
#define DEFAULT_TYPE		"stationery"
					// Default "media-type" value

#  ifdef _CUPS_INLINE
_CUPS_INLINE int                        // O - 1 on match, 0 otherwise
_cups_isspace(int ch)                   // I - Character to test
{
  return (ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\v');
}

_CUPS_INLINE int                        // O - 1 on match, 0 otherwise
_cups_isupper(int ch)                   // I - Character to test
{
  return (ch >= 'A' && ch <= 'Z');
}

_CUPS_INLINE int                        // O - Converted character
_cups_tolower(int ch)                   // I - Character to convert
{
  return (_cups_isupper(ch) ? ch - 'A' + 'a' : ch);
}

#  endif


cups_array_t *                          // O - Array
cupsArrayNew1(cups_array_cb_t  f,        // I - Comparison callback function or `NULL` for an unsorted array
             void             *d,       // I - User data or `NULL`
             cups_ahash_cb_t  hf,       // I - Hash callback function or `NULL` for unhashed lookups
             size_t           hsize,    // I - Hash size (>= `0`)
             cups_acopy_cb_t  cf,       // I - Copy callback function or `NULL` for none
             cups_afree_cb_t  ff)       // I - Free callback function or `NULL` for none
{
  cups_array_t  *a;                     // Array


  // Allocate memory for the array...
  if ((a = calloc(1, sizeof(cups_array_t))) == NULL)
    return (NULL);

  a->compare   = f;
  a->data      = d;
  a->current   = SIZE_MAX;
  a->insert    = SIZE_MAX;
  a->num_saved = 0;
  a->unique    = true;

  if (hsize > 0 && hf)
  {
    a->hashfunc  = hf;
    a->hashsize  = hsize;
    a->hash      = malloc((size_t)hsize * sizeof(size_t));

    if (!a->hash)
    {
      free(a);
      return (NULL);
    }

    memset(a->hash, -1, (size_t)hsize * sizeof(size_t));
  }

  a->copyfunc = cf;
  a->freefunc = ff;

  return (a);
}

//
// '_cups_strcpy()' - Copy a string allowing for overlapping strings.
//

void
_cups_strcpy(char       *dst,           // I - Destination string
             const char *src)           // I - Source string
{
  while (*src)
    *dst++ = *src++;

  *dst = '\0';
}

//
// '_cups_strncasecmp()' - Do a case-insensitive comparison on up to N chars.
//

int                                     // O - Result of comparison (-1, 0, or 1)
_cups_strncasecmp(const char *s,        // I - First string
                  const char *t,        // I - Second string
                  size_t     n)         // I - Maximum number of characters to compare
{
  while (*s != '\0' && *t != '\0' && n > 0)
  {
    if (_cups_tolower(*s) < _cups_tolower(*t))
      return (-1);
    else if (_cups_tolower(*s) > _cups_tolower(*t))
      return (1);

    s ++;
    t ++;
    n --;
  }

  if (n == 0)
    return (0);
  else if (*s == '\0' && *t == '\0')
    return (0);
  else if (*s != '\0')
    return (1);
  else
    return (-1);
}




//
// 'cupsParseOptions2()' - Parse options from a command-line argument.
//
// This function converts space-delimited name/value pairs according
// to the PAPI text option ABNF specification. Collection values
// ("name={a=... b=... c=...}") are stored with the curley brackets
// intact - use @code cupsParseOptions2@ on the value to extract the
// collection attributes.
//
// The "end" argument, if not `NULL`, receives a pointer to the end of the
// options.
//

size_t					// O - Number of options found
cupsParseOptions2(
    const char    *arg,			// I - Argument to parse
    const char    **end,		// O - Pointer to end of options or `NULL` for "don't care"
    size_t        num_options,		// I - Number of options
    cups_option_t **options)		// O - Options found
{
  char	*copyarg,			// Copy of input string
	*ptr,				// Pointer into string
	*name,				// Pointer to name
	*value,				// Pointer to value
	sep,				// Separator character
	quote;				// Quote character


  // Range check input...
  if (end)
    *end = NULL;

  if (!arg)
    return (num_options);

  if (!options)
    return (0);

  // Make a copy of the argument string and then divide it up...
  if ((copyarg = strdup(arg)) == NULL)
  {
    DEBUG_puts("1cupsParseOptions2: Unable to copy arg string");
    return (num_options);
  }

  if (*copyarg == '{')
    ptr  = copyarg + 1;
  else
    ptr = copyarg;

  // Skip leading spaces...
  while (_cups_isspace(*ptr))
    ptr ++;

  // Loop through the string...
  while (*ptr != '\0')
  {
    // Get the name up to a SPACE, =, or end-of-string...
    name = ptr;
    while (!strchr("\f\n\r\t\v =", *ptr) && *ptr)
      ptr ++;

    // Avoid an empty name...
    if (ptr == name)
      break;

    // End after the closing brace...
    if (*ptr == '}' && *copyarg == '{')
    {
      *ptr++ = '\0';
      break;
    }

    // Skip trailing spaces...
    while (_cups_isspace(*ptr))
      *ptr++ = '\0';

    if ((sep = *ptr) == '=')
      *ptr++ = '\0';

    if (sep != '=')
    {
      // Boolean option...
      if (!_cups_strncasecmp(name, "no", 2))
        num_options = cupsAddOption(name + 2, "false", num_options, options);
      else
        num_options = cupsAddOption(name, "true", num_options, options);

      continue;
    }

    // Remove = and parse the value...
    value = ptr;

    while (*ptr && !_cups_isspace(*ptr))
    {
      if (*ptr == ',')
      {
        ptr ++;
      }
      else if (*ptr == '\'' || *ptr == '\"')
      {
        // Quoted string constant...
	quote = *ptr;
	_cups_strcpy(ptr, ptr + 1);

	while (*ptr != quote && *ptr)
	{
	  if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);

	  ptr ++;
	}

	if (*ptr)
	  _cups_strcpy(ptr, ptr + 1);
      }
      else if (*ptr == '{')
      {
        // Collection value...
	int depth;			// Nesting depth for braces

	for (depth = 0; *ptr; ptr ++)
	{
	  if (*ptr == '{')
	  {
	    depth ++;
	  }
	  else if (*ptr == '}')
	  {
	    depth --;
	    if (!depth)
	    {
	      ptr ++;
	      break;
	    }
	  }
	  else if (*ptr == '\\' && ptr[1])
	  {
	    _cups_strcpy(ptr, ptr + 1);
	  }
	}
      }
      else
      {
        // Normal space-delimited string...
	while (*ptr && !_cups_isspace(*ptr))
	{
	  if (*ptr == '}' && *copyarg == '{')
	  {
	    *ptr++ = '\0';
	    break;
	  }

	  if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);

	  ptr ++;
	}
      }
    }

    if (*ptr != '\0')
      *ptr++ = '\0';

    // Skip trailing whitespace...
    while (_cups_isspace(*ptr))
      ptr ++;

    // Add the string value...
    num_options = cupsAddOption(name, value, num_options, options);
  }

  // Save the progress in the input string...
  if (end)
    *end = arg + (ptr - copyarg);

  // Free the copy of the argument we made and return the number of options found.
  free(copyarg);

  return (num_options);
}


//
// 'validate_end()' - Validate the last UTF-8 character in a buffer.
//

static void
validate_end(char *s,                   // I - Pointer to start of string
             char *end)                 // I - Pointer to end of string
{
  char *ptr = end - 1;                  // Pointer into string


  if (ptr > s && *ptr & 0x80)
  {
    while ((*ptr & 0xc0) == 0x80 && ptr > s)
      ptr --;

    if ((*ptr & 0xe0) == 0xc0)
    {
      // Verify 2-byte UTF-8 sequence...
      if ((end - ptr) != 2)
        *ptr = '\0';
    }
    else if ((*ptr & 0xf0) == 0xe0)
    {
      // Verify 3-byte UTF-8 sequence...
      if ((end - ptr) != 3)
        *ptr = '\0';
    }
    else if ((*ptr & 0xf8) == 0xf0)
    {
      // Verify 4-byte UTF-8 sequence...
      if ((end - ptr) != 4)
        *ptr = '\0';
    }
    else if (*ptr & 0x80)
    {
      // Invalid sequence at end...
      *ptr = '\0';
    }
  }
}


//
// 'cupsConcatString()' - Safely concatenate two UTF-8 strings.
//

size_t					// O - Length of string
cupsConcatString(char       *dst,	// O - Destination string
                 const char *src,	// I - Source string
	         size_t     dstsize)	// I - Size of destination string buffer
{
  size_t	srclen;			// Length of source string
  size_t	dstlen;			// Length of destination string


  // Range check input...
  if (!dst || !src || dstsize == 0)
    return (0);

  // Figure out how much room is left...
  dstlen = strlen(dst);

  if (dstsize < (dstlen + 1))
    return (dstlen);		        // No room, return immediately...

  dstsize -= dstlen + 1;

  // Figure out how much room is needed...
  srclen = strlen(src);

  // Copy the appropriate amount...
  if (srclen <= dstsize)
  {
    // String fits, just copy over...
    memmove(dst + dstlen, src, srclen);
    dst[dstlen + srclen] = '\0';
  }
  else
  {
    // String too big, copy what we can and clean up the end...
    memmove(dst + dstlen, src, dstsize);
    dst[dstlen + dstsize] = '\0';

    validate_end(dst, dst + dstlen + dstsize);
  }

  return (dstlen + srclen);
}


//
// 'cupsArrayGetElement()' - Get the N-th element in the array.
//
void *                                  // O - N-th element or `NULL`
cupsArrayGetElement(cups_array_t *a,    // I - Array
                    size_t       n)     // I - Index into array, starting at 0
{
  // Range check input...
  if (!a || n >= a->num_elements)
    return (NULL);

  a->current = n;

  return (a->elements[n]);
}


//
// 'cupsArrayGetFirst()' - Get the first element in an array.
//

void *                                  // O - First element or `NULL` if the array is empty
cupsArrayGetFirst(cups_array_t *a)      // I - Array
{
  return (cupsArrayGetElement(a, 0));
}

//
// 'cupsArrayGetNext()' - Get the next element in an array.
//
// This function returns the next element in an array.  The next element is
// undefined until you call @link cupsArrayFind@, @link cupsArrayGetElement@,
// @link cupsArrayGetFirst@, or @link cupsArrayGetLast@ to set the current
// element.
//

void *                                  // O - Next element or @code NULL@
cupsArrayGetNext(cups_array_t *a)       // I - Array
{
  // Range check input...
  if (!a || a->num_elements == 0)
    return (NULL);
  else if (a->current == SIZE_MAX)
    return (cupsArrayGetElement(a, 0)); 
  else
    return (cupsArrayGetElement(a, a->current + 1));
} 

//
// 'cupsCopyString()' - Safely copy a UTF-8 string.
//

size_t                                  // O - Length of string
cupsCopyString(char       *dst,         // O - Destination string
               const char *src,         // I - Source string
               size_t     dstsize)      // I - Size of destination string buffer
{
  size_t        srclen;                 // Length of source string


  // Range check input...
  if (!dst || !src || dstsize == 0)
  {
    if (dst)
      *dst = '\0';
    return (0);
  }

  // Figure out how much room is needed...
  dstsize --;

  srclen = strlen(src);

  // Copy the appropriate amount...
  if (srclen <= dstsize)
  {
    // Source string will fit...
    memmove(dst, src, srclen);
    dst[srclen] = '\0';
  }
  else
  {
    // Source string too big, copy what we can and clean up the end...
    memmove(dst, src, dstsize);
    dst[dstsize] = '\0';

    validate_end(dst, dst + dstsize);
  }

  return (srclen);
}
     
//
// Local functions...
//

static int			compare_overrides(cf_filter_override_t *a, cf_filter_override_t *b);
static cf_filter_override_t	*copy_override(cf_filter_override_t *is);
static const char		*get_option(const char *name, size_t num_options, cups_option_t *options);
static bool			parse_media(const char *value, cups_media_t *media);


//
// 'cfFilterOptionsIsPageInRange()' - Check whether a page number is included in the "page-ranges" value(s).
//

bool	       // O - `true` if page in ranges, `false` otherwise
cfFilterOptionsIsPageInRange(cf_filter_options_t *ippo,  // I - IPP options
                             int           page)	 // I - Page number (starting at 1)
{
  size_t	i;			        // Looping var

  if (!ippo)
    return (true);

  if (ippo->page_set == CF_FILTER_PAGESET_ODD && (page % 2) == 0)
    return (false); 

  if (ippo->page_set == CF_FILTER_PAGESET_EVEN && (page % 2) != 0)
    return (false);

  if (ippo->num_page_ranges == 0)
    return (true);

  for (i = 0; i < ippo->num_page_ranges; i ++)
  {
    if (page >= ippo->page_ranges[i].lower && page <= ippo->page_ranges[i].upper)
      return (true);
  }

  return (false);
}


//
// 'cfFilterOptionsDelete()' - Free memory used by IPP options.
//

void
cfFilterOptionsDelete(cf_filter_options_t *ippo)	// I - IPP options
{
  // Range check input...
  if (!ippo)
    return;

  // Free memory
  cupsArrayDelete(ippo->overrides);
  free(ippo);
}

//
// 'cfFilterOptionsCreate()' - Allocate memory for IPP options and populate.
//
// This function initializes an `cf_filter_options_t` structure from the environment
// and command-line options passed in "num_options" and "options".
//

cf_filter_options_t *			    // O - IPP options
cfFilterOptionsCreate(size_t num_options,   // I - Number of command-line options
              cups_option_t *options)	    // I - Command-line options
{
  cf_filter_options_t	*ippo;		// IPP options
  const char	*value;			// Option value...
  int		intvalue;		// Integer value...
  size_t	i;			// Looping var
  size_t	num_col;		// Number of collection values
  cups_option_t	*col;			// Collection values
  const char	*nextcol;		// Next collection value


  // Allocate memory and set defaults...
  if ((ippo = calloc(1, sizeof(cf_filter_options_t))) == NULL)
    return (NULL);

  ippo->copies                     = 1;
  ippo->image_orientation          = CF_FILTER_ORIENT_NONE;
  ippo->multiple_document_handling = CF_FILTER_HANDLING_COLLATED_COPIES;
  ippo->print_scaling 		   = CF_FILTER_SCALING_NONE;
  ippo->number_up                  = 1;
  ippo->orientation_requested      = CF_FILTER_ORIENT_NONE;
  ippo->page_set      		   = CF_FILTER_PAGESET_ALL;
  ippo->reverse_order 		   = false;
  ippo->mirror                     = false;
  ippo->pdf_auto_rotate            = false;

  cupsCopyString(ippo->page_border, "none", sizeof(ippo->page_border));
  cupsCopyString(ippo->page_label, "", sizeof(ippo->page_label));
  cupsCopyString(ippo->job_name, "Untitled", sizeof(ippo->job_name));
  cupsCopyString(ippo->job_originating_user_name, "Guest", sizeof(ippo->job_originating_user_name));
  cupsCopyString(ippo->job_sheets, "none", sizeof(ippo->job_sheets));
  cupsCopyString(ippo->sides, "one-sided", sizeof(ippo->sides));

  // "media" and "media-col" needs to be handled specially to make sure that
  // "media" can override "media-col-default"...
  if ((value = cupsGetOption("media", num_options, options)) == NULL)
    value = getenv("IPP_MEDIA");

  if (!value)
  {
    if ((value = get_option("media-col", num_options, options)) == NULL)
      value = get_option("media", num_options, options);
  }

  if (value)
    parse_media(value, &ippo->media);
  else
    parse_media(DEFAULT_SIZE_NAME, &ippo->media);

  ippo->job_error_sheet.media = ippo->media;
  ippo->job_sheets_media      = ippo->media;
  ippo->separator_media       = ippo->media;

  // Set the rest of the options...
  if ((value = get_option("output-order", num_options, options)) != NULL)
  {
    if (!strcasecmp(value, "reverse"))
      ippo->reverse_order = true;
  }

  if (get_option("landscape", num_options, options))
  {
    ippo->orientation_requested = CF_FILTER_ORIENT_LANDSCAPE;
  }

  if ((value = get_option("Duplex", num_options, options)) != NULL &&
    (!strcasecmp(value, "true") || !strcasecmp(value, "on") || !strcasecmp(value, "yes")))
  {
    cupsCopyString(ippo->sides, "two-sided-long-edge", sizeof(ippo->sides));
  }

  if ((value = get_option("Collate", num_options, options)) != NULL &&
    (!strcasecmp(value, "true") || !strcasecmp(value, "on") || !strcasecmp(value, "yes")))
  {
    ippo->multiple_document_handling = CF_FILTER_HANDLING_COLLATED_COPIES;
  }

  if (get_option("fitplot", num_options, options))
    ippo->print_scaling = CF_FILTER_SCALING_FIT;

  if (get_option("fill", num_options, options))
    ippo->print_scaling = CF_FILTER_SCALING_FILL;

  if ((value = get_option("mirror", num_options, options)) != NULL)
  {
    if (!strcasecmp(value, "true") || !strcasecmp(value, "on") || 
        !strcasecmp(value, "yes"))
    {
      ippo->mirror = true;
    }
  }

  if ((value = get_option("page-border", num_options, options)) != NULL)
    cupsCopyString(ippo->page_border, value, sizeof(ippo->page_border));

  if ((value = get_option("page-top", num_options, options)) != NULL)
    ippo->page_top = atoi(value);
  if ((value = get_option("page-left", num_options, options)) != NULL)
    ippo->page_left = atoi(value);
  if ((value = get_option("page-right", num_options, options)) != NULL)
    ippo->page_right = atoi(value);
  if ((value = get_option("page-bottom", num_options, options)) != NULL)
    ippo->page_bottom = atoi(value);

  if ((value = get_option("page-label", num_options, options)) != NULL)
    cupsCopyString(ippo->page_label, value, sizeof(ippo->page_label));

  if (((value = get_option("copies", num_options, options)) != NULL ||
       (value = get_option("Copies", num_options, options)) != NULL ||
       (value = get_option("num-copies", num_options, options)) != NULL ||
       (value = get_option("NumCopies", num_options, options)) != NULL) && 
	(intvalue = atoi(value)) >= 1 && intvalue <= 999)
    ippo->copies = intvalue;

  if ((value = get_option("page-set", num_options, options)) != NULL)
  {
    if (!strcasecmp(value, "odd"))
      ippo->page_set = CF_FILTER_PAGESET_ODD;
    else if (!strcasecmp(value, "even"))
      ippo->page_set = CF_FILTER_PAGESET_EVEN;
  }

  if ((value = get_option("force-front-side", num_options, options)) != NULL)
  {
    const char	*ptr;			// Pointer into value

    ptr = value;
    while (ptr && *ptr && isdigit(*ptr & 255) && ippo->num_force_front_side < (sizeof(ippo->force_front_side) / sizeof(ippo->force_front_side[0])))
    {
      ippo->force_front_side[ippo->num_force_front_side ++] = (int)strtol(ptr, (char **)&ptr, 10);

      if (ptr && *ptr == ',')
        ptr ++;
    }
  }

  if ((value = get_option("image-orientation", num_options, options)) != NULL && (intvalue = atoi(value)) >= CF_FILTER_ORIENT_PORTRAIT && intvalue <= CF_FILTER_ORIENT_NONE)
    ippo->image_orientation = (cf_filter_orient_t)intvalue;

  if ((value = get_option("imposition-template", num_options, options)) != NULL ||
      (value = get_option("booklet", num_options, options)) != NULL)
  {
    if(strcasecmp(value, "yes") ||
       strcasecmp(value, "true") ||
       strcasecmp(value, "booklet")) 
    {
      cupsCopyString(ippo->imposition_template, "booklet", sizeof(ippo->imposition_template));
    }
    else
    {
      fprintf(stderr, "cfFilterPDFToPDF: Unsupported booklet value %s, using booklet off", value);
    }
  }

  if ((value = get_option("job-error-sheet", num_options, options)) != NULL)
  {
    // Parse job-error-sheet collection value...
    num_col = cupsParseOptions2(value, /*end*/NULL, 0, &col);

    if ((value = cupsGetOption("job-error-sheet-when", num_col, col)) != NULL)
    {
      if (!strcmp(value, "always"))
        ippo->job_error_sheet.report = CF_FILTER_ERROR_REPORT_ALWAYS;
      else if (!strcmp(value, "on-error"))
        ippo->job_error_sheet.report = CF_FILTER_ERROR_REPORT_ON_ERROR;
    }

    cupsFreeOptions(num_col, col);
  }

  if ((value = get_option("job-name", num_options, options)) != NULL)
    cupsCopyString(ippo->job_name, value, sizeof(ippo->job_name));

  if ((value = get_option("job-originating-user-name", num_options, options)) != NULL)
    cupsCopyString(ippo->job_originating_user_name, value, sizeof(ippo->job_originating_user_name));

  if ((value = get_option("job-pages-per-set", num_options, options)) != NULL && (intvalue = atoi(value)) >= 1)
    ippo->job_pages_per_set = intvalue;

  if ((value = get_option("job-sheet-message", num_options, options)) != NULL)
    cupsCopyString(ippo->job_sheet_message, value, sizeof(ippo->job_sheet_message));

  if ((value = get_option("job-sheets-col", num_options, options)) != NULL)
  {
    // Parse "job-sheets-col" collection value...
    num_col = cupsParseOptions2(value, /*end*/NULL, 0, &col);

    if ((value = cupsGetOption("media-col", num_col, col)) == NULL)
      value = cupsGetOption("media", num_col, col);

    if (value)
      parse_media(value, &ippo->job_sheets_media);

    if ((value = get_option("job-sheets", num_col, col)) == NULL)
      value = "standard";

    cupsCopyString(ippo->job_sheets, value, sizeof(ippo->job_sheets));
    cupsFreeOptions(num_col, col);
  }
  else if ((value = get_option("job-sheets", num_options, options)) != NULL)
  {
    cupsCopyString(ippo->job_sheets, value, sizeof(ippo->job_sheets));
  }

  if ((value = get_option("multiple-document-handling", num_options, options)) != NULL)
  {
    static const char * const handlings[] =
    {					// "multiple-document-handling" values
      "separate-documents-collated-copies",
      "separate-documents-uncollated-copies",
      "single-document",
      "single-document-new-sheet"
    };

    for (i = 0; i < (sizeof(handlings) / sizeof(handlings[0])); i ++)
    {
      if (!strcmp(value, handlings[i]))
      {
        ippo->multiple_document_handling = (cf_filter_handling_t)i;
        break;
      }
    }
  }

  if ((value = get_option("number-up", num_options, options)) != NULL && (intvalue = atoi(value)) >= 1)
    ippo->number_up = intvalue;

  if ((value = get_option("orientation-requested", num_options, options)) != NULL &&
    (intvalue = atoi(value), intvalue >= CF_FILTER_ORIENT_PORTRAIT && intvalue <= CF_FILTER_ORIENT_NONE))
  {
    ippo->orientation_requested = (cf_filter_orient_t)intvalue;
  }

  if ((value = get_option("output-bin", num_options, options)) != NULL)
    cupsCopyString(ippo->output_bin, value, sizeof(ippo->output_bin));

  if ((value = get_option("page-delivery", num_options, options)) != NULL)
  {
    static const char * const deliveries[] =
    {					// "page-delivery" values
      "same-order-face-down",
      "same-order-face-up",
      "reverse-order-face-down",
      "reverse-order-face-up"
    };

    for (i = 0; i < (sizeof(deliveries) / sizeof(deliveries[0])); i ++)
    {
      if (!strcmp(value, deliveries[i]))
      {
        ippo->page_delivery = (cf_filter_delivery_t)i;
        break;
      }
    }
  }

  if ((value = get_option("page-ranges", num_options, options)) != NULL)
  {
    // Parse comma-delimited page ranges...
    const char	*ptr;			// Pointer into value
    int		first, last;		// First and last page

    ptr = value;
    while (ptr && *ptr && isdigit(*ptr & 255))
    {
      first = (int)strtol(ptr, (char **)&ptr, 10);

      if (ptr && *ptr == '-')
        last = (int)strtol(ptr + 1, (char **)&ptr, 10);
      else
        last = first;

      if (ippo->num_page_ranges < (sizeof(ippo->page_ranges) / sizeof(ippo->page_ranges[0])))
      {
        ippo->page_ranges[ippo->num_page_ranges].lower = first;
        ippo->page_ranges[ippo->num_page_ranges].upper = last;
        ippo->num_page_ranges ++;
      }

      if (ptr && *ptr == ',')
        ptr ++;
    }
  }

  if ((value = get_option("print-color-mode", num_options, options)) != NULL)
    cupsCopyString(ippo->print_color_mode, value, sizeof(ippo->print_color_mode));

  if ((value = get_option("print-content-optimize", num_options, options)) != NULL)
    cupsCopyString(ippo->print_content_optimize, value, sizeof(ippo->print_content_optimize));

  if ((value = get_option("print-quality", num_options, options)) != NULL && (intvalue = atoi(value)) >= IPP_QUALITY_DRAFT && intvalue <= IPP_QUALITY_HIGH)
    ippo->print_quality = (ipp_quality_t)intvalue;

  if ((value = get_option("print-rendering-intent", num_options, options)) != NULL)
    cupsCopyString(ippo->print_rendering_intent, value, sizeof(ippo->print_rendering_intent));

  if ((value = get_option("print-scaling", num_options, options)) != NULL)
  {
    static const char * const scalings[] =
    {					// "print-scaling" values
      "auto",
      "auto-fit",
      "fill",
      "fit",
      "none"
    };

    for (i = 0; i < (sizeof(scalings) / sizeof(scalings[0])); i ++)
    {
      if (!strcmp(value, scalings[i]))
      {
        ippo->print_scaling = (cf_filter_septype_t)i;
        break;
      }
    }
  }

  if ((value = get_option("printer-resolution", num_options, options)) != NULL)
  {
    int	xdpi, ydpi;			// X/Y resolution values

    if (sscanf(value, "%dx%ddpi", &xdpi, &ydpi) != 2)
    {
      if (sscanf(value, "%ddpi", &xdpi) == 1)
        ydpi = xdpi;
      else
        xdpi = ydpi = 0;
    }

    if (xdpi > 0 && ydpi > 0)
    {
      ippo->printer_resolution[0] = xdpi;
      ippo->printer_resolution[1] = ydpi;
    }
  }

  if ((value = get_option("separator-sheets", num_options, options)) != NULL)
  {
    // Parse separator-sheets collection value...
    num_col = cupsParseOptions2(value, /*end*/NULL, 0, &col);

    if ((value = cupsGetOption("media-col", num_col, col)) == NULL)
      value = cupsGetOption("media", num_col, col);

    if (value)
      parse_media(value, &ippo->separator_media);

    if ((value = get_option("separator-sheets-type", num_col, col)) != NULL)
    {
      static const char * const types[] =
      {					// "separator-sheets-type" values
	"none",
	"slip-sheets",
	"start-sheet",
	"end-sheet",
	"both-sheets"
      };

      for (i = 0; i < (sizeof(types) / sizeof(types[0])); i ++)
      {
	if (!strcmp(value, types[i]))
	{
	  ippo->separator_type = (cf_filter_septype_t)i;
	  break;
	}
      }
    }

    cupsFreeOptions(num_col, col);
  }

  if ((value = get_option("sides", num_options, options)) != NULL)
    cupsCopyString(ippo->sides, value, sizeof(ippo->sides));

  if ((value = get_option("x-image-position", num_options, options)) != NULL)
  {
    static const char * const positions[] =
    {					// "x-image-position" values
      "none",
      "left",
      "center",
      "right"
    };

    for (i = 0; i < (sizeof(positions) / sizeof(positions[0])); i ++)
    {
      if (!strcmp(value, positions[i]))
      {
	ippo->x_image_position = (cf_filter_imgpos_t)i;
	break;
      }
    }
  }

  if ((value = get_option("x-image-shift", num_options, options)) != NULL)
    ippo->x_side1_image_shift = ippo->x_side2_image_shift = atoi(value);

  if ((value = get_option("x-side1-image-shift", num_options, options)) != NULL)
    ippo->x_side1_image_shift = atoi(value);

  if ((value = get_option("x-side2-image-shift", num_options, options)) != NULL)
    ippo->x_side2_image_shift = atoi(value);

  if ((value = get_option("y-image-position", num_options, options)) != NULL)
  {
    static const char * const positions[] =
    {					// "y-image-position" values
      "none",
      "bottom",
      "center",
      "top"
    };

    for (i = 0; i < (sizeof(positions) / sizeof(positions[0])); i ++)
    {
      if (!strcmp(value, positions[i]))
      {
	ippo->y_image_position = (cf_filter_imgpos_t)i;
	break;
      }
    }
  }

  if ((value = get_option("y-image-shift", num_options, options)) != NULL)
    ippo->y_side1_image_shift = ippo->y_side2_image_shift = atoi(value);

  if ((value = get_option("y-side1-image-shift", num_options, options)) != NULL)
    ippo->y_side1_image_shift = atoi(value);

  if ((value = get_option("y-side2-image-shift", num_options, options)) != NULL)
    ippo->y_side2_image_shift = atoi(value);

  if ((value = get_option("overrides", num_options, options)) != NULL && *value == '{')
  {
    // Parse "overrides" collection value(s)...
    cf_filter_override_t override;		// overrides value

    ippo->overrides = cupsArrayNew1((cups_array_cb_t)compare_overrides, NULL, NULL, 0, (cups_acopy_cb_t)copy_override, (cups_afree_cb_t)free);

    for (nextcol = value; nextcol && *nextcol;)
    {
      // Get the next collection value
      if (*nextcol == ',')
        nextcol ++;

      num_col = cupsParseOptions2(nextcol, &nextcol, 0, &col);

      memset(&override, 0, sizeof(override));

      if ((value = cupsGetOption("document-numbers", num_col, col)) != NULL)
      {
        switch (sscanf(value, "%d-%d", &override.first_document, &override.last_document))
        {
          case 1 :
              override.last_document = override.first_document;
              break;

          case 2 :
              break;

	  default :
	      override.first_document = 0;
	      override.last_document  = 0;
	      break;
        }
      }

      if ((value = cupsGetOption("page-numbers", num_col, col)) != NULL)
      {
        switch (sscanf(value, "%d-%d", &override.first_page, &override.last_page))
        {
          case 1 :
              override.last_page = override.first_page;
              break;

          case 2 :
              break;

	  default :
	      override.first_page = 0;
	      override.last_page  = 0;
	      break;
        }
      }

      if ((value = cupsGetOption("media", num_col, col)) == NULL)
        value = cupsGetOption("media-col", num_col, col);
      if (value)
        parse_media(value, &override.media);

      if ((value = cupsGetOption("orientation-requests", num_col, col)) != NULL && (intvalue = atoi(value)) >= CF_FILTER_ORIENT_PORTRAIT && intvalue <= CF_FILTER_ORIENT_NONE)
        override.orientation_requested = (cf_filter_orient_t)intvalue;

      cupsArrayAdd(ippo->overrides, &override);
    }
  }

  // Return the final IPP options...
  return (ippo);
}


//
// 'compare_overrides()' - Compare two "overrides" values...
//

static int					// O - Result of comparison
compare_overrides(cf_filter_override_t *a,	// I - First override
                  cf_filter_override_t *b)	// I - Second override
{
  if (a->first_document < b->first_document)
    return (-1);
  else if (a->first_document > b->first_document)
    return (1);
  else if (a->last_document < b->last_document)
    return (-1);
  else if (a->last_document > b->last_document)
    return (1);
  else if (a->first_page < b->first_page)
    return (-1);
  else if (a->first_page > b->first_page)
    return (1);
  else if (a->last_page < b->last_page)
    return (-1);
  else if (a->last_page > b->last_page)
    return (1);
  else
    return (0);
}


//
// 'copy_override()' - Copy an "overrides" value.
//

static cf_filter_override_t *		// O - New "overrides" value
copy_override(cf_filter_override_t *ov)	// I - "overrides" value
{
  cf_filter_override_t	*nov;		// New "overrides" value


  if ((nov = (cf_filter_override_t *)malloc(sizeof(cf_filter_override_t))) != NULL)
    memcpy(nov, ov, sizeof(cf_filter_override_t));

  return (nov);
}


//
// 'get_option()' - Get the value of an option from the command-line or environment.
//

static const char *			// O - Value or `NULL` if not set
get_option(const char    *name,		// I - Attribute name
           size_t        num_options,	// I - Number of command-line options
           cups_option_t *options)	// I - Command-line options
{
  char		temp[1024],		// Temporary environment variable name
		*ptr;			// Pointer into temporary name
  const char	*value;			// Value


  if ((value = cupsGetOption(name, num_options, options)) == NULL)
  {
    // Try finding "IPP_NAME" in the environment...
    snprintf(temp, sizeof(temp), "IPP_%s", name);
    for (ptr = temp + 4; *ptr; ptr ++)
    {
      if (*ptr == '-')
        *ptr = '_';
      else
	*ptr = (char)toupper(*ptr);
    }

    if ((value = getenv(temp)) == NULL)
    {
      // Nope, try "IPP_NAME_DEFAULT" in the environment...
      cupsConcatString(temp, "_DEFAULT", sizeof(temp));
      value = getenv(temp);
    }
  }

  return (value);
}


//
// 'parse_media()' - Parse a media/media-col value.
//

static bool				// O - `true` on success, `false` on error
parse_media(const char   *value,	// I - "media" or "media-col" value
            cups_media_t *media)	// O - Media value
{
  bool		margins_set = false,	// Have margins been set?
		ret = true;		// Return value
  pwg_media_t	*pwg = NULL;		// PWG media values


  // Initialize media
  memset(media, 0, sizeof(cups_media_t));

  if (*value == '{')
  {
    // Parse a "media-col" value...
    size_t	num_col;		// Number of "media-col" values
    cups_option_t *col;			// "media-col" values
    const char	*bottom_margin,		// "media-bottom-margin" value
  		*color,			// "media-color" value
		*left_margin,		// "media-left-margin" value
		*right_margin,		// "media-right-margin" value
		*size_col,		// "media-size" value
		*size_name,		// "media-size-name" value
		*source,		// "media-source" value
		*top_margin,		// "media-top-margin" value
		*type;			// "media-type" value

    num_col = cupsParseOptions2(value, /*end*/NULL, 0, &col);
    if ((size_name = cupsGetOption("media-size-name", num_col, col)) != NULL)
    {
      if ((pwg = pwgMediaForPWG(size_name)) != NULL)
        cupsCopyString(media->media, size_name, sizeof(media->media));
      else
        ret = false;
    }
    else if ((size_col = cupsGetOption("media-size", num_col, col)) != NULL)
    {
      size_t		num_size;	// Number of collection values
      cups_option_t	*size;		// Collection values
      const char	*x_dim,		// x-dimension
			*y_dim;		// y-dimension

      num_size = cupsParseOptions2(size_col, /*end*/NULL, 0, &size);
      if ((x_dim = cupsGetOption("x-dimension", num_size, size)) != NULL && (y_dim = cupsGetOption("y-dimension", num_size, size)) != NULL && (pwg = pwgMediaForSize(atoi(x_dim), atoi(y_dim))) != NULL)
        cupsCopyString(media->media, pwg->pwg, sizeof(media->media));
      else
        ret = false;

      cupsFreeOptions(num_size, size);
    }

    if (pwg)
    {
      // Copy width/length...
      media->width  = pwg->width;
      media->length = pwg->length;
    }

    // Get other media-col values...
    if ((bottom_margin = cupsGetOption("media-bottom-margin", num_col, col)) != NULL)
      media->bottom = atoi(bottom_margin);
    if ((left_margin = cupsGetOption("media-left-margin", num_col, col)) != NULL)
      media->left = atoi(left_margin);
    if ((right_margin = cupsGetOption("media-right-margin", num_col, col)) != NULL)
      media->right = atoi(right_margin);
    if ((top_margin = cupsGetOption("media-top-margin", num_col, col)) != NULL)
      media->top = atoi(top_margin);
    margins_set = bottom_margin != NULL || left_margin != NULL || right_margin != NULL || top_margin != NULL;

    if ((color = cupsGetOption("media-color", num_col, col)) != NULL)
      cupsCopyString(media->color, color, sizeof(media->color));
    if ((source = cupsGetOption("media-source", num_col, col)) != NULL)
      cupsCopyString(media->source, source, sizeof(media->source));
    if ((type = cupsGetOption("media-type", num_col, col)) != NULL)
      cupsCopyString(media->type, type, sizeof(media->type));

    // Free the "media-col" values...
    cupsFreeOptions(num_col, col);
  }
  else if ((pwg = pwgMediaForPWG(value)) != NULL)
  {
    // Use "media" size name...
    cupsCopyString(media->media, value, sizeof(media->media));
    media->width  = pwg->width;
    media->length = pwg->length;
  }
  else
  {
    // No media... :(
    ret = false;
  }

  // Set some defaults...
  if (!media->color[0])
    cupsCopyString(media->color, DEFAULT_COLOR, sizeof(media->color));

  if (!media->media[0])
  {
    pwg = pwgMediaForPWG(DEFAULT_SIZE_NAME);
    cupsCopyString(media->media, DEFAULT_SIZE_NAME, sizeof(media->media));
    media->width  = pwg->width;
    media->length = pwg->length;
  }

  if (!margins_set)
  {
    if (!strcmp(media->media, "iso_a6_105x148mm") || !strcmp(media->media, "na_index-4x6_4x6in") || !strcmp(media->media, "na_5x7_5x7in") || !strcmp(media->media, "na_govt-letter_8x10in") || strstr(media->media, "photo") != NULL)
    {
      // Standard photo sizes so use borderless margins...
      media->bottom = media->top = 0;
      media->left = media->right = 0;
    }
    else
    {
      // Normal media sizes so use default margins...
      media->bottom = media->top = DEFAULT_MARGIN_BOTTOM_TOP;
      media->left = media->right = DEFAULT_MARGIN_LEFT_RIGHT;
    }
  }

  if (!media->source[0])
    cupsCopyString(media->source, DEFAULT_SOURCE, sizeof(media->source));

  if (!media->type[0])
  {
    if (media->bottom == 0 && media->left == 0 && media->right == 0 && media->top == 0)
    {
      // Borderless so use 'photographic' type...
      cupsCopyString(media->type, "photographic", sizeof(media->type));
    }
    else
    {
      // Otherwise default type...
      cupsCopyString(media->type, DEFAULT_TYPE, sizeof(media->type));
    }
  }

  // Return...
  return (ret);
}

