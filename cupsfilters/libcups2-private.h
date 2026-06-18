//
// Libcups2 header file for libcupsfilters.
//
// Copyright 2020-2022 by Till Kamppeter.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _LIBCUPS2_PRIVATE_H_
#  define _LIBCUPS2_PRIVATE_H_

#  include <config.h>

#  ifdef HAVE_LIBCUPS2 

//   These CUPS headers need to get applied before applying the
//   renaming "#define"s. Otherwise we get conflicting duplicate
//   declarations.

#    include "cups/http.h"
#    include "cups/array.h"
#    include "cups/cups.h"
#    include "cups/ipp.h"
#    include "cups/raster.h"

//   Functions renamed in libcups3

#    define cupsArrayGetCount      cupsArrayCount
#    define cupsArrayGetFirst      cupsArrayFirst
#    define cupsArrayGetNext       cupsArrayNext
#    define cupsArrayGetElement    cupsArrayIndex
#    define cupsArrayNew           cupsArrayNew3
#    define cupsGetDests           cupsGetDests2
#    define cupsGetError           cupsLastError
#    define cupsGetErrorString     cupsLastErrorString
#    define cupsRasterReadHeader   cupsRasterReadHeader2
#    define cupsRasterWriteHeader  cupsRasterWriteHeader2
#    define httpConnect            httpConnect2
#    define ippGetFirstAttribute   ippFirstAttribute
#    define ippGetNextAttribute    ippNextAttribute

//   Option parser: libcups3 spells it cupsParseOptions() with a trailing
//   "end" pointer.  CUPS 2.5 provides the same 4-argument parser under the
//   historic name cupsParseOptions2(); CUPS 2.4 only has the 3-argument form,
//   so the "end" argument is dropped there.

#    if CUPS_VERSION_MINOR >= 5
#      define cupsParseOptions cupsParseOptions2
#    else
#      define cupsParseOptions(arg, end, num_options, options) cupsParseOptions(arg, num_options, options)
#    endif

//   Function replaced by a different function in libcups3

#    define cupsCreateTempFd(prefix,suffix,buffer,bufsize) cupsTempFd(buffer,bufsize)

//   Data types renamed in libcups3

#    define cups_acopy_cb_t       cups_acopy_func_t
#    define cups_afree_cb_t       cups_afree_func_t
#    define cups_array_cb_t       cups_array_func_t
#    define cups_page_header_t    cups_page_header2_t

//   For some functions' parameters in libcups3 size_t is used while
//   int was used in libcups2. We use this type in such a case.

#    define cups_len_t            int

#  else

#    define cups_len_t            size_t

#  endif // HAVE_LIBCUPS2

#endif // !_LIBCUPS2_PRIVATE_H_
