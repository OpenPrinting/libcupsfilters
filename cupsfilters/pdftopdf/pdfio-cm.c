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
  int numObj = pdfioFileGetNumObjs(pdf);
  for(int count=0; count<numObj; count++)
  {
     pdfio_obj_t *tempObj = pdfioFileGetObj(pdf, count+1);
     pdfio_dict_t *tempDict = pdfioObjGetDict(tempObj);
     if(pdfioDictGetObj(tempDict, "Root"))
     {
       pdfio_obj_t *rootObj = pdfioDictGetObj(tempDict, "Root");
       pdfio_dict_t *rootDict = pdfioObjGetDict(rootObj);
    
       if(pdfioDictGetArray(rootDict, "OutputIntents"))
       {
	 pdfioObjClose(tempObj);
         pdfioObjClose(rootObj);
	 return (true); 
       }

       else
       {
	 pdfioObjClose(tempObj);
         pdfioObjClose(rootObj);
	 return (false); 
       }
     }

     pdfioObjClose(tempObj);
  }

  return (false);
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


  int numObj = pdfioFileGetNumObjs(pdf);
  for(int count=0; count<numObj; count++)
  {
     pdfio_obj_t *tempObj = pdfioFileGetObj(pdf, count+1);
     pdfio_dict_t *tempDict = pdfioObjGetDict(tempObj);
     if(pdfioDictGetObj(tempDict, "Root"))
     {
       pdfio_obj_t *rootObj = pdfioDictGetObj(tempDict, "Root");
       pdfio_dict_t *rootDict = pdfioObjGetDict(rootObj);
    
       if(!pdfioDictGetArray(rootDict, "OutputIntents"))
       { 
         pdfio_array_t *arrayOutputIntents = pdfioArrayCreate(pdf);
         pdfioDictSetArray(rootDict, "OutputIntents", arrayOutputIntents);
       }
       
       pdfio_array_t *arrayOutputIntents = pdfioDictGetArray(rootDict, "OutputIntents");
       pdfioDictSetArray(rootDict, "OutputIntents", arrayOutputIntents);
     
       pdfioObjClose(tempObj); 
       pdfioObjClose(rootObj);
       break;
     }
  }
}
//  }}}

void _cfPDFToPDFAddDefaultRGB(pdfio_file_t *pdf, 
			      pdfio_obj_t *srcicc) //  {{{
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
       pdfioArrayAppendObj(defaultRGB, srcicc);
       pdfioDictSetArray(cdict, "DefaultRGB", defaultRGB);
     }
     break;
  }
}
// }}}


pdfio_obj_t* 
_cfPDFToPDFSetDefaultICC(pdfio_file_t *pdf, 
			 const char *filename)// {{{
{
	pdfio_obj_t *ret = pdfioFileCreateICCObjFromFile(pdf, filename, 3);
	return ret;
}
//  }}}
