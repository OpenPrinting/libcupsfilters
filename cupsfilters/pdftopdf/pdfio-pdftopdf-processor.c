//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "pdfio.h"  // Replace with appropriate PDFIO headers
#include "pdfio-content.h"
#include "pdfio-pdftopdf-private.h"
#include "pdfio-tools-private.h"
#include "pdfio-xobject-private.h"
#include "processor.h"
#include "pdfio-cm-private.h"

#define DEBUG_assert(x) do { if (!(x)) abort(); } while (0)

//map definition
unsigned int 
hash(const char *key) // {{{
{
  unsigned int hash = 0;
  while (*key) {
    hash = (hash << 5) + *key++;
  }
  return hash % HASH_TABLE_SIZE;
}
// }}}

// Initialize a new hash table
HashTable*
hashCreate_hash_table()  // {{{
{
  HashTable *table = malloc(sizeof(HashTable));
  for (int i = 0; i < HASH_TABLE_SIZE; i++) 
  {
    table->buckets[i] = NULL;
  }
  table->count = 0;  // Initialize the count of filled elements
  return table;
}
// }}}

// Create a new key-value pair
KeyValuePair*
create_key_value_pair(const char *key, pdfio_obj_t *value) // {{{
{
  KeyValuePair *new_pair = malloc(sizeof(KeyValuePair));
  new_pair->key = strdup(key);  // Duplicate the key string
  new_pair->value = value;
  new_pair->next = NULL;
  return new_pair;
}
// }}}

// Insert a key-value pair into the hash table
void 
hashInsert(HashTable *table, const char *key, pdfio_obj_t *value) // {{{
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
// }}}

// Retrieve a value by key from the hash table
pdfio_obj_t*
hashGet(HashTable *table, const char *key) // {{{
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
// }}}

// Get the number of elements currently filled in the hash table
int 
hashGet_filled_count(HashTable *table) // {{{
{
  return table->count;
}
// }}}

// Free the hash table
void 
hashFree_hash_table(HashTable *table) // {{{
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
// }}}

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
_cfPDFToPDFPageHandle_existingMode(_cfPDFToPDFPageHandle *handle, 
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
}
// }}}

void 
_cfPDFToPDFPageHandle_create_newMode(_cfPDFToPDFPageHandle *handle, 
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
}
// }}}

void 
_cfPDFToPDFPageHandle_destroy(_cfPDFToPDFPageHandle *handle) // {{{
{
  free(handle->xobjs);
  free(handle->content); 
}
// }}}

bool 
_cfPDFToPDFPageHandle_is_existing(_cfPDFToPDFPageHandle *handle) // {{{
{
  return (handle->content == NULL || 
		  strlen((const char *)handle->content) == 0);
}
// }}}


_cfPDFToPDFPageRect 
_cfPDFToPDFPageHandle_get_rect(const _cfPDFToPDFPageHandle *handle) // {{{
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
_cfPDFToPDFPageHandle_get(_cfPDFToPDFPageHandle *handle) // {{{
{
  pdfio_dict_t *resources, *contents_dict;
  pdfio_stream_t *contents_stream;
  pdfio_obj_t *ret = handle->page;

  if (!_cfPDFToPDFPageHandle_is_existing(handle)) 
  {
    resources = pdfioDictGetDict(pdfioObjGetDict(ret), "/Resources");
    if (resources)
    {
      char name_buffer[handle->xobjs->count];   
      int xobj_index = 0; 
      for (int i = 0; i < HASH_TABLE_SIZE && xobj_index < handle->xobjs->count; i++) 
      {
        KeyValuePair *current = handle->xobjs->buckets[i];

        while (current != NULL) 
	{
          snprintf(name_buffer, sizeof(name_buffer), "/X%d", xobj_index + 1);
          pdfioDictSetObj(resources, name_buffer, current->value); 
          current = current->next;
          xobj_index++;
        }
      }
      pdfioDictSetDict(resources, "/XObject", resources);
    }

    contents_stream = pdfioPageOpenStream(ret, 0, true);
    if (contents_stream)
    {
      pdfioStreamPuts(contents_stream, "Q\n"); 
      pdfioStreamClose(contents_stream);
    }

    contents_dict = pdfioDictGetDict(pdfioObjGetDict(ret), "/Contents");
    if (contents_dict)
    {
      pdfioDictSetNull(contents_dict, "/Filter");  
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
          const _cfPDFToPDFPageHandle *ph,
          pdftopdf_rotation_e rotation,
          pdfio_obj_t *page)  // {{{
{
  
  _cfPDFToPDFPageRect pg1 = _cfPDFToPDFPageHandle_get_rect(ph);
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
_cfPDFToPDFPageHandle_add_border_rect(_cfPDFToPDFPageHandle *handle,   
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
    pdfioStreamClose(stm1);
  } 
  else 
    fprintf(stderr, "Failed to create PDF stream for pre content\n");
    
  char combined[2048]; 
  snprintf(combined, sizeof(combined), "%s%s", post, boxcmd);

  pdfio_dict_t *stm2_dict = pdfioDictCreate(pdf);
  pdfio_obj_t *stm2_obj = pdfioFileCreateObj(pdf, stm2_dict);
  pdfio_stream_t *stm2 = pdfioObjCreateStream(stm2_obj, PDFIO_FILTER_NONE);
  if (stm2) 
  {
    pdfioStreamWrite(stm2, combined, strlen(combined));
    pdfioStreamClose(stm2);
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
    pdfioStreamClose(stm);
  } 
  else 
    fprintf(stderr, "Failed to create PDF stream for boxcmd content\n");
#endif
}
// }}}

pdftopdf_rotation_e 
_cfPDFToPDFPageHandle_crop(_cfPDFToPDFPageHandle *handle,
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
_cfPDFToPDFPageHandle_is_landscape(const _cfPDFToPDFPageHandle *handle, 
				   pdftopdf_rotation_e orientation) // {{{
{
  pdftopdf_rotation_e save_rotate = _cfPDFToPDFGetRotate(handle->page);
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

  pdfioDictSetNumber(pageDict, "Rotate", save_rotate);

  if (width > height)
    return true;
  return false;
}
// }}}

void 
_cfPDFToPDFPageHandle_add_subpage(_cfPDFToPDFPageHandle *handle, 
				  _cfPDFToPDFPageHandle *sub, 
				  pdfio_file_t *pdf, 
				  float xpos, float ypos, float scale, 
				  const _cfPDFToPDFPageRect *crop) // {{{
{
  char xoname[64];
  snprintf(xoname, sizeof(xoname), "/X%d", (sub->no != -1) ? sub->no : ++handle->no);

  if (crop) 
  {
    _cfPDFToPDFPageRect pg = _cfPDFToPDFPageHandle_get_rect(sub);
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

    pdfio_dict_t *pageDict = pdfioObjGetDict(sub->page);
    pdfioDictSetRect(pageDict, "TrimBox", trimBox);
  }
  
  hashInsert(handle->xobjs, xoname,_cfPDFToPDFMakeXObject(pdf, sub->page));
  
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
_cfPDFToPDFPageHandle_mirror(_cfPDFToPDFPageHandle *handle,
			     pdfio_file_t *pdf)
{
  _cfPDFToPDFPageRect orig = _cfPDFToPDFPageHandle_get_rect(handle);
  if(_cfPDFToPDFPageHandle_is_existing(handle))
  {
    char xoname[10];
    snprintf(xoname, sizeof(xoname), "/X%d", handle->no);

    pdfio_obj_t *subpage = _cfPDFToPDFPageHandle_get(handle);;

    _cfPDFToPDFPageHandle_create_newMode(handle, pdf, orig.width, orig.height); 
    hashInsert(handle->xobjs, xoname,_cfPDFToPDFMakeXObject(pdf, subpage));

    char temp_content[1024];
    snprintf(temp_content, sizeof(temp_content), "%s Do\n", xoname);
    strcat(handle->content, temp_content);
  }

  static const char *pre = "%pdftopdf cm\n";
  char mrcmd[100];
  snprintf(mrcmd, sizeof(mrcmd), "-1 0 0 1 %.2f 0 cm\n", orig.right);

  size_t new_len = strlen(pre) + strlen(mrcmd) + strlen(handle->content) + 1;
  char *new_content = (char *)malloc(new_len);
  snprintf(new_content, new_len, "%s%s%s", pre, mrcmd, handle->content);

  free(handle->content); // Free the old content
  handle->content = new_content;
}


void
_cfPDFToPDFPageHandle_rotate(_cfPDFToPDFPageHandle *handle,
			     pdftopdf_rotation_e rot)
{
  handle->rotation = rot;
}

void 
_cfPDFToPDFPageHandle_add_label(_cfPDFToPDFPageHandle *handle,
				pdfio_file_t *pdf,
				const _cfPDFToPDFPageRect *rect, 
				const char *label)
{
  
  _cfPDFToPDFPageRect rect_mod = ungetRect(*rect, handle, handle->rotation, handle->page);

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

  pdfio_obj_t *font_obj = pdfioFileCreateObj(pdf, font_dict);

  pdfio_dict_t *resources = pdfioObjGetDict(handle->page);
  if (resources == NULL)
  {
    resources = pdfioDictCreate(pdf);
    pdfioDictSetDict(resources, "Font", pdfioDictCreate(pdf));
  }

  pdfio_dict_t *font_resources = pdfioDictGetDict(resources, "Font");
  if (font_resources == NULL)
  {
    font_resources = pdfioDictCreate(pdf);
    pdfioDictSetDict(resources, "Font", font_resources);
  }
  pdfioDictSetObj(font_resources, "pagelabel-font", font_obj);
  pdfioDictSetDict(resources, "Resources", resources);

  double margin = 2.25;
  double height = 12;
  
  char boxcmd[1024] = "q\n";

  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "1 1 1 rg\n%f %f %f %f re f\n",
           rect_mod.left + margin, rect_mod.top - height - 2 * margin,
           rect_mod.right - rect_mod.left - 2 * margin, height + 2 * margin);

  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "%f %f %f %f re f\n",
           rect_mod.left + margin, rect_mod.bottom + height + margin,
           rect_mod.right - rect_mod.left - 2 * margin, height + 2 * margin);

  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "0 0 0 RG\n%f %f %f %f re S\n",
           rect_mod.left + margin, rect_mod.top - height - 2 * margin,
           rect_mod.right - rect_mod.left - 2 * margin, height + 2 * margin);

  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "%f %f %f %f re S\n",
           rect_mod.left + margin, rect_mod.bottom + height + margin,
           rect_mod.right - rect_mod.left - 2 * margin, height + 2 * margin);

  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "0 0 0 rg\nBT\n/f1 12 Tf\n%f %f Td\n(%s) Tj\nET\n",
           rect_mod.left + 2 * margin, rect_mod.top - height - margin, label);

  snprintf(boxcmd + strlen(boxcmd), sizeof(boxcmd) - strlen(boxcmd),
           "BT\n/f1 12 Tf\n%f %f Td\n(%s) Tj\nET\n",
           rect_mod.left + 2 * margin, rect_mod.bottom + height + 2 * margin, label);

  strcat(boxcmd, "Q\n");
 
  const char *pre = "%pdftopdf q\nq\n";
  char post[256];

  snprintf(post, sizeof(post), "%%pdftopdf Q\nQ\n%s", boxcmd);

  pdfio_stream_t *stream1 = pdfioPageOpenStream(handle->page, PDFIO_FILTER_FLATE, true);
  pdfioStreamPuts(stream1, pre);
  pdfioStreamClose(stream1);

  pdfio_stream_t *stream2 = pdfioPageOpenStream(handle->page, PDFIO_FILTER_FLATE, true);
  pdfioStreamPuts(stream2, post);
  pdfioStreamClose(stream2);

}

void 
_cfPDFToPDFPageHandle_debug(_cfPDFToPDFPageHandle *handle, 
		  	    const _cfPDFToPDFPageRect *rect, 
			    float xpos, float ypos) 
{
  if (!_cfPDFToPDFPageHandle_is_existing(handle)) 
    return;

  append_debug_box(handle->content, rect, xpos, ypos);
}

void _cfPDFToPDF_PDFioProcessor_close_file(_cfPDFToPDF_PDFioProcessor *handle)
{
    if (handle->pdf != NULL)
    {
        pdfioFileClose(handle->pdf);
        handle->pdf = NULL;
    }
    handle->hasCM = false;
}


bool 
_cfPDFToPDF_PDFioProcessor_load_file(_cfPDFToPDF_PDFioProcessor *handle, 
				      FILE *f, pdftopdf_doc_t *doc, 
				      pdftopdf_arg_ownership_e take, int flatten_forms)
{
  _cfPDFToPDF_PDFioProcessor_close_file(handle);

  if (!f)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
      		      		   "load_file(NULL, ...) not allowed");
    return false;
  }

  handle->pdf = pdfioFileCreate("tempfile", NULL, NULL, NULL, NULL, NULL);

  
  if (handle->pdf == NULL)
  {
    if (take == CF_PDFTOPDF_TAKE_OWNERSHIP)
    {
      fclose(f);
    }
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		    		   "Failed to open PDF file.");
    return false;
  }

  switch (take)
  {
    case CF_PDFTOPDF_WILL_STAY_ALIVE:
      break;

    case CF_PDFTOPDF_TAKE_OWNERSHIP:
      if (fclose(f) != 0)
      {
        if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
				       "Failed to close file after loading.");
        return false;
      }
      break;

    case CF_PDFTOPDF_MUST_DUPLICATE:
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		      		     "CF_PDFTOPDF_MUST_DUPLICATE is not supported.");
      return false;
  }


  //_cfPDFToPDF_PDFioProcessor_start(handle, flatten_forms);
  return true;
}

bool _cfPDFToPDF_PDFioProcessor_load_filename(_cfPDFToPDF_PDFioProcessor *handle,
					      const char *name, 
					      pdftopdf_doc_t *doc, 
					      int flatten_forms)
{
  _cfPDFToPDF_PDFioProcessor_close_file(handle);

  handle->pdf = pdfioFileOpen(name, NULL, NULL, NULL, NULL);
  if (!handle->pdf) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, 3, 
                                   "cfFilterPDFToPDF: load_filename failed: Could not open file %s", 
				   name);
    return false;
  }

  //_cfPDFToPDF_PDFioProcessor_start(handle, flatten_forms);
  return true;
}


bool 
_cfPDFToPDF_PDFioProcessor_check_print_permissions(_cfPDFToPDF_PDFioProcessor *handle, 
					           pdftopdf_doc_t *doc)
{
  if (!handle->pdf)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR,
                                   "cfFilterPDFToPDF: No PDF loaded");
    return false;
  }

  int permissions = pdfioFileGetPermissions(handle->pdf, NULL);
   
  if ((permissions & PDFIO_PERMISSION_PRINT_HIGH) || (permissions & PDFIO_PERMISSION_PRINT))
    return true;
    
  return false;
}

pdfio_obj_t**
get_all_pages(pdfio_file_t *pdf)
{
    size_t num_pages = pdfioFileGetNumPages(pdf);
    pdfio_obj_t **pages = malloc(sizeof(pdfio_obj_t *) * num_pages);
    for (size_t i = 0; i < num_pages; i++)
    {
        pages[i] = pdfioFileGetPage(pdf, i);
    }
    return pages;
}

/*
void 
_cfPDFToPDF_PDFioProcessor_start(_cfPDFToPDF_PDFioProcessor *proc,
				 int flatten_forms)
{
  if (!proc->pdf)
  {
    fprintf(stderr, "No PDF loaded.\n");
    return;
  }

  if (flatten_forms)
  {
    pdfio_dict_t *catalog = pdfioFileGetCatalog(proc->pdf);
    pdfio_obj_t *acroForm = pdfioDictGetObj(catalog, "AcroForm");

    if (acroForm)
    {
      pdfio_dict_t *acroForm_dict = pdfioObjGetDict(acroForm);
      pdfio_array_t *fields = pdfioDictGetArray(acroForm_dict, "Fields");
      size_t num_fields = pdfioArrayGetSize(fields);

      // Iterating over each field and generating render as static content
      for (size_t i = 0; i < num_fields; i++)
      {
        pdfio_obj_t *field = pdfioArrayGetObj(fields, i);
        pdfio_dict_t *field_dict = pdfioObjGetDict(field);
	const char *field_type = pdfioDictGetName(field_dict, "FT");  // Field type
        const char *field_value = pdfioDictGetString(field_dict, "V");  // Field value

        if (field_type && strcmp(field_type, "Tx") == 0) 
        {
          double x = 100.0, y = 200.0;  
          pdfio_stream_t *stream = pdfioPageOpenStream(handle->page, 0, true); 

          pdfioContentTextMoveTo(stream, x, y);  
          pdfioContentTextShow(stream, 1, field_value);   // Render the text

          pdfioStreamClose(stream);  // Closing the stream after render
        }
      }
      pdfioDictSetNull(catalog, "AcroForm");
    }
  }


    // Get all pages
  proc->orig_pages = get_all_pages(proc->pdf);
  size_t num_pages = pdfioFileGetNumPages(proc->pdf);

  // Remove (unlink) all pages
  for (size_t i = 0; i < num_pages; i++)
  {
//    pdfio_remove_page(pdf, orig_pages[i]);
  }

  pdfio_dict_t *root = pdfioFileGetCatalog(proc->pdf);
  pdfioDictSetNull(root, "PageMode");
  pdfioDictSetNull(root, "Outlines");
  pdfioDictSetNull(root, "OpenAction");
  pdfioDictSetNull(root, "PageLabels");

  pdfioFileClose(proc->pdf);
}
*/

_cfPDFToPDFPageHandle**
_cfPDFToPDF_PDFioProcessor_get_pages(_cfPDFToPDF_PDFioProcessor *handle, 
	                             pdftopdf_doc_t *doc, int *out_len) 
{
  _cfPDFToPDFPageHandle **ret = NULL;

  if (handle->orig_pages_size == 0 || handle->orig_pages == NULL) 
  {
    if (doc->logfunc) 
    {
      doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		   "cfFilterPDFToPDF: No PDF loaded");
    }
    *out_len = 0;
    return ret; 
  }

  int len = handle->orig_pages_size;
  *out_len = len;

  ret = (_cfPDFToPDFPageHandle **)malloc(len * sizeof(_cfPDFToPDFPageHandle *));
  if (!ret) 
  {
    fprintf(stderr, "Memory allocation failed for pages array\n");
  }

  for (int i = 0; i < len; i++) 
  {
    ret[i] = (_cfPDFToPDFPageHandle *)malloc(sizeof(_cfPDFToPDFPageHandle));
    if (!ret[i]) 
    {
      fprintf(stderr, "Memory allocation failed for page handle %d\n", i + 1);
      for (int j = 0; j < i; j++) 
      {
        free(ret[j]);
      }
      free(ret);
    }

    ret[i]->page = handle->orig_pages[i];
    //ret[i]->orig_pages_size = i + 1;
  }

  return ret;
}

_cfPDFToPDFPageHandle*
_cfPDFToPDF_PDFioProcessor_new_page(_cfPDFToPDF_PDFioProcessor *handle,
	 			    float width, float height, 
				    pdftopdf_doc_t *doc) 
{
  if (!handle->pdf)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR, 
		                   "cfFilterPDFToPDF: No PDF loaded"); 
    return NULL;
  }

  _cfPDFToPDFPageHandle *page_handle = (_cfPDFToPDFPageHandle *)malloc(sizeof(_cfPDFToPDFPageHandle));
   
  _cfPDFToPDFPageHandle_create_newMode(page_handle, handle->pdf, width, height);

  return page_handle;
}

void
_cfPDFToPDF_PDFioProcessor_multiply(_cfPDFToPDF_PDFioProcessor *handle,
				    int copies, bool collate)
{
  int num_pages=pdfioFileGetNumPages(handle->pdf);
 
  pdfio_obj_t **pages = (pdfio_obj_t **)malloc(num_pages * sizeof(pdfio_obj_t *));
  
  for (int i = 0; i < num_pages; i++) 
  {
    pages[i] = pdfioFileGetPage(handle->pdf, i); 
  }
  
  if (collate) {
    for (int iA = 1; iA < copies; iA++) 
    {
      for (int iB = 0; iB < num_pages; iB++) 
      {
        pdfioPageCopy(handle->pdf, pages[iB]);
      }
    }
  }

  else 
  {
    for (int iB = 0; iB < num_pages; iB++) 
    { 
      for (int iA = 1; iA < copies; iA++) 
      {
        pdfioPageCopy(handle->pdf, pages[iB]); 
      }
    }
  }
}

void 
_cfPDFToPDF_PDFioProcessor_auto_rotate_all(_cfPDFToPDF_PDFioProcessor *handle,
					   bool dst_lscape,
					   pdftopdf_rotation_e normal_landscape)
{
  const int len = handle->orig_pages_size; 

  for (int iA = 0; iA < len; iA ++)
  {
    pdfio_obj_t *page = handle->orig_pages[iA]; 
    
    pdftopdf_rotation_e src_rot = _cfPDFToPDFGetRotate(page);
    
    pdfio_rect_t trimBox = _cfPDFToPDFGetTrimBox(page); 
    _cfPDFToPDFPageRect ret = _cfPDFToPDFGetBoxAsRect(&trimBox);

    _cfPDFToPDFPageRect_rotate_move(&ret, src_rot, ret.width, ret.height);

    const bool src_lscape = (ret.width > ret.height); 
    
    if (src_lscape != dst_lscape)
    {
      pdftopdf_rotation_e rotation = normal_landscape; 
      
      pdfio_dict_t *pageDict = pdfioObjGetDict(page);
      pdfioDictSetNumber(pageDict, "Rotate", 
		         _cfPDFToPDFMakeRotate(src_rot + rotation));
    }
  }
}

void 
_cfPDFToPDF_PDFioProcessor_add_cm(_cfPDFToPDF_PDFioProcessor *handle,
				  const char *defaulticc, const char *outputicc) 
{
  if (_cfPDFToPDFHasOutputIntent(handle->pdf))
    return;

  pdfio_obj_t *srcicc = _cfPDFToPDFSetDefaultICC(handle->pdf, defaulticc);
  _cfPDFToPDFAddDefaultRGB(handle->pdf, srcicc); 
  _cfPDFToPDFAddOutputIntent(handle->pdf, outputicc);
  
  handle->hasCM = true; 
}

void 
_cfPDFToPDF_PDFioProcessor_set_comments(_cfPDFToPDF_PDFioProcessor *handle,
		               		char **comments, int num_comments)
{
  if (handle->extraheader) 
  {
    free(handle->extraheader); 
  }

  handle->extraheader = (char *)malloc(1);
  handle->extraheader[0] = '\0'; 

  int total_length = 0;
  for (int i = 0; i < num_comments; i++) 
  {
    total_length += strlen(comments[i]) + 1; 
  }

  handle->extraheader = (char *)realloc(handle->extraheader, total_length + 1);

  for (int i = 0; i < num_comments; i++) 
  {
    strcat(handle->extraheader, comments[i]);
    strcat(handle->extraheader, "\n");
  }
}

void 
_cfPDFToPDF_PDFioProcessor_emit_file(_cfPDFToPDF_PDFioProcessor *handle,
				     FILE *f, pdftopdf_doc_t *doc, 
				     pdftopdf_arg_ownership_e take) 
{
}

void 
_cfPDFToPDF_PDFioProcessor_emit_filename(_cfPDFToPDF_PDFioProcessor *handle,
				         const char *name, pdftopdf_doc_t *doc) 
{
}

bool 
_cfPDFToPDF_PDFioProcessor_has_acro_form(_cfPDFToPDF_PDFioProcessor *handle)
{
  if (!handle->pdf) 
  {
    return false;
  }

  pdfio_dict_t *root = pdfioFileGetCatalog(handle->pdf);

  if (!pdfioDictGetDict(root, "AcroForm")) 
    return false;
  return true;
}
