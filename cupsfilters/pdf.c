//
// Copyright 2012 Canonical Ltd.
// Copyright 2013 ALT Linux, Andrew V. Stepanov <stanv@altlinux.com>
// Copyright 2018 Sahil Arora <sahilarora.535@gmail.com>
// Copyright 2024-2025 Uddhav Phatak <uddhavphatak@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"

#include <pdfio.h>
#include <pdfio-content.h>

//
// Useful reference:
//
// http://www.gnupdf.org/Indirect_Object
// http://www.gnupdf.org/Introduction_to_PDF
// http://blog.idrsolutions.com/2011/05/understanding-the-pdf-file-format-%E2%80%93-pdf-xref-tables-explained
// http://labs.appligent.com/pdfblog/pdf-hello-world/
// https://github.com/OpenPrinting/cups-filters/pull/25
//

//
// 'make_real_box()' - Return a PDFio rect object of real values for a box.
//

static pdfio_rect_t         // O - PDFioObjectHandle for a rect
make_real_box(float values[4])  // I - Dimensions of the box in a float array
{
  pdfio_rect_t rect;

  rect.x1 = values[0];
  rect.y1 = values[1];
  rect.x2 = values[2];
  rect.y2 = values[3];

  return rect;
}

//
// 'cfPDFLoadTemplate()' - Load an existing PDF file using PDFio.
//

cf_pdf_t*
cfPDFLoadTemplate(const char *filename) 
{
  pdfio_file_t *pdf = pdfioFileOpen(filename, NULL, NULL, NULL, NULL);

  if (!pdf) 
    return NULL;

  if (pdfioFileGetNumPages(pdf) != 1) 
  {
    pdfioFileClose(pdf);
    return NULL;
  }

  return (cf_pdf_t *)pdf;
}

//
// 'cfPDFFree()' - Free pointer used by PDF object
//

void 
cfPDFFree(cf_pdf_t *pdf) 
{
  if (pdf) 
  {
    pdfioFileClose((pdfio_file_t *)pdf);
  }
}

//
// 'cfPDFPages()' - Count number of pages in file using PDFio.
//

int 
cfPDFPages(const char *filename) 
{
  pdfio_file_t *pdf = pdfioFileOpen(filename, NULL, NULL, NULL, NULL);

  if (!pdf) 
  {
    return -1;
  }

  int pages = pdfioFileGetNumPages(pdf);
  pdfioFileClose(pdf);
  return pages;
}

//
// 'cfPDFPagesFP()' - Count number of pages in file using PDFio.
//

int 
cfPDFPagesFP(char *file) 
{
  pdfio_file_t *pdf = pdfioFileOpen(file, NULL, NULL, NULL, NULL);

  if (!pdf)
    return -1;

  int pages = pdfioFileGetNumPages(pdf);
  pdfioFileClose(pdf);
  return pages;
}

//
// 'cfPDFPrependStream()' - Prepend a stream to the contents of a specified
//                          page in PDF file.
//

int 
cfPDFPrependStream(cf_pdf_t *pdf,
	       	   unsigned page_num, 
		   const char *buf, 
		   size_t len) 
{

  if(pdfioFileGetNumPages((pdfio_file_t *)pdf)==0)
    return 1;

  pdfio_obj_t *page = pdfioFileGetPage((pdfio_file_t *)pdf, page_num - 1);
  pdfio_dict_t *pageDict = pdfioObjGetDict(page);

  pdfio_stream_t *existing_stream = pdfioPageOpenStream(page, 0, true);
  if (!existing_stream) 
    return 1;

  pdfio_obj_t *new_stream_obj = pdfioFileCreateObj((pdfio_file_t *)pdf, pageDict);
  if (!new_stream_obj) 
  {
    pdfioStreamClose(existing_stream);
    return 1;
  }

  pdfio_stream_t *new_stream = pdfioObjCreateStream(new_stream_obj, PDFIO_FILTER_FLATE);
  if (!new_stream) 
  {
    pdfioStreamClose(existing_stream);
    return 1;
  }

  pdfioStreamWrite(new_stream, buf, len);
  pdfioStreamClose(new_stream);

  pdfio_stream_t *combined_stream = pdfioObjCreateStream(page, PDFIO_FILTER_FLATE);
  if (!combined_stream) 
  {
    pdfioStreamClose(existing_stream);
    return 1;
  }

  char buffer[1024];
  size_t read_len;
  while ((read_len = pdfioStreamRead(existing_stream, buffer, sizeof(buffer))) > 0) 
    pdfioStreamWrite(combined_stream, buffer, read_len);

  pdfioStreamClose(existing_stream);
  pdfioStreamClose(combined_stream);

  return 0; 
}

//
// 'cfPDFAddType1Font()' - Add the specified type1 font face to the specified
//                         page in a PDF document.
//

int 
cfPDFAddType1Font(cf_pdf_t *pdf, 
		  unsigned page_num, 
		  const char *name) 
{
  pdfio_obj_t *page = pdfioFileGetPage((pdfio_file_t *)pdf, page_num);
  pdfio_dict_t *pageDict = pdfioObjGetDict(page);
  if (!page) 
    return 1; 

  pdfio_dict_t *resources = pdfioDictGetDict(pageDict, "Resources");

  if (!resources) 
  {
    resources = pdfioDictCreate((pdfio_file_t *)pdf);
    pdfioDictSetDict(pageDict, "Resources", resources);
  }

  pdfio_dict_t *fonts = pdfioDictGetDict(resources, "Font");
  if (!fonts) 
  {
    fonts = pdfioDictCreate((pdfio_file_t *)pdf);
    pdfioDictSetDict(resources, "Font", fonts);
  }

  pdfio_dict_t *font = pdfioDictCreate((pdfio_file_t *)pdf);
  if (!font) 
    return 1; 

  pdfioDictSetName(font, "Type", "Font");
  pdfioDictSetName(font, "Subtype", "Type1");
  char basefont[256];
  snprintf(basefont, sizeof(basefont), "/%s", name);
  pdfioDictSetName(font, "BaseFont", basefont);

  pdfioDictSetDict(fonts, "bannertopdf-font", font);

  return 0;
}

//
// 'dict_lookup_rect()' - Lookup for an array of rectangle dimensions in a PDFio
//                        dictionary object. If it is found, store the values in
//                        an array and return true, else return false.
//

static bool
dict_lookup_rect(pdfio_obj_t *object,  // I - PDF dictionary object
                 const char *key,      // I - Key to lookup
                 float rect[4],        // O - Array to store values if key is found
                 bool inheritable)     // I - Whether to look for inheritable values
{
  pdfio_dict_t *dict = pdfioObjGetDict(object);
  if (!dict)
    return false;

  pdfio_obj_t *value = pdfioDictGetObj(dict, key);
  if (!value && inheritable)
    return false;

  pdfio_array_t *array = pdfioObjGetArray(value);
  if (!array || pdfioArrayGetSize(array) != 4)
    return false;

  for (int i = 0; i < 4; i++)
  {
    pdfio_obj_t *elem = pdfioArrayGetObj(array, i);

    if (pdfioArrayGetType(array, i) == PDFIO_VALTYPE_NUMBER)
    {
      rect[i] = pdfioObjGetNumber(elem);
    }
    else
      return false; // If any value is not numeric, return false
  }

  return true;
}

//
// 'fit_rect()' - Update the scale of the page using old media box dimensions
//                and new media box dimensions.
//

static void
fit_rect(float oldrect[4],  // I - Old media box
         float newrect[4],  // I - New media box
         float *scale)      // I - Pointer to scale which needs to be updated
{
  float oldwidth = oldrect[2] - oldrect[0];
  float oldheight = oldrect[3] - oldrect[1];
  float newwidth = newrect[2] - newrect[0];
  float newheight = newrect[3] - newrect[1];

  *scale = newwidth / oldwidth;
  if (oldheight * (*scale) > newheight)
    *scale = newheight / oldheight;
}

//
// 'cfPDFResizePage()' - Resize page in a PDF with the given dimensions.
//

int cfPDFResizePage(cf_pdf_t *pdf,   // I - Pointer to PDFio file object
                    unsigned page_num,   // I - Page number (1-based index)
                    float width,         // I - New width of the page
                    float length,        // I - New length of the page
                    float *scale)        // O - Scale of the page to be updated
{
  pdfio_obj_t *page = pdfioFileGetPage((pdfio_file_t *)pdf, page_num - 1);
  if (!page)
    return 1; 

  float new_mediabox[4] = {0.0, 0.0, width, length};
  float old_mediabox[4];
  pdfio_rect_t media_box;

  if (!dict_lookup_rect(page, "/MediaBox", old_mediabox, true))
    return (1);
  
  fit_rect(old_mediabox, new_mediabox, scale);
  media_box = make_real_box(new_mediabox);
  
  pdfio_dict_t *pageDict = pdfioObjGetDict(page);
  if (pageDict)
  {
    pdfioDictSetRect(pageDict, "CropBox", &media_box);
    pdfioDictSetRect(pageDict, "TrimBox", &media_box);
    pdfioDictSetRect(pageDict, "BleedBox", &media_box);
    pdfioDictSetRect(pageDict, "ArtBox", &media_box);
  }

  return 0; 
}

//
// 'cfPDFDuplicatePage()' - Duplicate a specified pdf page in a PDF
//

int 
cfPDFDuplicatePage(cf_pdf_t *pdf, 
		   unsigned page_num,
		   unsigned count) 
{
  pdfio_obj_t *page = pdfioFileGetPage((pdfio_file_t *)pdf, page_num - 1);

  if (!page) 
    return 1;

  for (unsigned i = 0; i < count; ++i) 
  {
    pdfioPageCopy((pdfio_file_t *)pdf, page);
  }

  return 0;
}

//
// 'cfPDFWrite()' - Write the contents of PDF object to an already open FILE*.
//

void 
cfPDFWrite(cf_pdf_t *pdf, 
	   FILE *file) 
{
//  pdfioFileCreateImageObjFromFile((pdfio_file_t *)pdf, file, false);
}

//
// 'cfPDFFillForm()' - Fill recognized fields with information
//

int cfPDFFillForm(cf_pdf_t *doc, cf_opt_t *opt) {
    // TODO: PDFio does not directly support form filling.
    return 0;
}

