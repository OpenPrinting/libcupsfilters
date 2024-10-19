//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-xobject-private.h"
#include "pdfio-tools-private.h"
#include "pdfio-pdftopdf-private.h"

#include <pdfio.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

typedef struct 
{
  size_t content_count;
  pdfio_stream_t **contents;
} CombineFromContents_Provider;

CombineFromContents_Provider* 
CombineFromContents_Provider_new(pdfio_stream_t **contents, 
				 size_t content_count) // {{{
{
  CombineFromContents_Provider *provider = (CombineFromContents_Provider*)malloc(sizeof(CombineFromContents_Provider));
    
  provider->content_count = content_count;
  provider->contents = contents;
  return provider;
}
// }}}

void 
CombineFromContents_Provider_free(CombineFromContents_Provider *provider) // {{{
{
  if (provider) 
  {
    free(provider->contents);
    free(provider);
  }
}
// }}}

void 
CombineFromContents_Provider_provideStreamData(CombineFromContents_Provider *provider, 
					       pdfio_stream_t *pipeline) // {{{
{
  char buffer[8192];
  size_t bytes;

  for (size_t i = 0; i < provider->content_count; i++) 
  {
    pdfio_stream_t *stream = provider->contents[i];
    while ((bytes = pdfioStreamRead(stream, buffer, sizeof(buffer))) > 0) 
    {
      pdfioStreamWrite(pipeline, buffer, bytes);
    }
  }
}
// }}}

//
//  To convert a page to an XObject there are several keys to consider:
//
//  /Type /Page        -> /Type /XObject (/Type optional for XObject)
//                     -> /Subtype /Form
//                     -> [/FormType 1]  (optional)
//  /Parent ? ? R      -> remove
//  /Resources dict    -> copy
//  /MediaBox rect [/CropBox /BleedBox /TrimBox /ArtBox] 
//                     -> /BBox  (use TrimBox [+ Bleed consideration?], 
//                        with fallback to /MediaBox)
//                        note that /BBox is in *Form Space*, see /Matrix!
//  [/BoxColorInfo dict]   (used for guidelines that may be shown by viewer)
//                     -> ignore/remove
//  [/Contents asfd]   -> concatenate into stream data of the XObject
//                        (page is a dict, XObject a stream)
//
//  [/Rotate 90]   ... must be handled (either use CTM where XObject is
//                        /used/ -- or set /Matrix)
//  [/UserUnit] (PDF 1.6)   -> to /Matrix ?   -- it MUST be handled.
//
//  [/Group dict]      -> copy
//  [/Thumb stream]    -> remove, not needed any more / would have to be
//                        regenerated (combined)
//  [/B]               article beads -- ignore for now
//  [/Dur]             -> remove  (transition duration)
//  [/Trans]           -> remove  (transitions)
//  [/AA]              -> remove  (additional-actions)
//
//  [/Metadata]        what shall we do?? (kill: we can't combine XML)
//  [/PieceInfo]       -> remove, we can't combine private app data (?)
//  [/LastModified  date]  (opt except /PieceInfo)  -> see there
//
//  [/PZ]              -> remove, can't combine/keep (preferred zoom level)
//  [/SeparationInfo]  -> remove, no way to keep this (needed for separation)
//
//  [/ID]              related to web capture -- ignore/kill?
//  [/StructParents]   (opt except pdf contains "structural content items")
//                     -> copy (is this correct?)
//

pdfio_obj_t*
_cfPDFToPDFMakeXObject(pdfio_file_t *pdf, 
		       pdfio_obj_t *page) // {{{
{
  pdfio_dict_t *page_dict = pdfioObjGetDict(page);

    // Create the XObject dictionary
  pdfio_dict_t *dict = pdfioDictCreate(pdf);

  if (!dict) {
    fprintf(stderr, "Failed to create dictionary for XObject.\n");
    return NULL;
  }

  pdfioDictSetName(dict, "Type", "XObject");
  pdfioDictSetName(dict, "Subtype", "Form");

  // Set BBox from TrimBox or MediaBox
  pdfio_rect_t box = _cfPDFToPDFGetTrimBox(page);
  pdfioDictSetRect(dict, "BBox", &box);

  // [/Matrix .]   ...  default is [1 0 0 1 0 0]; we incorporate /UserUnit and
  _cfPDFToPDFMatrix mtx;
  _cfPDFToPDFMatrix_init(&mtx);

  //               /Rotate here
  double user_unit = _cfPDFToPDFGetUserUnit(page);
  _cfPDFToPDFMatrix_scale(&mtx, user_unit, user_unit);

  // transform, so that bbox is [0 0 w h]  (in outer space, but after UserUnit)
  pdftopdf_rotation_e rot = _cfPDFToPDFGetRotate(page);

  // calculate rotation effect on [0 0 w h]
  _cfPDFToPDFPageRect bbox = _cfPDFToPDFGetBoxAsRect(&box),
                        tmp;
  tmp.left = 0;
  tmp.bottom = 0;
  tmp.right = 0;
  tmp.top = 0;

  _cfPDFToPDFPageRect_rotate_move(&tmp, rot, bbox.width, bbox.height);
  
  // tmp.rotate_move moves the bbox; we must achieve this move with the matrix.
  _cfPDFToPDFMatrix_translate(&mtx, tmp.left, tmp.bottom);// 1. move origin to end up at
                                       			  //    left,bottom after rotation


  _cfPDFToPDFMatrix_rotate(&mtx, rot); // 2. rotate coordinates according to /Rotate
  _cfPDFToPDFMatrix_translate(&mtx, -bbox.left, -bbox.bottom);// 3. move origin from 0,0 to
                                                              //    "form space"


  pdfio_array_t *matrix_array = pdfioArrayCreate(pdf);
  for (int i = 0; i < 6; i++) 
  {
    pdfioArrayAppendNumber(matrix_array, mtx.ctm[i]);
  }
  pdfioDictSetArray(dict, "Matrix", matrix_array);

  pdfio_obj_t *resources = pdfioDictGetObj(page_dict, "Resources");
  if (resources) 
  {
    pdfioDictSetObj(dict, "Resources", resources);
  }

  pdfio_obj_t *group = pdfioDictGetObj(page_dict, "Group");
  if (group) 
  {
    pdfioDictSetObj(dict, "Group", group);
  }

  // Create the stream inside the XObject using pdfioFileCreateObj
  pdfio_obj_t *xobject = pdfioFileCreateObj(pdf, dict);
  if (!xobject) 
  {
    fprintf(stderr, "Failed to create XObject.\n");
    return NULL;
  }

  pdfio_stream_t **contents = NULL;
  size_t content_count = pdfioPageGetNumStreams(page);

  for (size_t i = 0; i < content_count; i++)
  {
    contents[i] = pdfioPageOpenStream(page, i, false);
  }

  if (!contents || content_count == 0)
  {
    fprintf(stderr, "No valid contents streams found in the page dictionary.\n");
    return NULL;
  }

  // Write the content streams to the XObject stream
  pdfio_stream_t *xobject_stream = pdfioObjOpenStream(xobject, true);

  CombineFromContents_Provider *provider = CombineFromContents_Provider_new(contents, content_count);
  if (provider)
  {
    CombineFromContents_Provider_provideStreamData(provider, xobject_stream);
    CombineFromContents_Provider_free(provider);
  }
  else
  {
    fprintf(stderr, "Failed to create CombineFromContents_Provider.\n");
    pdfioStreamClose(xobject_stream);
    return NULL;
  }

  // Finalize the XObject stream and return the object
  pdfioStreamClose(xobject_stream);

  return xobject;
}
// }}}
