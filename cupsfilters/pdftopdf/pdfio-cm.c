#include "pdfio-cm-private.h"  
#include <stdio.h>
#include "cupsfilters/debug-internal.h"
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <pdfio-content.h>

bool 
_cfPDFToPDFHasOutputIntent(pdfio_file_t *pdf)   // {{{
{
  pdfio_dict_t *catalogDict = pdfioFileGetCatalog(pdf);
  if(!pdfioDictGetArray(catalogDict, "OutputIntents"))
    return false;
  return true; 
}
// }}}

void 
_cfPDFToPDFAddOutputIntent(pdfio_file_t *pdf, 
			   const char *filename) //{{{
{
  pdfio_obj_t *outicc = pdfioFileCreateICCObjFromFile(pdf, filename, 4);

  pdfio_dict_t *intent = pdfioDictCreate(pdf);
 
  pdfioDictSetName(intent, "Type", "OutputIntent");
  pdfioDictSetName(intent, "S", "GTS_PDFX");
  pdfioDictSetString(intent, "OutputCondition", "Commercial and specialty printing");
  pdfioDictSetString(intent, "Info", "none");
  pdfioDictSetString(intent, "OutputConditionIdentifier", "CGATS TR001");
  pdfioDictSetName(intent, "RegistryName", "http://www.color.org");
  pdfioDictSetNull(intent, "DestOutputProfile");
  
  pdfioDictSetObj(intent, "DestOutputProfile", outicc);


  pdfio_dict_t *catalogDict = pdfioFileGetCatalog(pdf);
  if(!pdfioDictGetArray(catalogDict, "OutputIntents"))
  {
    pdfio_array_t *newArray = pdfioArrayCreate(pdf);
    pdfioDictSetArray(catalogDict, "OutputIntents", newArray);
  }
  pdfioDictSetArray(intent, "OutputIntents", pdfioDictGetArray(catalogDict, "OutputIntents"));
}
//  }}}

void _cfPDFToPDFAddDefaultRGB(pdfio_file_t *pdf, 
			      pdfio_stream_t *srcicc) //  {{{
{

  int numPages = pdfioFileGetNumPages(pdf);
  for(int count=0; count<numPages; count++)
  {
     pdfio_obj_t *tempPage = pdfioFileGetPage(pdf, count+1);
     pdfio_dict_t *pageDict = pdfioObjGetDict(tempPage);
    
     if(!pdfioDictGetDict(pageDict, "Resources"))
     {
       pdfio_dict_t *resourcesDict = pdfioDictCreate(pdf);
       pdfioDictSetDict(pageDict, "Resources", resourcesDict);
     }
     pdfio_dict_t *rdict = pdfioDictGetDict(pageDict, "resources");

     if(!pdfioDictGetDict(rdict, "ColourSpace"))
     {
       pdfio_dict_t *colourDict = pdfioDictCreate(pdf);
       pdfioDictSetDict(rdict, "Resources", colourDict);
     } 
     pdfio_dict_t *cdict = pdfioDictGetDict(rdict, "ColourSpace");
    
     if(!pdfioDictGetDict(cdict, "DefaultRGB"))
     {
       pdfio_array_t *defaultRGB = pdfioArrayCreate(pdf);
       pdfioArrayAppendName(defaultRGB, "ICCBased");
       pdfioDictSetArray(cdict, "DefaultRGB", defaultRGB);
	
     }
     break;
  }
}
// }}}


pdfio_stream_t* 
_cfPDFToPDFSetDefaultICC(pdfio_file_t *pdf, 
			 const char *filename)// {{{
{
	pdfio_obj_t *retobj = pdfioFileCreateICCObjFromFile(pdf, filename, 3);
	pdfio_stream_t *ret = pdfioObjCreateStream(retobj, PDFIO_FILTER_NONE);
	return ret;
}
//  }}}
