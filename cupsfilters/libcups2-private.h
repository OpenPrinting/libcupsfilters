#include<config.h>

#ifndef HAVE_lIBCUPS2
# define HAVE_LIBCUPS2
#  include "cups/http.h"
#  include "cups/array.h"
#  include "cups/cups.h"
#  include "cups/ipp.h"
#  include "cups/raster.h"


#  define cupsArrayGetCount      cupsArrayCount
#  define cupsArrayGetFirst      cupsArrayFirst
#  define cupsArrayGetElement    cupsArrayIndex
#  define cupsArrayNew           cupsArrayNew3
#  define cupsGetsDests          cupsGetDests2
#  define cupsRasterReadHeader   cupsRasterReadHeader2
#  define cupsRasterWriteHeader  cupsRasterWriteHeader2
#  define httpConnect            httpConnect2
#  define ippGetFirstAttribute   ippFirstAttribute
#  define ippGetNextAttribute    ippNextAttribute



#  define cups_acopy_cb_t       cups_acopy_func_t
#  define cups_afree_cb_t       cups_afree_func_t
#  define cups_array_cb_t     cups_array_func_t
#  define cups_page_header_t    cups_page_header2_t


# endif // HAVE_LIBCUPS2

