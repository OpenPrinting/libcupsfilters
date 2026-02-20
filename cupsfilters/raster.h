//
// Functions to handle CUPS/PWG Raster headers for libcupsfilters.
//
// Copyright 2013-2022 by Till Kamppeter.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_FILTERS_RASTER_H_
#  define _CUPS_FILTERS_RASTER_H_

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Include necessary headers...
//

#  include <cupsfilters/filter.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <time.h>
#  include <math.h>

#  if defined(WIN32) || defined(__EMX__)
#    include <io.h>
#  else
#    include <unistd.h>
#    include <fcntl.h>
#  endif // WIN32 || __EMX__

#  include <cups/cups.h>
#  include <cups/raster.h>


//
// Renamed CUPS type in API
//

#if CUPS_VERSION_MAJOR < 3
#  define cups_page_header_t cups_page_header2_t
#endif


//
// 'cfRasterColorSpaceString()' - Return a human-readable
//  name for the given raster color space.
//

extern const char       *cfRasterColorSpaceString(cups_cspace_t cspace);

//
// 'cfRasterPrepareHeader()' - Prepare a CUPS/PWG raster page
// 	header based on job and printer data.
//
// Returns 0 on success and non-zero on error.
//

extern int              cfRasterPrepareHeader(cups_page_header_t *h,
					      cf_filter_data_t *data,
					      cf_filter_out_format_t
					      final_outformat,
					      cf_filter_out_format_t
					      header_outformat,
					      int no_high_depth,
					      cups_cspace_t *cspace);

//
// 'cfRasterSetColorSpace()' - Update a raster header with the
// appropriate color space and color depth based on printer
// capabilities and job settings.
//
// Returns 0 on success and -1 on error.
//

extern int              cfRasterSetColorSpace(cups_page_header_t *h,
					      const char *available,
					      const char *color_mode,
					      cups_cspace_t *cspace,
					      int *high_depth);

#  ifdef __cplusplus
}
#  endif // __cplusplus

#endif // !_CUPS_FILTERS_RASTER_H_
