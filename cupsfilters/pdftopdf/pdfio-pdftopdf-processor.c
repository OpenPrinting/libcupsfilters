//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "pdfio.h"  // Replace with appropriate PDFIO headers
#include "C-pdftopdf-processor-private.h"
#include "pdfio-pdftopdf-private.h"
#include "pdfio-tools-private.h"
#include "pdfio-xobject-private.h"
#include "pdfio-pdftopdf-processor-private.h"

#define DEBUG_assert(x) do { if (!(x)) abort(); } while (0)

//map definition
unsigned int 
hash(const char *key) 
{
  unsigned int hash = 0;
  while (*key) {
    hash = (hash << 5) + *key++;
  }
  return hash % HASH_TABLE_SIZE;
}

// Initialize a new hash table
HashTable*
hashCreate_hash_table() 
{
  HashTable *table = malloc(sizeof(HashTable));
  for (int i = 0; i < HASH_TABLE_SIZE; i++) 
  {
    table->buckets[i] = NULL;
  }
  table->count = 0;  // Initialize the count of filled elements
  return table;
}

// Create a new key-value pair
KeyValuePair*
create_key_value_pair(const char *key, pdfio_obj_t *value) 
{
  KeyValuePair *new_pair = malloc(sizeof(KeyValuePair));
  new_pair->key = strdup(key);  // Duplicate the key string
  new_pair->value = value;
  new_pair->next = NULL;
  return new_pair;
}

// Insert a key-value pair into the hash table
void 
hashInsert(HashTable *table, const char *key, pdfio_obj_t *value) 
{
  unsigned int index = hash(key);
  KeyValuePair *new_pair = create_key_value_pair(key, value);

  // Handle collisions using chaining (linked list)
  if (table->buckets[index] == NULL) 
  {
    table->buckets[index] = new_pair;
  } 
  else 
  {
    KeyValuePair *current = table->buckets[index];
    while (current->next != NULL) 
    {
      current = current->next;
    }
    current->next = new_pair;
  }
  table->count++;  // Increment the count of filled elements
}

// Retrieve a value by key from the hash table
pdfio_obj_t*
hashGet(HashTable *table, const char *key) 
{
  unsigned int index = hash(key);
  KeyValuePair *current = table->buckets[index];

  while (current != NULL) {
    if (strcmp(current->key, key) == 0) {
      return current->value;
    }
    current = current->next;
  }
  return NULL;  // Key not found
}

// Get the number of elements currently filled in the hash table
int 
hashGet_filled_count(HashTable *table) 
{
  return table->count;
}

// Free the hash table
void 
hashFree_hash_table(HashTable *table) 
{
  for (int i = 0; i < HASH_TABLE_SIZE; i++) 
  {
    KeyValuePair *current = table->buckets[i];
    while (current != NULL) 
    {
      KeyValuePair *tmp = current;
      current = current->next;
      free(tmp->key);
      free(tmp);
    }
  }
  free(table);
}

// main code starts

// Use: append_debug_box(content, &pe.sub, xpos, ypos);
static void 
append_debug_box(char *content, 
		 const _cfPDFToPDFPageRect *box, 
		 float xshift, float yshift) // {{{
{
  char buf[1024];
  snprintf(buf, sizeof(buf),
           "q 1 w 0.1 G\n %f %f m  %f %f l S \n %f %f m  %f %f l S \n %f %f  %f %f re S Q\n",
           box->left + xshift, box->bottom + yshift,
           box->right + xshift, box->top + yshift,
           box->right + xshift, box->bottom + yshift,
           box->left + xshift, box->top + yshift,
           box->left + xshift, box->bottom + yshift,
           box->right - box->left, box->top - box->bottom);

  size_t new_size = strlen(content) + strlen(buf) + 1;

  char *new_content = realloc(content, new_size);
  if (!new_content) {
      fprintf(stderr, "Memory allocation failed\n");
      return;
  }

  content = new_content;  // Update the pointer to the new allocated memory

  strcat(content, buf);
}
// }}}

void 
_cfPDFToPDF_PDFioProcessor_existingMode(_cfPDFToPDF_PDFioProcessor *handle, 
			                pdfio_obj_t *page, 
					int orig_no) // {{{
{
  handle->page = page;
  
  if (orig_no == -1) 
    handle->no = -1;  // Default value handling
  else 
    handle->no = orig_no;

  handle->rotation = ROT_0;

  handle->xobjs = NULL;
  handle->xobjs->count = 0;
  handle->content = NULL;

  handle->pdf = NULL;
  handle->orig_pages = NULL;
  handle->orig_pages_size = 0;
  handle->orig_pages_capacity = 0;
  
  handle->hasCM = false; 
  handle->extraheader = NULL;
}
// }}}

void 
_cfPDFToPDF_PDFioProcessor_create_newMode(_cfPDFToPDF_PDFioProcessor *handle, 
				          pdfio_file_t *pdf, 
					  float width, float height) // {{{
{
  handle->no = 0;
  handle->rotation = ROT_0;

  handle->content = strdup("q\n");  // Allocate and copy content string

  pdfio_dict_t *pageDict = pdfioDictCreate(pdf);

  pdfio_rect_t *media_box = _cfPDFToPDFMakeBox(0, 0, width, height);

  pdfio_dict_t *resources_dict = pdfioDictCreate(pdf);
  pdfioDictSetNull(resources_dict, "XObject");

  pdfioDictSetName(pageDict, "Type", "Page");
  pdfioDictSetRect(pageDict, "MediaBox", media_box);
  pdfioDictSetDict(pageDict, "Resources", resources_dict);

  // Add /Resources dictionary with empty /XObject entry
  pdfio_dict_t *resource_dict = pdfioDictCreate(pdf);
  pdfioDictSetNull(resource_dict, "XObject");
  pdfioDictSetDict(pageDict, "Resources", resource_dict);

  pdfio_stream_t *content_stream = pdfioFileCreatePage(pdf, pageDict);
  pdfioStreamWrite(content_stream, handle->content, strlen(handle->content));
  pdfioStreamClose(content_stream);

  handle->xobjs = NULL;
  handle->xobjs->count = 0;

  handle->pdf = NULL;
  handle->orig_pages = NULL;
  handle->orig_pages_size = 0;
  handle->orig_pages_capacity = 0;
  
  handle->hasCM = false; 
  handle->extraheader = NULL;
}
// }}}

void 
_cfPDFToPDF_PDFioProcessor_destroy(_cfPDFToPDF_PDFioProcessor *handle) // {{{
{
  if (handle->pdf != NULL) 
    pdfioFileClose(handle->pdf);
    

  free(handle->xobjs);
  free(handle->content);
  free(handle->orig_pages);

  // Free the extraheader if it was allocated
  if (handle->extraheader != NULL) 
    free(handle->extraheader);
  
}
// }}}

bool 
_cfPDFToPDF_PDFioProcessor_is_existing(struct _cfPDFToPDF_PDFioProcessor *handle) // {{{
{
  return (handle->content == NULL || 
		  strlen((const char *)handle->content) == 0);
}
// }}}


_cfPDFToPDFPageRect 
_cfPDFToPDF_PDFioProcessor_get_rect(const _cfPDFToPDF_PDFioProcessor *handle) // {{{
{
  pdfio_rect_t trimBox = _cfPDFToPDFGetTrimBox(handle->page); 
  _cfPDFToPDFPageRect ret = _cfPDFToPDFGetBoxAsRect(&trimBox);
  _cfPDFToPDFPageRect_translate(&ret, -ret.left, -ret.bottom);
  _cfPDFToPDFPageRect_rotate_move(&ret, _cfPDFToPDFGetRotate(handle->page), ret.width, ret.height);
  _cfPDFToPDFPageRect_scale(&ret, _cfPDFToPDFGetUserUnit(handle->page));

  return (ret); 
}
// }}}

pdfio_obj_t*
_cfPDFToPDF_PDFioProcessor_get(struct _cfPDFToPDF_PDFioProcessor *handle) // {{{
{
  pdfio_dict_t *resources, *contents_dict;
  pdfio_stream_t *contents_stream;

  pdfio_obj_t *ret = handle->page;

  if (!_cfPDFToPDF_PDFioProcessor_is_existing(handle)) 
  {
    // Step 1: Replace /XObject in /Resources
    resources = pdfioDictGetDict(pdfioObjGetDict(ret), "/Resources");
    if (resources)
    {
      char name_buffer[handle->xobjs->count];   
      int xobj_index = 0; 
      // Loop through all the buckets
      for (int i = 0; i < HASH_TABLE_SIZE && xobj_index < handle->xobjs->count; i++) 
      {
        KeyValuePair *current = handle->xobjs->buckets[i];

        // Traverse the linked list in case of collisions
        while (current != NULL) 
	{
          // Build the XObject name (e.g., "/X1", "/X2", ...)
          snprintf(name_buffer, sizeof(name_buffer), "/X%d", xobj_index + 1);
            
          // Add the object to the PDF resources dictionary
          pdfioDictSetObj(resources, name_buffer, current->value);  // current->value is pdfio_obj_t *

          // Move to the next element in the linked list (in case of collisions)
          current = current->next;
            
          // Increment the XObject index
          xobj_index++;
        }
      }

      pdfioDictSetDict(resources, "/XObject", resources);  // Set new dictionary for XObject
    }

    contents_stream = pdfioPageOpenStream(ret, 0, true);  // Open the content stream of the page
    if (contents_stream)
    {
      pdfioStreamPuts(contents_stream, "Q\n");  // Append "Q\n" to content
      pdfioStreamClose(contents_stream);  // Close the stream
    }

    contents_dict = pdfioDictGetDict(pdfioObjGetDict(ret), "/Contents");
    if (contents_dict)
    {
      pdfioDictSetNull(contents_dict, "/Filter");  // Remove filter keys
      pdfioDictSetNull(contents_dict, "/DecodeParms");
    }

    pdfioDictSetNumber(pdfioObjGetDict(ret), "/Rotate", _cfPDFToPDFMakeRotate(handle->rotation));
  } 
  else 
  {
    pdftopdf_rotation_e rot = pdftopdf_rotation_add(_cfPDFToPDFGetRotate(handle->page), 
		    			            handle->rotation);
    pdfioDictSetNumber(pdfioObjGetDict(handle->page), "Rotate", rot);
  }

  handle->page = NULL;
  return ret;
}
// }}}

static _cfPDFToPDFPageRect
ungetRect(_cfPDFToPDFPageRect rect,
          const _cfPDFToPDF_PDFioProcessor *ph,
          pdftopdf_rotation_e rotation,
          pdfio_obj_t *page)  // {{{
{
  
  _cfPDFToPDFPageRect pg1 = _cfPDFToPDF_PDFioProcessor_get_rect(ph);
  pdfio_rect_t TrimBox = _cfPDFToPDFGetTrimBox(page);
  _cfPDFToPDFPageRect pg2 = _cfPDFToPDFGetBoxAsRect(&TrimBox); 
  rect.width = pg1.width;
  rect.height = pg1.height;
  _cfPDFToPDFPageRect_rotate_move(&rect, pdftopdf_rotation_neg(_cfPDFToPDFGetRotate(page)), pg1.width, pg1.height);
  
  _cfPDFToPDFPageRect_scale(&rect, 1.0/_cfPDFToPDFGetUserUnit(page));
  _cfPDFToPDFPageRect_translate(&rect, pg2.left, pg2.bottom);

  return rect;
}
// }}}

void 
_cfPDFToPDF_PDFioProcessor_add_border_rect(_cfPDFToPDF_PDFioProcessor *handle,   
					   pdfio_file_t *pdf,	
   					   const _cfPDFToPDFPageRect givenRect,              
					   pdftopdf_border_type_e border, 
					   float fscale) // {{{
{
  double lw = (border & THICK) ? 0.5 : 0.24;
  double line_width = lw * fscale;
  double margin = 2.25 * fscale;

  _cfPDFToPDFPageRect rect = ungetRect(givenRect, handle, handle->rotation, handle->page);

  char boxcmd[1024];
  char buffer[64];

  snprintf(boxcmd, sizeof(boxcmd), "q\n");

  sprintf(buffer, "%.2f", line_width);  
  strncat(boxcmd, buffer, sizeof(boxcmd) - strlen(boxcmd) - 1);
  strncat(boxcmd, " w 0 G \n", sizeof(boxcmd) - strlen(boxcmd) - 1);

  sprintf(buffer, "%.2f", rect.left + margin);  
  strncat(boxcmd, buffer, sizeof(boxcmd) - strlen(boxcmd) - 1);
  strncat(boxcmd, " ", sizeof(boxcmd) - strlen(boxcmd) - 1);

  sprintf(buffer, "%.2f", rect.bottom + margin);  
  strncat(boxcmd, buffer, sizeof(boxcmd) - strlen(boxcmd) - 1);
  strncat(boxcmd, " ", sizeof(boxcmd) - strlen(boxcmd) - 1);

  sprintf(buffer, "%.2f", rect.right - rect.left + 2 * margin);  
  strncat(boxcmd, buffer, sizeof(boxcmd) - strlen(boxcmd) - 1);
  strncat(boxcmd, " ", sizeof(boxcmd) - strlen(boxcmd) - 1);

  sprintf(buffer, "%.2f", rect.top - rect.bottom - 2 * margin);  
  strncat(boxcmd, buffer, sizeof(boxcmd) - strlen(boxcmd) - 1);
  strncat(boxcmd, " re S \n", sizeof(boxcmd) - strlen(boxcmd) - 1);

  if (border & TWO) 
  {
    margin += 2 * fscale;
    sprintf(buffer, "%.2f", rect.left + margin);  
    strncat(boxcmd, buffer, sizeof(boxcmd) - strlen(boxcmd) - 1);
    strncat(boxcmd, " ", sizeof(boxcmd) - strlen(boxcmd) - 1);

    sprintf(buffer, "%.2f", rect.bottom + margin);  
    strncat(boxcmd, buffer, sizeof(boxcmd) - strlen(boxcmd) - 1);
    strncat(boxcmd, " ", sizeof(boxcmd) - strlen(boxcmd) - 1);

    sprintf(buffer, "%.2f", rect.right - rect.left - 2 * margin);  
    strncat(boxcmd, buffer, sizeof(boxcmd) - strlen(boxcmd) - 1);
    strncat(boxcmd, " ", sizeof(boxcmd) - strlen(boxcmd) - 1);

    sprintf(buffer, "%.2f", rect.top - rect.bottom - 2 * margin);  
    strncat(boxcmd, buffer, sizeof(boxcmd) - strlen(boxcmd) - 1);
    strncat(boxcmd, " re S \n", sizeof(boxcmd) - strlen(boxcmd) - 1);
  }
  strncat(boxcmd, "Q\n", sizeof(boxcmd) - strlen(boxcmd) - 1);

#ifdef DEBUG
  const char *pre = "%pdftopdf q\nq\n";
  const char *post = "%pdftopdf Q\nQ\n";
 
  pdfio_dict_t *stm1_dict = pdfioDictCreate(pdf);
  pdfio_obj_t *stm1_obj = pdfioFileCreateObj(pdf, stm1_dict);
  pdfio_stream_t *stm1 = pdfioObjCreateStream(stm1_obj, PDFIO_FILTER_NONE);
  if (stm1) 
  {
    pdfioStreamWrite(stm1, pre, strlen(pre));
    pdfioStreamClose(stm1); // Finalize the stream
  } 
  else 
    fprintf(stderr, "Failed to create PDF stream for pre content\n");
    
  char combined[2048]; // Ensure this is large enough
  snprintf(combined, sizeof(combined), "%s%s", post, boxcmd);

  pdfio_dict_t *stm2_dict = pdfioDictCreate(pdf);
  pdfio_obj_t *stm2_obj = pdfioFileCreateObj(pdfio_file_t *pdf, stm2_dict);
  pdfio_stream_t *stm2 = pdfioObjCreateStream(stm2_obj, PDFIO_FILTER_NONE);
  if (stm2) 
  {
    pdfioStreamWrite(stm2, combined, strlen(combined));
    pdfioStreamClose(stm2); // Finalize the stream
  } 
  else 
    fprintf(stderr, "Failed to create PDF stream for post content\n");


#else 
  pdfio_dict_t *stm_dict = pdfioDictCreate(pdf);
  pdfio_obj_t *stm_obj = pdfioFileCreateObj(pdf, stm_dict);
  pdfio_stream_t *stm = pdfioObjCreateStream(stm_obj, PDFIO_FILTER_NONE);
  if (stm) 
  {
    pdfioStreamWrite(stm, boxcmd, strlen(boxcmd));
    pdfioStreamClose(stm); // Finalize the stream
  } 
  else 
    fprintf(stderr, "Failed to create PDF stream for boxcmd content\n");
#endif
}
// }}}

pdftopdf_rotation_e 
_cfPDFToPDF_PDFioProcessor_crop(_cfPDFToPDF_PDFioProcessor *handle,
				const _cfPDFToPDFPageRect *cropRect,
				pdftopdf_rotation_e orientation, 
				pdftopdf_rotation_e param_orientation,
				pdftopdf_position_e xpos, pdftopdf_position_e ypos, 
				bool scale, bool autorotate,
				pdftopdf_doc_t *doc) // {{{
{
  pdftopdf_rotation_e save_rotate = _cfPDFToPDFGetRotate(handle->page);

  pdfio_dict_t *pageDict = pdfioObjGetDict(handle->page); 
  if (orientation == ROT_0 || orientation == ROT_180)
    pdfioDictSetNumber(pageDict, "Rotate", _cfPDFToPDFMakeRotate(ROT_90));
  else
    pdfioDictSetNumber(pageDict, "Rotate", _cfPDFToPDFMakeRotate(ROT_0));
  
  pdfio_rect_t trimBox = _cfPDFToPDFGetTrimBox(handle->page); 
  _cfPDFToPDFPageRect currpage = _cfPDFToPDFGetBoxAsRect(&trimBox);

  double width = currpage.right - currpage.left;
  double height = currpage.top - currpage.bottom;
  double pageWidth = cropRect->right - cropRect->left;
  double pageHeight = cropRect->top - cropRect->bottom;
  double final_w, final_h;

  pdftopdf_rotation_e pageRot = _cfPDFToPDFGetRotate(handle->page);
  if ((autorotate &&
       (((pageRot == ROT_0 || pageRot == ROT_180) && pageWidth <= pageHeight) ||
        ((pageRot == ROT_90 || pageRot == ROT_270) && pageWidth > pageHeight))) ||
      (!autorotate && (param_orientation == ROT_90 || param_orientation == ROT_270)))
  {
    double temp=pageHeight;
    pageHeight=pageWidth;
    pageWidth=temp;
  }

  if (scale)
  {
    if (width * pageHeight / pageWidth <= height)
    {
      final_w = width;
      final_h = width * pageHeight / pageWidth;
    }
    else
    {
      final_w = height * pageWidth / pageHeight;
      final_h = height;
    }
  }
  else
  {
    final_w = pageWidth;
    final_h = pageHeight;
  }

  if (doc->logfunc)
  {
    doc->logfunc(doc->logdata, 1, "cfFilterPDFToPDF: After Cropping: %lf %lf %lf %lf",
                 width, height, final_w, final_h);
  }

  double posw = (width - final_w) / 2;
  double posh = (height - final_h) / 2;

  if (xpos == LEFT)
    posw = 0;
  else if (xpos == RIGHT)
    posw *= 2;

  if (ypos == TOP)
    posh *= 2;
  else if (ypos == BOTTOM)
    posh = 0;

  currpage.left += posw;
  currpage.bottom += posh;
  currpage.top = currpage.bottom + final_h;
  currpage.right = currpage.left + final_w;

  pdfioDictSetRect(pageDict, "TrimBox", 
		  _cfPDFToPDFMakeBox(currpage.left, currpage.bottom, currpage.right, currpage.top));
  pdfioDictSetNumber(pageDict, "Rotate", _cfPDFToPDFMakeRotate(save_rotate));

  return _cfPDFToPDFGetRotate(handle->page);
}
// }}}

bool 
_cfPDFToPDF_PDFioProcessor_is_landscape(const _cfPDFToPDF_PDFioProcessor *handle, 
					pdftopdf_rotation_e orientation) // {{{
{
  pdftopdf_rotation_e save_rotate = _cfPDFToPDFGetRotate(handle->page);
  
  // Temporarily set the page rotation based on the orientation
  
  pdfio_dict_t *pageDict = pdfioObjGetDict(handle->page); 
  
  if (orientation == ROT_0 || orientation == ROT_180)
    pdfioDictSetNumber(pageDict, "Rotate", ROT_90);
  else
    pdfioDictSetNumber(pageDict, "Rotate", ROT_0);

  // Get the current page dimensions after rotation
  pdfio_rect_t trimBox = _cfPDFToPDFGetTrimBox(handle->page); 
  _cfPDFToPDFPageRect currpage = _cfPDFToPDFGetBoxAsRect(&trimBox);
  double width = currpage.right - currpage.left;
  double height = currpage.top - currpage.bottom;

  // Restore the original page rotation
  pdfioDictSetNumber(pageDict, "Rotate", save_rotate);

  // Determine if the page is in landscape orientation
  if (width > height)
    return true;
  return false;
}
// }}}

void 
_cfPDFToPDF_PDFioPageHandle_add_subpage(_cfPDFToPDF_PDFioProcessor *handle, 
				       	_cfPDFToPDF_PDFioProcessor *sub, 
					pdfio_file_t *pdf, 
					float xpos, float ypos, float scale, 
					const _cfPDFToPDFPageRect *crop) // {{{
{
  char xoname[64];
  snprintf(xoname, sizeof(xoname), "/X%d", (sub->no != -1) ? sub->no : ++handle->no);

  if (crop) 
  {
    _cfPDFToPDFPageRect pg = _cfPDFToPDF_PDFioProcessor_get_rect(sub);
    _cfPDFToPDFPageRect tmp = *crop;
    tmp.width = tmp.right - tmp.left;
    tmp.height = tmp.top - tmp.bottom;
    pdftopdf_rotation_e tempRotation = _cfPDFToPDFGetRotate(sub->page);
    _cfPDFToPDFPageRect_rotate_move(&tmp, pdftopdf_rotation_neg(tempRotation), tmp.width, tmp.height);

    if (pg.width < tmp.width)
      pg.right = pg.left + tmp.width;
    if (pg.height < tmp.height)
      pg.top = pg.bottom + tmp.height;

    _cfPDFToPDFPageRect rect = ungetRect(pg, sub, ROT_0, sub->page); 
    
    pdfio_rect_t *trimBox = _cfPDFToPDFMakeBox(rect.left, rect.bottom, rect.right, rect.top);

    // Set TrimBox in pdfio (adjust if pdfio allows this kind of modification)
    pdfio_dict_t *pageDict = pdfioObjGetDict(sub->page);
    pdfioDictSetRect(pageDict, "TrimBox", trimBox);
  }
  
  hashInsert(handle->xobjs, xoname,_cfPDFToPDFMakeXObject(pdf, sub->page));
  // Prepare transformation matrix
  _cfPDFToPDFMatrix mtx;
  _cfPDFToPDFMatrix_init(&mtx);
  _cfPDFToPDFMatrix_translate(&mtx, xpos, ypos);
  _cfPDFToPDFMatrix_scale(&mtx, scale, scale);
  _cfPDFToPDFMatrix_rotate(&mtx, sub->rotation);

  if (crop) {
    _cfPDFToPDFMatrix_translate(&mtx, crop->left, crop->bottom);
  }

  strcat(handle->content, "q\n  ");
  char matrix_string[128];
  _cfPDFToPDFMatrix_get_string(&mtx, matrix_string, 128);
  strcat(handle->content, matrix_string);
  strcat(handle->content, " cm\n  ");

  if (crop) {
    strcat(handle->content, " cm\n  ");
    char crop_string[128];
    snprintf(crop_string, sizeof(crop_string), "0 0 %.2f %.2f re W n\n", crop->right - crop->left, crop->top - crop->bottom);
    strcat(handle->content, crop_string);
  }

  strcat(handle->content, xoname);
  strcat(handle->content, " Do\nQ\n");
}
// }}}

void
_cfPDFToPDF_PDFioProcessor_mirror(_cfPDFToPDF_PDFioProcessor *handle,
				  pdfio_file_t *pdf)
{
  _cfPDFToPDFPageRect orig = _cfPDFToPDF_PDFioProcessor_get_rect(handle);
  if(_cfPDFToPDF_PDFioProcessor_is_existing(handle))
  {
    char xoname[10];
    snprintf(xoname, sizeof(xoname), "/X%d", 1); // Assuming 'no' is 1 for example

    // Get the page to mirror (this would be replaced by actual pdfio page handling)
    pdfio_obj_t *subpage = _cfPDFToPDF_PDFioProcessor_get(handle);;

    _cfPDFToPDF_PDFioProcessor_create_newMode(handle, pdf, orig.width, orig.height); 
    // Reinitialize the handle with new dimensions
    hashInsert(handle->xobjs, xoname,_cfPDFToPDFMakeXObject(pdf, subpage));

    char temp_content[1024];
    snprintf(temp_content, sizeof(temp_content), "%s Do\n", xoname);
    strcat(handle->content, temp_content);
  }

  static const char *pre = "%pdftopdf cm\n";
  char mrcmd[100];
  snprintf(mrcmd, sizeof(mrcmd), "-1 0 0 1 %.2f 0 cm\n", orig.right);

  // Insert the mirroring matrix at the beginning of the content
  size_t new_len = strlen(pre) + strlen(mrcmd) + strlen(handle->content) + 1;
  char *new_content = (char *)malloc(new_len);
  snprintf(new_content, new_len, "%s%s%s", pre, mrcmd, handle->content);

  free(handle->content); // Free the old content
  handle->content = new_content;
}


void
_cfPDFToPDF_PDFioProcessor_rotate(_cfPDFToPDF_PDFioProcessor *handle,
				  pdftopdf_rotation_e rot)
{
  handle->rotation = rot;
}

void 
_cfPDFToPDF_PDFioProcessor_add_label(_cfPDFToPDF_PDFioProcessor *handle,
				     const _cfPDFToPDFPageRect *rect, 
				     const char *label)
{
  
  _cfPDFToPDFPageRect rect_mod = ungetRect(rect);

  if (rect_mod.left > rect_mod.right || rect_mod.bottom > rect_mod.top)
  {
    fprintf(stderr, "Invalid rectangle dimensions!\n");
    return;
  }
  pdfio_dict_t *font_dict = pdfioDictCreate(pdf);
  pdfioDictSetName(font_dict, "Type", "Font");
  pdfioDictSetName(font_dict, "Subtype", "Type1");
  pdfioDictSetName(font_dict, "Name", "pagelabel-font");
  pdfioDictSetName(font_dict, "BaseFont", "Helvetica");

  // Add font to the document as an indirect object
  pdfio_obj_t *font_obj = pdfioFileCreateObj(pdf, font_dict);

  // Get the Resources dictionary from the page
  pdfio_dict_t *resources = pdfioObjGetDict(page);
  if (resources == NULL)
  {
    resources = pdfioDictCreate(pdf);
    pdfioDictSetDict(resources, "Font", pdfioDictCreate(pdf));
  }

  // Get or create the Font dictionary in Resources
  pdfio_dict_t *font_resources = pdfioDictGetDict(resources, "Font");
  if (font_resources == NULL)
  {
    font_resources = pdfioDictCreate(pdf);
    pdfioDictSetDict(resources, "Font", font_resources);
  }

  // Add the pagelabel-font to the Font dictionary in Resources
  pdfioDictSetObj(font_resources, "pagelabel-font", font_obj);

  // Finally, replace the Resources key in the page with updated Resources
  pdfioDictSetDict(resources, "Resources", resources);

  double margin = 2.25;
  double height = 12;

  // Start creating the PDF content commands (simplified for pdfio)
  char boxcmd[1024] = "q\n";

  // White filled rectangle (top)
  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "1 1 1 rg\n%f %f %f %f re f\n",
           rect_mod.left + margin, rect_mod.top - height - 2 * margin,
           rect_mod.right - rect_mod.left - 2 * margin, height + 2 * margin);

    // White filled rectangle (bottom)
  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "%f %f %f %f re f\n",
           rect_mod.left + margin, rect_mod.bottom + height + margin,
           rect_mod.right - rect_mod.left - 2 * margin, height + 2 * margin);

  // Black outline (top)
  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "0 0 0 RG\n%f %f %f %f re S\n",
           rect_mod.left + margin, rect_mod.top - height - 2 * margin,
           rect_mod.right - rect_mod.left - 2 * margin, height + 2 * margin);

  // Black outline (bottom)
  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "%f %f %f %f re S\n",
           rect_mod.left + margin, rect_mod.bottom + height + margin,
           rect_mod.right - rect_mod.left - 2 * margin, height + 2 * margin);

  // Black text (top)
  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "0 0 0 rg\nBT\n/f1 12 Tf\n%f %f Td\n(%s) Tj\nET\n",
           rect_mod.left + 2 * margin, rect_mod.top - height - margin, label);

  // Black text (bottom)
  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "BT\n/f1 12 Tf\n%f %f Td\n(%s) Tj\nET\n",
           rect_mod.left + 2 * margin, rect_mod.bottom + height + 2 * margin, label);

  // End the graphic context
  strcat(boxcmd, "Q\n");
 
  const char *pre = "%pdftopdf q\nq\n";
  char post[256]; // Make sure the buffer is large enough for the combined string

  // Combine the "post" string with the passed boxcmd
  snprintf(post, sizeof(post), "%%pdftopdf Q\nQ\n%s", boxcmd);

  // Create the first stream (pre)
  pdfio_stream_t *stream1 = pdfioPageOpenStream(page, PDFIO_FILTER_FLATE, true);
  pdfioStreamPuts(stream1, pre);
  pdfioStreamClose(stream1);

    // Create the second stream (post + boxcmd)
  pdfio_stream_t *stream2 = pdfioPageOpenStream(page, PDFIO_FILTER_FLATE, true);
  pdfioStreamPuts(stream2, post);
  pdfioStreamClose(stream2);

  // Add the streams as page contents
  pdfioPageDictAddFont(page, "Contents", stream1); // before content
  pdfioPageDictAddFont(page, "Contents", stream2); // after content
}

void 
debug(_cfPDFToPDF_PDFioProcessor *handle, 
      const _cfPDFToPDFPageRect *rect, 
      float xpos, float ypos) 
{
  if (!_cfPDFToPDF_PDFioProcessor_is_existing(handle)) 
    return;

  // append to the content stream 
  append_debug_box(handle->content, rect, xpos, ypos);
}

/*
// Define the _cfPDFToPDFQPDFProcessor structure
struct _cfPDFToPDFQPDFProcessor {
    pdfio_file_t *pdf;  // pdfio object to represent the PDF document
    _cfPDFToPDFQPDFPageHandle **orig_pages;
    int num_pages;
    bool hasCM;
    char *extraheader;  // Dynamic string for comments
};

void
_cfPDFToPDFQPDFProcessor_close_file(_cfPDFToPDFQPDFProcessor *processor)
{
    if (processor->pdf)
    {
        pdfioFileClose(processor->pdf);
        processor->pdf = NULL;
    }
    processor->hasCM = false;
}

bool
_cfPDFToPDFQPDFProcessor_load_file(_cfPDFToPDFQPDFProcessor *processor,
                                   FILE *f, pdftopdf_doc_t *doc,
                                   pdftopdf_arg_ownership_e take,
                                   int flatten_forms)
{
    _cfPDFToPDFQPDFProcessor_close_file(processor);

    if (!f) {
        // Handle error in C style
        return false;
    }

    // Simulate opening the file with PDFIO
    processor->pdf = pdfioFileOpen(f, NULL);
    if (!processor->pdf) {
        if (take == CF_PDFTOPDF_TAKE_OWNERSHIP)
            fclose(f);
        return false;
    }

    // Call start function to initialize the pages
    _cfPDFToPDFQPDFProcessor_start(processor, flatten_forms);
    return true;
}

bool
_cfPDFToPDFQPDFProcessor_load_filename(_cfPDFToPDFQPDFProcessor *processor,
                                       const char *name, pdftopdf_doc_t *doc,
                                       int flatten_forms)
{
    _cfPDFToPDFQPDFProcessor_close_file(processor);

    processor->pdf = pdfioFileOpenFile(name, NULL);
    if (!processor->pdf) {
        return false;
    }

    // Call start function to initialize the pages
    _cfPDFToPDFQPDFProcessor_start(processor, flatten_forms);
    return true;
}

void
_cfPDFToPDFQPDFProcessor_start(_cfPDFToPDFQPDFProcessor *processor, int flatten_forms)
{
    DEBUG_assert(processor->pdf);

    processor->orig_pages = pdfioFileGetPages(processor->pdf, &(processor->num_pages));
    // Remove the pages from the PDF document for processing
    for (int i = 0; i < processor->num_pages; i++) {
        pdfioPageRemove(processor->orig_pages[i]);
    }

    // Initialize other PDF data as necessary (e.g., remove defunct keys)
    pdfioFileRemoveKey(processor->pdf, "/PageMode");
    pdfioFileRemoveKey(processor->pdf, "/Outlines");
    pdfioFileRemoveKey(processor->pdf, "/OpenAction");
    pdfioFileRemoveKey(processor->pdf, "/PageLabels");
}

bool
_cfPDFToPDFQPDFProcessor_check_print_permissions(_cfPDFToPDFQPDFProcessor *processor, pdftopdf_doc_t *doc)
{
    if (!processor->pdf) {
        return false;
    }

    // Simulate permission checking with PDFIO
    return pdfioFileAllowPrint(processor->pdf);
}

_cfPDFToPDFQPDFPageHandle **
_cfPDFToPDFQPDFProcessor_get_pages(_cfPDFToPDFQPDFProcessor *processor,
                                   pdftopdf_doc_t *doc, int *num_pages)
{
    if (!processor->pdf) {
        *num_pages = 0;
        return NULL;
    }

    *num_pages = processor->num_pages;
    return processor->orig_pages;
}

_cfPDFToPDFQPDFPageHandle *
_cfPDFToPDFQPDFProcessor_new_page(_cfPDFToPDFQPDFProcessor *processor,
                                  float width, float height,
                                  pdftopdf_doc_t *doc)
{
    if (!processor->pdf) {
        return NULL;
    }

    return _cfPDFToPDFQPDFPageHandle_new_new(processor->pdf, width, height);
}

void
_cfPDFToPDFQPDFProcessor_add_page(_cfPDFToPDFQPDFProcessor *processor,
                                  _cfPDFToPDFQPDFPageHandle *page, bool front)
{
    DEBUG_assert(processor->pdf);
    if (front) {
        pdfioFileInsertPage(processor->pdf, page->page, 0);  // Insert at the front
    } else {
        pdfioFileAppendPage(processor->pdf, page->page);  // Append at the end
    }
}

void
_cfPDFToPDFQPDFProcessor_multiply(_cfPDFToPDFQPDFProcessor *processor,
                                  int copies, bool collate)
{
    DEBUG_assert(processor->pdf);
    DEBUG_assert(copies > 0);

    _cfPDFToPDFQPDFPageHandle **pages = _cfPDFToPDFQPDFProcessor_get_pages(processor, NULL, &processor->num_pages);

    if (collate) {
        for (int i = 1; i < copies; i++) {
            for (int j = 0; j < processor->num_pages; j++) {
                pdfioFileAppendPage(processor->pdf, pages[j]->page);
            }
        }
    } else {
        for (int j = 0; j < processor->num_pages; j++) {
            for (int i = 1; i < copies; i++) {
                pdfioFileAppendPage(processor->pdf, pages[j]->page);
            }
        }
    }
}

void
_cfPDFToPDFQPDFProcessor_auto_rotate_all(_cfPDFToPDFQPDFProcessor *processor,
                                         bool dst_lscape,
                                         pdftopdf_rotation_e normal_landscape)
{
    DEBUG_assert(processor->pdf);

    for (int i = 0; i < processor->num_pages; i++) {
        _cfPDFToPDFQPDFPageHandle *page = processor->orig_pages[i];

        pdftopdf_rotation_e src_rot = pdfioPageGetRotation(page->page);
        _cfPDFToPDFPageRect rect = _cfPDFToPDFQPDFPageHandle_get_rect(page);

        bool src_lscape = rect.width > rect.height;
        if (src_lscape != dst_lscape) {
            pdfioPageSetRotation(page->page, src_rot + normal_landscape);
        }
    }
}

void
_cfPDFToPDFQPDFProcessor_add_cm(_cfPDFToPDFQPDFProcessor *processor,
                                const char *defaulticc, const char *outputicc)
{
    DEBUG_assert(processor->pdf);

    if (pdfioFileHasOutputIntent(processor->pdf)) {
        return;  // Nothing to do
    }

    // Simulate adding ICC profile and output intent with PDFIO
    pdfioFileAddICCProfile(processor->pdf, defaulticc);
    pdfioFileAddOutputIntent(processor->pdf, outputicc);

    processor->hasCM = true;
}

void
_cfPDFToPDFQPDFProcessor_set_comments(_cfPDFToPDFQPDFProcessor *processor,
                                      const char **comments, int num_comments)
{
    if (processor->extraheader) {
        free(processor->extraheader);
    }

    // Concatenate comments into one string
    int total_len = 0;
    for (int i = 0; i < num_comments; i++) {
        total_len += strlen(comments[i]) + 1;
    }

    processor->extraheader = malloc(total_len + 1);
    processor->extraheader[0] = '\0';

    for (int i = 0; i < num_comments; i++) {
        strcat(processor->extraheader, comments[i]);
        strcat(processor->extraheader, "\n");
    }
}

void
_cfPDFToPDFQPDFProcessor_emit_file(_cfPDFToPDFQPDFProcessor *processor,
                                   FILE *dst, pdftopdf_doc_t *doc,
                                   pdftopdf_arg_ownership_e take)
{
    if (!processor->pdf) {
        return;
    }

    // Simulate writing the PDF to a file with PDFIO
    pdfioFileWrite(processor->pdf, dst, processor->hasCM, processor->extraheader);

    if (take == CF_PDFTOPDF_TAKE_OWNERSHIP) {
        fclose(dst);
    }
}

void
_cfPDFToPDFQPDFProcessor_emit_filename(_cfPDFToPDFQPDFProcessor *processor,
                                       const char *name, pdftopdf_doc_t *doc)
{
    if (!processor->pdf) {
        return;
    }

    // Special case: name == NULL -> stdout
    FILE *output = name ? fopen(name, "wb") : stdout;

    // Simulate writing the PDF to a file with PDFIO
    pdfioFileWrite(processor->pdf, output, processor->hasCM, processor->extraheader);

    if (name) {
        fclose(output);
    }
}

bool
_cfPDFToPDFQPDFProcessor_has_acro_form(_cfPDFToPDFQPDFProcessor *processor)
{
    if (!processor->pdf) {
        return false;
    }

    // Simulate checking for an AcroForm in the PDF with PDFIO
    return pdfioFileHasAcroForm(processor->pdf);
}

*/
