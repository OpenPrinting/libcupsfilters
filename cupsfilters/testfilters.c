#include <config.h>
#include <ctype.h>
#include <errno.h>
#include <cupsfilters/filter.h>
#include <cups/cups.h>
#include <cups/array.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

/*
 * 'remove_white_space()' - Remove white spaces from beginning and end of a string
 */

char* 
remove_white_space(
    char* str)
{
  char *end;
  while(isspace((unsigned char)*str)) str++;
  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;
  
  // Write new null terminator character
  end[1] = '\0';

  return str;
}

/*
 * 'create_media_size_range()' - Create a ranged media-size value.
 */

static ipp_t *				/* O - media-col collection */
create_media_size_range(int min_width,	/* I - Minimum x-dimension in 2540ths */
			int max_width,	/* I - Maximum x-dimension in 2540ths */
			int min_length,	/* I - Minimum x-dimension in 2540ths */
			int max_length)	/* I - Maximum y-dimension in 2540ths */
{
  ipp_t	*media_size = ippNew();		/* media-size value */


  ippAddRange(media_size, IPP_TAG_ZERO, "x-dimension", min_width, max_width);
  ippAddRange(media_size, IPP_TAG_ZERO, "y-dimension", min_length, max_length);

  return (media_size);
}


static ipp_t *				/* O - media-col collection */
create_media_col(const char *media,	/* I - Media name */
		 const char *source,	/* I - Media source, if any */
		 const char *type,	/* I - Media type, if any */
		 ipp_t      *media_size,/* I - media-size collection value */
		 int        bottom,	/* I - Bottom margin in 2540ths */
		 int        left,	/* I - Left margin in 2540ths */
		 int        right,	/* I - Right margin in 2540ths */
		 int        top)	/* I - Top margin in 2540ths */
{
  ipp_t		*media_col = ippNew();	/* media-col value */
  char		media_key[256];		/* media-key value */
  const char	*media_key_suffix = "";	/* media-key suffix */


  if (bottom == 0 && left == 0 && right == 0 && top == 0)
    media_key_suffix = "_borderless";

  if (media)
  {
    if (type && source)
      snprintf(media_key, sizeof(media_key), "%s_%s_%s%s", media, source, type, media_key_suffix);
    else if (type)
      snprintf(media_key, sizeof(media_key), "%s__%s%s", media, type, media_key_suffix);
    else if (source)
      snprintf(media_key, sizeof(media_key), "%s_%s%s", media, source, media_key_suffix);
    else
      snprintf(media_key, sizeof(media_key), "%s%s", media, media_key_suffix);

    ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-key", NULL, media_key);
  }
  ippAddCollection(media_col, IPP_TAG_PRINTER, "media-size", media_size);
  if (media)
    ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-size-name", NULL, media);
  if (bottom >= 0)
    ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin", bottom);
  if (left >= 0)
    ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin", left);
  if (right >= 0)
    ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin", right);
  if (top >= 0)
    ippAddInteger(media_col, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin", top);
  if (source)
    ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-source", NULL, source);
  if (type)
    ippAddString(media_col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-type", NULL, type);

  ippDelete(media_size);

  return (media_col);
}


/*
 * 'create_media_size()' - Create a media-size value.
 */

static ipp_t *				/* O - media-col collection */
create_media_size(int width,		/* I - x-dimension in 2540ths */
		  int length)		/* I - y-dimension in 2540ths */
{
  ipp_t	*media_size = ippNew();		/* media-size value */


  ippAddInteger(media_size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "x-dimension", width);
  ippAddInteger(media_size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "y-dimension", length);

  return (media_size);
}

/*
 * 'test_wrapper()' - Utilizes libcupsfilters API for running a particular test
 *
 */

int					// O - Exit status
test_wrapper(
     int  num_clargs,				// I - Number of command-line args
     char *clargs[],			// I - Command-line arguments
     void *parameters,                  // I - Filter function parameters
     int *JobCanceled,			// I - Var set to 1 when job canceled
     ipp_t* emulated_ipp,
     char* inputMIME,
     char* outputMIME,
     char* inputFile,
     char* outputFile)                  
{
  int	        inputfd;		// Print file descriptor
  int 		outputfd;		// File Descriptor for Output File
  int           inputseekable = 0;          // Is the input seekable (actual file
					// not stdin)?
  int		num_options = 0;	// Number of print options
  cups_option_t	*options = NULL;	// Print options
  cf_filter_data_t filter_data;
  const char    *val;
  char          buf[256];
  int           retval = 0;

  //
  // Make sure status messages are not buffered...
  //

  setbuf(stderr, NULL);

  //
  // Ignore broken pipe signals...
  //

  signal(SIGPIPE, SIG_IGN);

  //
  // Try to open the print file...
  //
    
   if ((inputfd = open(inputFile, O_RDONLY)) < 0)
    {
      if (!(*JobCanceled))
      {
        fprintf(stderr, "DEBUG: Unable to open \"%s\": %s\n", inputFile,
		strerror(errno));
	fprintf(stderr, "ERROR: Unable to open print file\n");
      }

      return (1);
    }
    
  // 
  // Try to open output file...
  //
   
   if ((outputfd = open(outputFile, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) < 0)
    {
      if (!(*JobCanceled))
      {
        fprintf(stderr, "DEBUG: Unable to open \"%s\": %s\n", outputFile,
		strerror(errno));
	fprintf(stderr, "ERROR: Unable to open Write file\n");
      }
      close(inputfd);
      
      return (1);
    }

  options = NULL;
  if (num_clargs > 5)
    num_options = cupsParseOptions(clargs[5], 0, &options);
	fprintf(stderr, "NUM Options: %d\n", num_options);
  if ((filter_data.printer = getenv("PRINTER")) == NULL)
    filter_data.printer = clargs[0];
  filter_data.job_id = num_clargs > 1 ? atoi(clargs[1]) : 0;
  filter_data.job_user = num_clargs > 2 ? clargs[2] : NULL;
  filter_data.job_title = num_clargs > 3 ? clargs[3] : NULL;
  filter_data.copies = num_clargs > 4 ? atoi(clargs[4]) : 1;
  filter_data.content_type = inputMIME;               ///Set the input MIME Type from the test
  filter_data.final_content_type = outputMIME;   /// Set the output MIME Tyoe from the test case 

  filter_data.job_attrs = NULL;        // We use command line options
  // The following two will get populated by ppdFilterLoadPPD()
  filter_data.printer_attrs = NULL;    // We use the queue's PPD file
  filter_data.header = NULL;           // CUPS Raster header of queue's PPD
  filter_data.num_options = num_options;
  

  filter_data.options = options;       // Command line options from 5th arg
  
  /* Debugging Options
  for(int option_no = 0; option_no < num_options; option_no++){
  	printf("Printing Options: %s %s\n", (filter_data.options)[option_no].name, (filter_data.options)[option_no].value);
  }
  */
  
  filter_data.back_pipe[0] = 3;        // CUPS uses file descriptor 3 for
  filter_data.back_pipe[1] = 3;        // the back channel
  filter_data.side_pipe[0] = 4;        // CUPS uses file descriptor 4 for
  filter_data.side_pipe[1] = 4;        // the side channel
  filter_data.extension = NULL;
  filter_data.logfunc = cfCUPSLogFunc;  // Logging scheme of CUPS
  filter_data.logdata = NULL;
  filter_data.iscanceledfunc = cfCUPSIsCanceledFunc; // Job-is-canceled
						     // function
  filter_data.iscanceleddata = JobCanceled;

  //
  // CUPS_FONTPATH (Usually /usr/share/cups/fonts)
  //

  if (cupsGetOption("cups-fontpath",
		    filter_data.num_options, filter_data.options) == NULL)
  {
    if ((val = getenv("CUPS_FONTPATH")) == NULL)
    {
      val = CUPS_DATADIR;
      snprintf(buf, sizeof(buf), "%s/fonts", val);
      val = buf;
    }
    if (val[0] != '\0')
      filter_data.num_options =
	cupsAddOption("cups-fontpath", val,
		      filter_data.num_options, &(filter_data.options));
  }

  //
  // Set the printer attributes to Emulated printer... 
  //
  
  filter_data.printer_attrs = emulated_ipp;
   
  //
  // Fire up the filter function (output to stdout, file descriptor 1)
  //

  retval = cfFilterUniversal(inputfd, outputfd, inputseekable, &filter_data, parameters);

  return retval;
}

/*
 * 'load_legacy_attributes()' - Load IPP attributes using the old ippserver
 *                              options.
 */


static ipp_t *				/* O - IPP attributes or `NULL` on error */
load_legacy_attributes(
    const char   *make,			/* I - Manufacturer name */
    const char   *model,		/* I - Model name */
    int          ppm,			/* I - pages-per-minute */
    int          ppm_color,		/* I - pages-per-minute-color */
    int          duplex,		/* I - Duplex support? */
    cups_array_t *docformats)		/* I - document-format-supported values */
{
  size_t		i;		/* Looping var */
  ipp_t			*attrs,		/* IPP attributes */
			*col;		/* Collection value */
  ipp_attribute_t	*attr;		/* Current attribute */
  char			device_id[1024],/* printer-device-id */
			*ptr,		/* Pointer into device ID */
			make_model[128];/* printer-make-and-model */
  const char		*format,	/* Current document format */
			*prefix;	/* Prefix for device ID */
  size_t		num_media;	/* Number of media */
  const char * const	*media;		/* List of media */
  size_t		num_ready;	/* Number of loaded media */
  const char * const	*ready;		/* List of loaded media */
  pwg_media_t		*pwg;		/* PWG media size information */
  static const char * const media_supported[] =
  {					/* media-supported values */
    "na_letter_8.5x11in",		/* Letter */
    "na_legal_8.5x14in",		/* Legal */
    "iso_a4_210x297mm",			/* A4 */
    "na_number-10_4.125x9.5in",		/* #10 Envelope */
    "iso_dl_110x220mm"			/* DL Envelope */
  };
  static const char * const media_supported_color[] =
  {					/* media-supported values */
    "na_letter_8.5x11in",		/* Letter */
    "na_legal_8.5x14in",		/* Legal */
    "iso_a4_210x297mm",			/* A4 */
    "na_number-10_4.125x9.5in",		/* #10 Envelope */
    "iso_dl_110x220mm",			/* DL Envelope */
    "na_index-3x5_3x5in",		/* Photo 3x5 */
    "oe_photo-l_3.5x5in",		/* Photo L */
    "na_index-4x6_4x6in",		/* Photo 4x6 */
    "iso_a6_105x148mm",			/* A6 */
    "na_5x7_5x7in",			/* Photo 5x7 aka 2L */
    "iso_a5_148x210mm",			/* A5 */
    "roll_min_4x1in",			/* Roll */
    "roll_max_8.5x39.6in"		/* Roll */
  };
  static const char * const media_ready[] =
  {					/* media-ready values */
    "na_letter_8.5x11in",		/* Letter */
    "na_number-10_4.125x9.5in"		/* #10 */
  };
  static const char * const media_ready_color[] =
  {					/* media-ready values */
    "na_letter_8.5x11in",		/* Letter */
    "na_index-4x6_4x6in",		/* Photo 4x6 */
    "roll_current_8.5x0in"		/* 8.5" roll */
  };
  static const char * const media_source_supported[] =
  {					/* media-source-supported values */
    "auto",
    "main",
    "manual",
    "by-pass-tray"			/* AKA multi-purpose tray */
  };
  static const char * const media_source_supported_color[] =
  {					/* media-source-supported values */
    "auto",
    "main",
    "photo",
    "roll"
  };
  static const char * const media_type_supported[] =
  {					/* media-type-supported values */
    "auto",
    "cardstock",
    "envelope",
    "labels",
    "other",
    "stationery",
    "stationery-letterhead",
    "transparency"
  };
  static const char * const media_type_supported_color[] =
  {					/* media-type-supported values */
    "auto",
    "cardstock",
    "envelope",
    "labels",
    "other",
    "stationery",
    "stationery-letterhead",
    "transparency",
    "photographic-glossy",
    "photographic-high-gloss",
    "photographic-matte",
    "photographic-satin",
    "photographic-semi-gloss"
  };
  static const int	media_bottom_margin_supported[] =
  {					/* media-bottom-margin-supported values */
    635					/* 1/4" */
  };
  static const int	media_bottom_margin_supported_color[] =
  {					/* media-bottom/top-margin-supported values */
    0,					/* Borderless */
    1168				/* 0.46" (common HP inkjet bottom margin) */
  };
  static const int	media_lr_margin_supported[] =
  {					/* media-left/right-margin-supported values */
    340,				/* 3.4mm (historical HP PCL A4 margin) */
    635					/* 1/4" */
  };
  static const int	media_lr_margin_supported_color[] =
  {					/* media-left/right-margin-supported values */
    0,					/* Borderless */
    340,				/* 3.4mm (historical HP PCL A4 margin) */
    635					/* 1/4" */
  };
  static const int	media_top_margin_supported[] =
  {					/* media-top-margin-supported values */
    635					/* 1/4" */
  };
  static const int	media_top_margin_supported_color[] =
  {					/* media-top/top-margin-supported values */
    0,					/* Borderless */
    102					/* 0.04" (common HP inkjet top margin */
  };
  static const int	orientation_requested_supported[4] =
  {					/* orientation-requested-supported values */
    IPP_ORIENT_PORTRAIT,
    IPP_ORIENT_LANDSCAPE,
    IPP_ORIENT_REVERSE_LANDSCAPE,
    IPP_ORIENT_REVERSE_PORTRAIT
  };
  static const char * const overrides_supported[] =
  {					/* overrides-supported values */
    "document-numbers",
    "media",
    "media-col",
    "orientation-requested",
    "pages"
  };
  static const char * const print_color_mode_supported[] =
  {					/* print-color-mode-supported values */
    "monochrome"
  };
  static const char * const print_color_mode_supported_color[] =
  {					/* print-color-mode-supported values */
    "auto",
    "color",
    "monochrome"
  };
  static const int	print_quality_supported[] =
  {					/* print-quality-supported values */
    IPP_QUALITY_DRAFT,
    IPP_QUALITY_NORMAL,
    IPP_QUALITY_HIGH
  };
  static const char * const printer_input_tray[] =
  {					/* printer-input-tray values */
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=-2;level=-2;status=0;name=auto",
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=250;level=100;status=0;name=main",
    "type=sheetFeedManual;mediafeed=0;mediaxfeed=0;maxcapacity=1;level=-2;status=0;name=manual",
    "type=sheetFeedAutoNonRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=25;level=-2;status=0;name=by-pass-tray"
  };
  static const char * const printer_input_tray_color[] =
  {					/* printer-input-tray values */
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=-2;level=-2;status=0;name=auto",
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=250;level=-2;status=0;name=main",
    "type=sheetFeedAutoRemovableTray;mediafeed=0;mediaxfeed=0;maxcapacity=25;level=-2;status=0;name=photo",
    "type=continuousRoll;mediafeed=0;mediaxfeed=0;maxcapacity=100;level=-2;status=0;name=roll"
  };
  static const char * const printer_supply[] =
  {					/* printer-supply values */
    "index=1;class=receptacleThatIsFilled;type=wasteToner;unit=percent;"
        "maxcapacity=100;level=25;colorantname=unknown;",
    "index=2;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=75;colorantname=black;"
  };
  static const char * const printer_supply_color[] =
  {					/* printer-supply values */
    "index=1;class=receptacleThatIsFilled;type=wasteInk;unit=percent;"
        "maxcapacity=100;level=25;colorantname=unknown;",
    "index=2;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=75;colorantname=black;",
    "index=3;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=50;colorantname=cyan;",
    "index=4;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=33;colorantname=magenta;",
    "index=5;class=supplyThatIsConsumed;type=ink;unit=percent;"
        "maxcapacity=100;level=67;colorantname=yellow;"
  };
  static const char * const printer_supply_description[] =
  {					/* printer-supply-description values */
    "Toner Waste Tank",
    "Black Toner"
  };
  static const char * const printer_supply_description_color[] =
  {					/* printer-supply-description values */
    "Ink Waste Tank",
    "Black Ink",
    "Cyan Ink",
    "Magenta Ink",
    "Yellow Ink"
  };
  static const int	pwg_raster_document_resolution_supported[] =
  {
    300,
    600
  };
  static const char * const pwg_raster_document_type_supported[] =
  {
    "black_1",
    "sgray_8"
  };
  static const char * const pwg_raster_document_type_supported_color[] =
  {
    "black_1",
    "sgray_8",
    "srgb_8",
    "srgb_16"
  };
  static const char * const sides_supported[] =
  {					/* sides-supported values */
    "one-sided",
    "two-sided-long-edge",
    "two-sided-short-edge"
  };
  static const char * const urf_supported[] =
  {					/* urf-supported values */
    "CP1",
    "IS1-4-5-19",
    "MT1-2-3-4-5-6",
    "RS600",
    "V1.4",
    "W8"
  };
  static const char * const urf_supported_color[] =
  {					/* urf-supported values */
    "CP1",
    "IS1-4-5-7-19",
    "MT1-2-3-4-5-6-8-9-10-11-12-13",
    "RS600",
    "SRGB24",
    "V1.4",
    "W8"
  };
  static const char * const urf_supported_color_duplex[] =
  {					/* urf-supported values */
    "CP1",
    "IS1-4-5-7-19",
    "MT1-2-3-4-5-6-8-9-10-11-12-13",
    "RS600",
    "SRGB24",
    "V1.4",
    "W8",
    "DM3"
  };
  static const char * const urf_supported_duplex[] =
  {					/* urf-supported values */
    "CP1",
    "IS1-4-5-19",
    "MT1-2-3-4-5-6",
    "RS600",
    "V1.4",
    "W8",
    "DM1"
  };


  attrs = ippNew();

  if (ppm_color > 0)
  {
    num_media = (int)(sizeof(media_supported_color) / sizeof(media_supported_color[0]));
    media     = media_supported_color;
    num_ready = (int)(sizeof(media_ready_color) / sizeof(media_ready_color[0]));
    ready     = media_ready_color;
  }
  else
  {
    num_media = (int)(sizeof(media_supported) / sizeof(media_supported[0]));
    media     = media_supported;
    num_ready = (int)(sizeof(media_ready) / sizeof(media_ready[0]));
    ready     = media_ready;
  }

  /* color-supported */
  ippAddBoolean(attrs, IPP_TAG_PRINTER, "color-supported", ppm_color > 0);

  /* copies-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default", 1);

  /* copies-supported */
  ippAddRange(attrs, IPP_TAG_PRINTER, "copies-supported", 1, (cupsArrayFind(docformats, (void *)"application/pdf") != NULL || cupsArrayFind(docformats, (void *)"image/jpeg") != NULL) ? 999 : 1);

  /* document-password-supported */
  if (cupsArrayFind(docformats, (void *)"application/pdf"))
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "document-password-supported", 1023);

  /* finishing-template-supported */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template-supported", NULL, "none");

  /* finishings-col-database */
  col = ippNew();
  ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template", NULL, "none");
  ippAddCollection(attrs, IPP_TAG_PRINTER, "finishings-col-database", col);
  ippDelete(col);

  /* finishings-col-default */
  col = ippNew();
  ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template", NULL, "none");
  ippAddCollection(attrs, IPP_TAG_PRINTER, "finishings-col-default", col);
  ippDelete(col);

  /* finishings-col-ready */
  col = ippNew();
  ippAddString(col, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template", NULL, "none");
  ippAddCollection(attrs, IPP_TAG_PRINTER, "finishings-col-ready", col);
  ippDelete(col);

  /* finishings-col-supported */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishings-col-supported", NULL, "finishing-template");

  /* finishings-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-default", IPP_FINISHINGS_NONE);

  /* finishings-ready */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-ready", IPP_FINISHINGS_NONE);

  /* finishings-supported */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-supported", IPP_FINISHINGS_NONE);

  /* media-bottom-margin-supported */
  if (ppm_color > 0)
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin-supported", (int)(sizeof(media_bottom_margin_supported) / sizeof(media_bottom_margin_supported[0])), media_bottom_margin_supported);
  else
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin-supported", (int)(sizeof(media_bottom_margin_supported_color) / sizeof(media_bottom_margin_supported_color[0])), media_bottom_margin_supported_color);

  /* media-col-database and media-col-default */
  for (i = 0, attr = NULL; i < num_media; i ++)
  {
    int		bottom, left,		/* media-xxx-margins */
		right, top;
    const char	*source;		/* media-source, if any */

    pwg = pwgMediaForPWG(media[i]);

    if (pwg->width < 21000 && pwg->length < 21000)
    {
      source = "photo";			/* Photo size media from photo tray */
      bottom =				/* Borderless margins */
      left   =
      right  =
      top    = 0;
    }
    else if (pwg->width < 21000)
    {
      source = "by-pass-tray";		/* Envelopes from multi-purpose tray */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are standard */
      right  = media_lr_margin_supported[1];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }
    else if (pwg->width == 21000)
    {
      source = NULL;			/* A4 from any tray */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are reduced */
      right  = media_lr_margin_supported[0];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }
    else
    {
      source = NULL;			/* Other size media from any tray */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are standard */
      right  = media_lr_margin_supported[1];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }

    if (!strncmp(media[i], "roll_min_", 9) && i < (num_media - 1))
    {
      // Roll min/max range...
      pwg_media_t	*pwg2;		// Max size
      ipp_t		*media_size;	// media-size member attribute

      i ++;
      pwg2 = pwgMediaForPWG(media[i]);

      media_size = ippNew();
      ippAddRange(media_size, IPP_TAG_ZERO, "x-dimension", pwg->width, pwg2->width);
      ippAddRange(media_size, IPP_TAG_ZERO, "y-dimension", pwg->length, pwg2->length);

      col = create_media_col(NULL, source, NULL, media_size, bottom, left, right, top);
    }
    else
    {
      // Sheet size
      col = create_media_col(media[i], source, NULL, create_media_size(pwg->width, pwg->length), bottom, left, right, top);
    }

    if (attr)
      ippSetCollection(attrs, &attr, ippGetCount(attr), col);
    else
      attr = ippAddCollection(attrs, IPP_TAG_PRINTER, "media-col-database", col);

    ippDelete(col);
  }

  /* media-col-default */
  pwg = pwgMediaForPWG(ready[0]);

  if (pwg->width == 21000)
    col = create_media_col(ready[0], "main", "stationery", create_media_size(pwg->width, pwg->length), ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0], media_lr_margin_supported[0], media_lr_margin_supported[0], ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0]);
  else
    col = create_media_col(ready[0], "main", "stationery", create_media_size(pwg->width, pwg->length), ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0], media_lr_margin_supported[1], media_lr_margin_supported[1], ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0]);

  ippAddCollection(attrs, IPP_TAG_PRINTER, "media-col-default", col);

  ippDelete(col);

  /* media-col-ready */
  attr = ippAddCollections(attrs, IPP_TAG_PRINTER, "media-col-ready", num_ready, NULL);
  for (i = 0; i < num_ready; i ++)
  {
    int		bottom, left,		/* media-xxx-margins */
		right, top;
    const char	*source,		/* media-source */
		*type;			/* media-type */

    pwg = pwgMediaForPWG(ready[i]);

    if (pwg->width < 21000 && pwg->length < 21000)
    {
      source = "photo";			/* Photo size media from photo tray */
      type   = "photographic-glossy";	/* Glossy photo paper */
      bottom =				/* Borderless margins */
      left   =
      right  =
      top    = 0;
    }
    else if (pwg->width < 21000)
    {
      source = "by-pass-tray";		/* Envelopes from multi-purpose tray */
      type   = "envelope";		/* Envelope */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are standard */
      right  = media_lr_margin_supported[1];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }
    else if (pwg->width == 21000)
    {
      source = "main";			/* A4 from main tray */
      type   = "stationery";		/* Plain paper */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are reduced */
      right  = media_lr_margin_supported[0];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }
    else
    {
      source = "main";			/* A4 from main tray */
      type   = "stationery";		/* Plain paper */
      bottom = ppm_color > 0 ? media_bottom_margin_supported_color[1] : media_bottom_margin_supported[0];
      left   =				/* Left/right margins are standard */
      right  = media_lr_margin_supported[1];
      top    = ppm_color > 0 ? media_top_margin_supported_color[1] : media_top_margin_supported[0];
    }

    col = create_media_col(ready[i], source, type, create_media_size(pwg->width, pwg->length), bottom, left, right, top);
    ippSetCollection(attrs, &attr, i, col);
    ippDelete(col);
  }

  /* media-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-default", NULL, media[0]);

  /* media-left/right-margin-supported */
  if (ppm_color > 0)
  {
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin-supported", (int)(sizeof(media_lr_margin_supported_color) / sizeof(media_lr_margin_supported_color[0])), media_lr_margin_supported_color);
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin-supported", (int)(sizeof(media_lr_margin_supported_color) / sizeof(media_lr_margin_supported_color[0])), media_lr_margin_supported_color);
  }
  else
  {
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin-supported", (int)(sizeof(media_lr_margin_supported) / sizeof(media_lr_margin_supported[0])), media_lr_margin_supported);
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin-supported", (int)(sizeof(media_lr_margin_supported) / sizeof(media_lr_margin_supported[0])), media_lr_margin_supported);
  }

  /* media-ready */
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", num_ready, NULL, ready);

  /* media-supported */
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-supported", num_media, NULL, media);

  /* media-size-supported */
  for (i = 0, attr = NULL; i < num_media; i ++)
  {
    pwg = pwgMediaForPWG(media[i]);

    if (!strncmp(media[i], "roll_min_", 9) && i < (num_media - 1))
    {
      // Roll min/max range...
      pwg_media_t	*pwg2;		// Max size

      i ++;
      pwg2 = pwgMediaForPWG(media[i]);

      col = create_media_size_range(pwg->width, pwg2->width, pwg->length, pwg2->length);
    }
    else
    {
      // Sheet size...
      col = create_media_size(pwg->width, pwg->length);
    }

    if (attr)
      ippSetCollection(attrs, &attr, ippGetCount(attr), col);
    else
      attr = ippAddCollection(attrs, IPP_TAG_PRINTER, "media-size-supported", col);

    ippDelete(col);
  }

  /* media-source-supported */
  if (ppm_color > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-source-supported", (int)(sizeof(media_source_supported_color) / sizeof(media_source_supported_color[0])), NULL, media_source_supported_color);
  else
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-source-supported", (int)(sizeof(media_source_supported) / sizeof(media_source_supported[0])), NULL, media_source_supported);

  /* media-top-margin-supported */
  if (ppm_color > 0)
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin-supported", (int)(sizeof(media_top_margin_supported) / sizeof(media_top_margin_supported[0])), media_top_margin_supported);
  else
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin-supported", (int)(sizeof(media_top_margin_supported_color) / sizeof(media_top_margin_supported_color[0])), media_top_margin_supported_color);

  /* media-type-supported */
  if (ppm_color > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-type-supported", (int)(sizeof(media_type_supported_color) / sizeof(media_type_supported_color[0])), NULL, media_type_supported_color);
  else
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-type-supported", (int)(sizeof(media_type_supported) / sizeof(media_type_supported[0])), NULL, media_type_supported);

  /* orientation-requested-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-default", IPP_ORIENT_PORTRAIT);

  /* orientation-requested-supported */
  if (cupsArrayFind(docformats, (void *)"application/pdf") || cupsArrayFind(docformats, (void *)"image/jpeg"))
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", (int)(sizeof(orientation_requested_supported) / sizeof(orientation_requested_supported[0])), orientation_requested_supported);
  else
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", IPP_ORIENT_PORTRAIT);

  /* output-bin-default */
  if (ppm_color > 0)
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, "face-up");
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-default", NULL, "face-down");

  /* output-bin-supported */
  if (ppm_color > 0)
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-supported", NULL, "face-up");
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "output-bin-supported", NULL, "face-down");

  /* overrides-supported */
  if (cupsArrayFind(docformats, (void *)"application/pdf"))
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "overrides-supported", (int)(sizeof(overrides_supported) / sizeof(overrides_supported[0])), NULL, overrides_supported);

  /* page-ranges-supported */
  ippAddBoolean(attrs, IPP_TAG_PRINTER, "page-ranges-supported", cupsArrayFind(docformats, (void *)"application/pdf") != NULL);

  /* pages-per-minute */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "pages-per-minute", ppm);

  /* pages-per-minute-color */
  if (ppm_color > 0)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "pages-per-minute-color", ppm_color);

  /* print-color-mode-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-default", NULL, ppm_color > 0 ? "auto" : "monochrome");

  /* print-color-mode-supported */
  if (ppm_color > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", (int)(sizeof(print_color_mode_supported_color) / sizeof(print_color_mode_supported_color[0])), NULL, print_color_mode_supported_color);
  else
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", (int)(sizeof(print_color_mode_supported) / sizeof(print_color_mode_supported[0])), NULL, print_color_mode_supported);

  /* print-content-optimize-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-default", NULL, "auto");

  /* print-content-optimize-supported */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-supported", NULL, "auto");

  /* print-quality-default */
  ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", IPP_QUALITY_NORMAL);

  /* print-quality-supported */
  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-supported", (int)(sizeof(print_quality_supported) / sizeof(print_quality_supported[0])), print_quality_supported);

  /* print-rendering-intent-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-rendering-intent-default", NULL, "auto");

  /* print-rendering-intent-supported */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-rendering-intent-supported", NULL, "auto");

  /* printer-device-id */
  snprintf(device_id, sizeof(device_id), "MFG:%s;MDL:%s;", make, model);
  ptr    = device_id + strlen(device_id);
  prefix = "CMD:";
  for (format = (const char *)cupsArrayFirst(docformats); format; format = (const char *)cupsArrayNext(docformats))
  {
    if (!strcasecmp(format, "application/pdf"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPDF", prefix);
    else if (!strcasecmp(format, "application/postscript"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPS", prefix);
    else if (!strcasecmp(format, "application/vnd.hp-PCL"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPCL", prefix);
    else if (!strcasecmp(format, "image/jpeg"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sJPEG", prefix);
    else if (!strcasecmp(format, "image/png"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPNG", prefix);
    else if (!strcasecmp(format, "image/pwg-raster"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sPWG", prefix);
    else if (!strcasecmp(format, "image/urf"))
      snprintf(ptr, sizeof(device_id) - (size_t)(ptr - device_id), "%sURF", prefix);
    else
      continue;

    ptr += strlen(ptr);
    prefix = ",";
  }
  if (ptr < (device_id + sizeof(device_id) - 1))
  {
    *ptr++ = ';';
    *ptr = '\0';
  }
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-device-id", NULL, device_id);

  /* printer-input-tray */
  if (ppm_color > 0)
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-input-tray", printer_input_tray_color[0], strlen(printer_input_tray_color[0]));
    for (i = 1; i < (int)(sizeof(printer_input_tray_color) / sizeof(printer_input_tray_color[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_input_tray_color[i], strlen(printer_input_tray_color[i]));
  }
  else
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-input-tray", printer_input_tray[0], strlen(printer_input_tray[0]));
    for (i = 1; i < (int)(sizeof(printer_input_tray) / sizeof(printer_input_tray[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_input_tray[i], strlen(printer_input_tray[i]));
  }

  /* printer-make-and-model */
  snprintf(make_model, sizeof(make_model), "%s %s", make, model);
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-make-and-model", NULL, make_model);

  /* printer-resolution-default */
  ippAddResolution(attrs, IPP_TAG_PRINTER, "printer-resolution-default", IPP_RES_PER_INCH, 600, 600);

  /* printer-resolution-supported */
  ippAddResolution(attrs, IPP_TAG_PRINTER, "printer-resolution-supported", IPP_RES_PER_INCH, 600, 600);

  /* printer-supply and printer-supply-description */
  if (ppm_color > 0)
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-supply", printer_supply_color[0], strlen(printer_supply_color[0]));
    for (i = 1; i < (int)(sizeof(printer_supply_color) / sizeof(printer_supply_color[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_supply_color[i], strlen(printer_supply_color[i]));

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-supply-description", (int)(sizeof(printer_supply_description_color) / sizeof(printer_supply_description_color[0])), NULL, printer_supply_description_color);
  }
  else
  {
    attr = ippAddOctetString(attrs, IPP_TAG_PRINTER, "printer-supply", printer_supply[0], strlen(printer_supply[0]));
    for (i = 1; i < (int)(sizeof(printer_supply) / sizeof(printer_supply[0])); i ++)
      ippSetOctetString(attrs, &attr, i, printer_supply[i], strlen(printer_supply[i]));

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-supply-description", (int)(sizeof(printer_supply_description) / sizeof(printer_supply_description[0])), NULL, printer_supply_description);
  }

  /* pwg-raster-document-xxx-supported */
  if (cupsArrayFind(docformats, (void *)"image/pwg-raster"))
  {
    ippAddResolutions(attrs, IPP_TAG_PRINTER, "pwg-raster-document-resolution-supported", (int)(sizeof(pwg_raster_document_resolution_supported) / sizeof(pwg_raster_document_resolution_supported[0])), IPP_RES_PER_INCH, pwg_raster_document_resolution_supported, pwg_raster_document_resolution_supported);

    if (ppm_color > 0 && duplex)
      ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-sheet-back", NULL, "rotated");
    else if (duplex)
      ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-sheet-back", NULL, "normal");

    if (ppm_color > 0)
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-type-supported", (int)(sizeof(pwg_raster_document_type_supported_color) / sizeof(pwg_raster_document_type_supported_color[0])), NULL, pwg_raster_document_type_supported_color);
    else
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-type-supported", (int)(sizeof(pwg_raster_document_type_supported) / sizeof(pwg_raster_document_type_supported[0])), NULL, pwg_raster_document_type_supported);
  }

  /* sides-default */
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-default", NULL, "one-sided");

  /* sides-supported */
  if (duplex)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-supported", (int)(sizeof(sides_supported) / sizeof(sides_supported[0])), NULL, sides_supported);
  else
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "sides-supported", NULL, "one-sided");

  /* urf-supported */
  if (cupsArrayFind(docformats, (void *)"image/urf"))
  {
    if (ppm_color > 0)
    {
      if (duplex)
	ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", (int)(sizeof(urf_supported_color_duplex) / sizeof(urf_supported_color_duplex[0])), NULL, urf_supported_color_duplex);
      else
	ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", (int)(sizeof(urf_supported_color) / sizeof(urf_supported_color[0])), NULL, urf_supported_color);
    }
    else if (duplex)
    {
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", (int)(sizeof(urf_supported_duplex) / sizeof(urf_supported_duplex[0])), NULL, urf_supported_duplex);
    }
    else
    {
      ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", (int)(sizeof(urf_supported) / sizeof(urf_supported[0])), NULL, urf_supported);
    }
  }

  return (attrs);
}


/*
 * 'run_test()' - Runs a particular test case
 *
 */

int 
run_test(
    char * test_case, 
    char * currentFile)
{
  char* make = (char*) malloc(100 * sizeof(char*));
  char* model = (char*) malloc(100 * sizeof(char*));
   
  int ppm = 1;
   
  char* inputFileName = (char*) malloc(100 * sizeof(char*));
  char* outputFileName = (char*) malloc(100 * sizeof(char*));
   
  char* inputContentType = (char*) malloc(100 * sizeof(char*));
  char* outputContentType = (char*) malloc(100 * sizeof(char*));
   
  // ppm_color and duplex should be supplied...
  int color = 0;
  int duplex = 0;
  int ppm_color = 0;
       
  cups_array_t* docformats = cupsArrayNew(NULL, NULL);
    
  int jobCanceled = 0;

  char* next_token1, *next_token2;
   
  char *token = strtok_r(test_case, "\t", &next_token1);
  token = remove_white_space(token);
  //printf( "%s\n", token ); //printing each token
   
  strcpy(inputFileName, token);
   
  int token_index = 1;
   
  //dynamic allocation of clargs as we dont know number of arguments
      
  char **clargs = (char**)malloc(1*sizeof(char*));
  clargs[0] = currentFile;
   
  int globalFlag = 1;

  while ( token != NULL ) 
  {
    token = strtok_r(NULL, "\t", &next_token1);
    if (!token) break;

    token = remove_white_space(token);
      
    if (globalFlag == 1)
    {
      strcpy(inputContentType, token);
      globalFlag++;
      continue;
    }
    else if (globalFlag == 2)
    {
      strcpy(outputFileName, token);
      globalFlag++;
      continue;
    }
    else if (globalFlag == 3)
    {
      strcpy(outputContentType, token);
      globalFlag++;
      continue;
    }
    else if (globalFlag == 4)
    {
      /* make string */
      strcpy(make, token);
      globalFlag++;
      continue;
    }
    else if (globalFlag == 5)
    {
      /* model string */
      strcpy(model, token);
      globalFlag++;
      continue;
    }
    else if (globalFlag == 6)
    {
      /* ppm color */
      color = atoi(token);
      if(color)
        ppm_color = 1;
        
      globalFlag++;
      continue;
    }
    else if(globalFlag == 7)
    {
      /* if printer is duplex */
      duplex = atoi(token);
      globalFlag++;
      continue;
    }
    else if(globalFlag == 8)
    {
      /* for file formats ',' seperated */
      globalFlag++;
      char* format_token = strtok_r(token, ",", &next_token2);
      
      if(format_token)
        cupsArrayAdd(docformats, format_token);
      	
      while(format_token != NULL){
       	format_token = strtok_r(NULL, ",", &next_token2);
      	if(format_token)
      	  cupsArrayAdd(docformats, format_token);
      }
      continue;
     }

     clargs = realloc(clargs, (token_index+1)*sizeof(char*));
     char* tmp_token = (char*)malloc(100*sizeof(char*));
     strcpy(tmp_token, token);
      
     clargs[token_index] = tmp_token;
     token_index++;
   }  
  
   ipp_t* emulated_ipp = load_legacy_attributes(make, model, ppm, ppm_color, duplex, docformats);
   return test_wrapper(token_index, clargs, NULL, &jobCanceled, emulated_ipp, inputContentType, outputContentType, inputFileName, outputFileName);
}

int main(int  argc,				// I - Number of command-line args
     char *argv[])			        // I - Command-line arguments{
{
  char *file_name; // File Name of Input Test File
  FILE *fp;            // File Pointer
  char *line = NULL;   // Input Stream
  size_t len = 0;      // Length of Input Stream
  ssize_t read;


  // set file_name (test.txt) from argv... 
  if (argc  > 1) 
    file_name = argv[1];
  else
  {
    fprintf(stderr, "No Input Test file Provided...\n");
    exit(EXIT_FAILURE);
  }
  char* tc_cnt = argv[2];
  int total_tc = atoi(tc_cnt) + 1;
  fprintf(stdout, "%s\n", file_name);
  fp = fopen(file_name, "r");

  if (fp == NULL)
    exit(EXIT_FAILURE);

  int test_case_no = 1;
  
  // Counts the number of test case which failed...
  int fail_cnt = 0;
  
   while ((read = getline(&line, &len, fp))!=-1 && total_tc--) 
   {
     if (!line)
       break;
     
     char* test_case = (char*)malloc(1000*sizeof(char));
     memcpy(test_case, line, strlen(line)+1);
     
     // skip lines starting with '#' (Intruction Line) ...
     if(test_case[0] == '#')
       continue;
    	
     test_case[len-1] = '\0';
     fprintf(stderr, "Running Test #%d\n", test_case_no);
     int testResult = run_test(test_case, argv[0]);
    	
     if(testResult == 0)
       fprintf(stderr, "Test Status %d: Successful\n", test_case_no);	
     else
     {
     	fprintf(stderr, "Test Status %d: Failed\n", test_case_no);
     	fail_cnt++;
     }
       
    	
     test_case_no++;
    }

   fclose(fp);
   return (fail_cnt);
}
