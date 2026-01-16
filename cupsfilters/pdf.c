//
// Copyright 2012 Canonical Ltd.
// Copyright 2013 ALT Linux, Andrew V. Stepanov <stanv@altlinux.com>
// Copyright 2018 Sahil Arora <sahilarora.535@gmail.com>
// Copyright 2024-2026 Uddhav Phatak <uddhavphatak@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

cf_pdf_t*					  // O - Pointer to cf_pdf_t struct
cfPDFLoadTemplate(const char *filename) 	// I - Filename of the PDF file
{
  // open pdf file from filename
  pdfio_file_t *pdf = pdfioFileOpen(filename, NULL, NULL, NULL, NULL);

  if (!pdf) 
    return NULL;

  // get number of pages, if it isn't 1; its invalid PDF file
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

void 				  // O - void
cfPDFFree(cf_pdf_t *pdf) 	// I - pointer to the cf_pdf_t struct to free
{
  if (pdf) 
  {
    pdfioFileClose((pdfio_file_t *)pdf);
  }
}

//
// 'cfPDFPages()' - Count number of pages in file using PDFio.
//

int 					  // O - Number of pages or -1 on error
cfPDFPages(const char *filename) 	// I - Filename of PDF
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

int 				  // O - Number of pages or -1 on error
cfPDFPagesFP(FILE *file) 	// I - File pointer of PDF
{
  // Convert the FILE to a temporary file, as PDFio doesn't open PDF without path
  char temp_filename[] = "/tmp/pdf-file-XXXXXX";
  int temp_fd = mkstemp(temp_filename);

  if (temp_fd == -1) 
  {
    fprintf(stderr, "mkstemp: Could not create temporary file\n");
    return (-1);
  }

  FILE *temp_fp = fdopen(temp_fd, "wb+");
  if (!temp_fp) 
  {
    fprintf(stderr, "fdopen: Could not open temporary file stream\n");
    close(temp_fd);
    unlink(temp_filename); 
    return 1;
  }

  // Copy contents from input stream to temporary file
  char buffer[8192];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) 
  {
    fwrite(buffer, 1, bytes_read, temp_fp);
  }

  // Open the temp file with pdfio
  pdfio_file_t *pdf = pdfioFileOpen(temp_filename, NULL, NULL, NULL, NULL);

  if (!pdf)
  {
    fclose(temp_fp);
    unlink(temp_filename);
    return -1;
  }

  int pages = pdfioFileGetNumPages(pdf);

  // Cleanup
  fclose(temp_fp);
  unlink(temp_filename);
  pdfioFileClose(pdf);
  return pages;
}

//
// 'cfPDFPrependStream()' - Prepend a stream to the contents of a specified
//                          page in PDF file.
//

int 						  // O - 0 on success, 1 on error
cfPDFPrependStream(cf_pdf_t *pdf,		// I - Pointer to PDF file
	       	   unsigned page_num, 		// I - page number to prepend to
		   const char *buf, 		// I - Buffer containing stream data
		   size_t len) 			// I - Length of Buffer
{
  // This is used to place content "underneath" existing page content, typically
  // for adding backgrounds or forms
	
  if(pdfioFileGetNumPages((pdfio_file_t *)pdf)==0)
    return 1;

  // Get the target page object
  pdfio_obj_t *page = pdfioFileGetPage((pdfio_file_t *)pdf, page_num - 1);
  pdfio_dict_t *pageDict = pdfioObjGetDict(page);

  // Open the existing content stream of the page
  pdfio_stream_t *existing_stream = pdfioPageOpenStream(page, 0, true);
  if (!existing_stream) 
    return 1;

  // Create a new stream object to hold the content we want to prepend
  pdfio_obj_t *new_stream_obj = pdfioFileCreateObj((pdfio_file_t *)pdf, pageDict);
  if (!new_stream_obj) 
  {
    pdfioStreamClose(existing_stream);
    return 1;
  }

  // Write the new buffer (buf) into the new stream
  pdfio_stream_t *new_stream = pdfioObjCreateStream(new_stream_obj, PDFIO_FILTER_FLATE);
  if (!new_stream) 
  {
    pdfioStreamClose(existing_stream);
    return 1;
  }

  pdfioStreamWrite(new_stream, buf, len);
  pdfioStreamClose(new_stream);

  // Create a combined stream on the page to merge new + existing
  // NOTE :  This logic does overwriting/appending to the page content stream,
  // 	     be cautious while making changes
  pdfio_stream_t *combined_stream = pdfioObjCreateStream(page, PDFIO_FILTER_FLATE);
  if (!combined_stream) 
  {
    pdfioStreamClose(existing_stream);
    return 1;
  }

  // Copy existing stream content to the combined stream
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

int 					  // O - 0 on success , 1 on error
cfPDFAddType1Font(cf_pdf_t *pdf, 	// I - Pointer to PDF object
		  unsigned page_num, 	// I - Page number to add font to
		  const char *name) 	// I - Name of the font
{
  pdfio_obj_t *page = pdfioFileGetPage((pdfio_file_t *)pdf, page_num);
  pdfio_dict_t *pageDict = pdfioObjGetDict(page);
  if (!page) 
    return 1; 

  // Locate or create the Resources dictionary
  pdfio_dict_t *resources = pdfioDictGetDict(pageDict, "Resources");

  if (!resources) 
  {
    resources = pdfioDictCreate((pdfio_file_t *)pdf);
    pdfioDictSetDict(pageDict, "Resources", resources);
  }

  // Locate or create the Font dictionary within Resources
  pdfio_dict_t *fonts = pdfioDictGetDict(resources, "Font");
  if (!fonts) 
  {
    fonts = pdfioDictCreate((pdfio_file_t *)pdf);
    pdfioDictSetDict(resources, "Font", fonts);
  }

  // Create the specific Font dictionary for this Type1 font
  pdfio_dict_t *font = pdfioDictCreate((pdfio_file_t *)pdf);
  if (!font) 
    return 1; 

  pdfioDictSetName(font, "Type", "Font");
  pdfioDictSetName(font, "Subtype", "Type1");
  char basefont[256];
  snprintf(basefont, sizeof(basefont), "/%s", name);
  pdfioDictSetName(font, "BaseFont", basefont);

  // Register the font under the name "bannertopdf-font"
  // Note: This fixed key name implies only one such font can be added per page via this function.
  pdfioDictSetDict(fonts, "bannertopdf-font", font);

  return 0;
}

//
// 'dict_lookup_rect()' - Lookup for an array of rectangle dimensions in a PDFio
//                        dictionary object. If it is found, store the values in
//                        an array and return true, else return false.
//

static bool				 // O - true if found, false otherwise
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

int 					// O - 0 if success, 1 if error
cfPDFResizePage(cf_pdf_t *pdf,       // I - Pointer to PDFio file object
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

  // Get original dimensions to calculate scale
  if (!dict_lookup_rect(page, "MediaBox", old_mediabox, true))
    return (1);
  
  fit_rect(old_mediabox, new_mediabox, scale);
  media_box = make_real_box(new_mediabox);
  
  // Set the new boxes
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

int 					  // O - 0 if success, 1 if error
cfPDFDuplicatePage(cf_pdf_t *pdf, 	// I - pointer to PDF file
		   unsigned page_num,	// I - page number
		   unsigned count) 	// I - number of copies to make
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
// TODO
//  pdfioFileCreateImageObjFromFile((pdfio_file_t *)pdf, file, false);
}

//
// 'cfPDFFillForm()' - Fill recognized fields with information
//

int
cfPDFFillForm(cf_pdf_t *doc, cf_opt_t *opt)
{
    // TODO: PDFio does not directly support form filling.
    return 0;
}

