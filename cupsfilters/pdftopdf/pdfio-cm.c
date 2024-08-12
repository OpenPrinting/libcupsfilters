//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-cm-private.h"
#include <stdio.h>
#include "cupsfilters/debug-internal.h" 

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <pdfio.h>
#include <pdfio-content.h>

bool 
_cfPDFToPDFHasOutputIntent(pdfio_file_t *pdf) // {{{
{
  pdfio_dict_t *catalog = pdfioFileGetCatalog(pdf);
  if(!pdfioDictGetArray(catalog, "OutputIntents"))
    return false;
  return true;
}
// }}}

void 
_cfPDFToPDFAddOutputIntent(pdfio_file_t *pdf, 
			   const char *filename) // {{{
{
  pdfio_obj_t *outicc = pdfioFileCreateICCObjFromFile(pdf, filename, 4);
  
  pdfio_dict_t *intent = pdfioDictCreate(pdf);
  pdfioDictSetName(intent, "Type", "OutputIntent");
  pdfioDictSetName(intent, "S", "GTS_PDFX");
  pdfioDictSetString(intent, "OutputCondition", "(Commercial and specialty printing)");
  pdfioDictSetString(intent, "Info", "(none)");
  pdfioDictSetString(intent, "OutputConditionIdentifier", "(CGATS TR001i)");
  pdfioDictSetString(intent, "RegistryName", "(http://www.color.orgi)");
  pdfioDictSetObj(intent, "DestOutputProfile", outicc);

  pdfio_dict_t *catalog = pdfioFileGetCatalog(pdf);
  pdfio_array_t *outputIntents = pdfioDictGetArray(catalog, "OutputIntents");
  if (!outputIntents) 
  {
    outputIntents = pdfioArrayCreate(pdf);
    pdfioDictSetArray(catalog, "OutputIntents", outputIntents);
  }
  
  pdfioArrayAppendDict(outputIntents, intent);
}
// }}}

//
// for color management:
// Use /DefaultGray, /DefaultRGB, /DefaultCMYK ...  from *current* resource
// dictionary ...
// i.e. set 
// /Resources <<
// /ColorSpace <<    --- can use just one indirect ref for this (probably)
// /DefaultRGB [/ICCBased 5 0 R]  ... sensible use is sRGB  for DefaultRGB, etc.
// >>
// >>
// for every page  (what with form /XObjects?)  and most importantly RGB
// (leave CMYK, Gray for now, as this is already printer native(?))
//
// ? also every  form XObject, pattern, type3 font, annotation appearance
//   stream(=form xobject +X)
//
// ? what if page already defines /Default?   -- probably keep!
//
// ? maybe we need to set /ColorSpace  in /Images ?
//   [gs idea is to just add the /Default-key and then reprocess...]
//

void 
_cfPDFToPDFAddDefaultRGB(pdfio_file_t *pdf, 
			 pdfio_obj_t *icc_obj) // {{{
{
  pdfio_array_t *icc_array = pdfioArrayCreate(pdf);
    
  pdfioArrayAppendName(icc_array, "ICCBased");
  pdfioArrayAppendObj(icc_array, icc_obj);

  size_t num_pages = pdfioFileGetNumPages(pdf);
  for (size_t i = 0; i < num_pages; i++)
  {
    pdfio_obj_t *page_obj = pdfioFileGetPage(pdf, i + 1);  
    pdfio_dict_t *page_dict = pdfioObjGetDict(page_obj);

    pdfio_dict_t *rdict = pdfioDictGetDict(page_dict, "Resources");
    if (!rdict) 
    {
      rdict = pdfioDictCreate(pdf);
      pdfioDictSetDict(page_dict, "Resources", rdict);
    }

    pdfio_dict_t *cdict = pdfioDictGetDict(rdict, "ColorSpace");
    if (!cdict) 
    {
      cdict = pdfioDictCreate(pdf);
      pdfioDictSetDict(rdict, "ColorSpace", cdict);
    }

    pdfioDictSetArray(cdict, "DefaultRGB", icc_array);
  }
}
// }}}

pdfio_obj_t*
_cfPDFToPDFSetDefaultICC(pdfio_file_t *pdf, 
			 const char *filename) // {{{
{
    return pdfioFileCreateICCObjFromFile(pdf, filename, 3);
}
// }}}
