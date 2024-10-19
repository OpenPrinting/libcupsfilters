//
//Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "C-pptypes-private.h"
#include <stdio.h>
#include <stdlib.h>
#include "cupsfilters/debug-internal.h"  

void 
_cfPDFToPDFPositionDump(pdftopdf_position_e pos, 
			pdftopdf_doc_t *doc) // {{{
{
  static const char *pstr[3] = {"Left/Bottom", "Center", "Right/Top"};
  if ((pos < LEFT) || (pos > RIGHT))
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                 	           "cfFilterPDFToPDF: (bad position: %d)", pos);
  }
  else
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                       	           "cfFilterPDFToPDF: %s", pstr[pos+1]);
  }
}
// }}}

void 
_cfPDFToPDFPositionAndAxisDump(pdftopdf_position_e pos, 
			       pdftopdf_axis_e axis,
			       pdftopdf_doc_t *doc) // {{{
{
  if ((pos < LEFT) || (pos > RIGHT)) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: Position %s: (bad position: %d)",
                                   (axis == X) ? "X" : "Y", pos);
    return;
  }

  if (axis == X) 
  {
    static const char *pxstr[3] = {"Left", "Center", "Right"};
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: Position X: %s", pxstr[pos + 1]);
  }
    
  else 
  {
    static const char *pystr[3] = {"Bottom", "Center", "Top"};
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
        	                   "cfFilterPDFToPDF: Position Y: %s", pystr[pos + 1]);
  
  }
}
// }}}

void 
_cfPDFToPDFRotationDump(pdftopdf_rotation_e rot, 
			pdftopdf_doc_t *doc) // {{{
{
  static const char *rstr[4] = {"0 deg", "90 deg", "180 deg", "270 deg"}; // CCW

  if ((rot < ROT_0) || (rot > ROT_270)) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: Rotation(CCW): (bad rotation: %d)", rot);
  } 

  else 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: Rotation(CCW): %s", rstr[rot]);
  }
}
// }}}

pdftopdf_rotation_e 
pdftopdf_rotation_add(pdftopdf_rotation_e lhs, 
	              pdftopdf_rotation_e rhs) // {{{
{
  return (pdftopdf_rotation_e)(((int)lhs + (int)rhs) % 4);
}
// }}}

pdftopdf_rotation_e 
pdftopdf_rotation_sub(pdftopdf_rotation_e lhs, 
		      pdftopdf_rotation_e rhs) // {{{
{
  return (pdftopdf_rotation_e)((((int)lhs - (int)rhs) % 4 + 4) % 4);
}
// }}}

pdftopdf_rotation_e 
pdftopdf_rotation_neg(pdftopdf_rotation_e rhs) // {{{
{
    return (pdftopdf_rotation_e)((4 - (int)rhs) % 4);
}
// }}}

void 
_cfPDFToPDFBorderTypeDump(pdftopdf_border_type_e border, 
			  pdftopdf_doc_t *doc) // {{{
{
  if ((border < NONE) || (border == 1) || (border > TWO_THICK)) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: Border: (bad border: %d)", border);
        
  } 
  else 
  {
    static const char *bstr[6] = 
    {"None", NULL, "one thin", "one thick", "two thin", "two thick"};
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: Border: %s", bstr[border]);
  }
}
// }}}

void 
_cfPDFToPDFPageRect_init(_cfPDFToPDFPageRect *rect) // {{{
{
  rect->top = NAN;
  rect->left = NAN;
  rect->right = NAN;
  rect->bottom = NAN;
  rect->width = NAN;
  rect->height = NAN;
}
// {{{

void 
swap_float(float *a, float *b) // {{{
{
  float temp = *a;
  *a = *b;
  *b = temp;
}
// }}}

void 
_cfPDFToPDFPageRect_rotate_move(_cfPDFToPDFPageRect *rect,
	      		        pdftopdf_rotation_e r, 
				float pwidth, float pheight) // {{{
{
  #if 1
  if (r >= ROT_180) 
  {
    swap_float(&rect->top, &rect->bottom);
    swap_float(&rect->left, &rect->right);
  }
   
  if ((r == ROT_90) || (r == ROT_270)) 
  {
    const float tmp = rect->bottom;
    rect->bottom = rect->left;
    rect->left = rect->top;
    rect->top = rect->right;
    rect->right = tmp;

    swap_float(&rect->width, &rect->height);
    swap_float(&pwidth, &pheight);
  }

  if ((r == ROT_90) || (r == ROT_180)) 
  {
    rect->left = pwidth - rect->left;
    rect->right = pwidth - rect->right;
  }

  if ((r == ROT_270) || (r == ROT_180)) 
  {
    rect->top = pheight - rect->top;
    rect->bottom = pheight - rect->bottom;
  }

  #else
  switch(r)
  {
    case ROT_0: // no-op
     	break;
    case ROT_90:
      	const float tmp0 = bottom;
       	bottom = left;
       	left = pheight - top;
       	top = right;
       	right = pheight - tmp0;

       	swap_float(&width, &height);
       	break;

    case ROT_180:
   	const float tmp1 = left;
        left = pwidth - right;
        right = pwidth - tmp1;

       	const float tmp2 = top;
       	top = pheight - bottom;
       	bottom = pheight - tmp2;
       	break;

    case ROT_270:
       	const float tmp3 = top;
       	top = pwidth - left;
       	left = bottom;
       	bottom = pwidth - right;
       	right = tmp3;

       	swap_float(&width, &height);
       	break;

  }
  #endif
}
// }}}

void 
_cfPDFToPDFPageRect_scale(_cfPDFToPDFPageRect *rect, 
			  float mult) // {{{
{
  if (mult == 1.0)
    return;

  rect->bottom *= mult;
  rect->left *= mult;
  rect->top *= mult;
  rect->right *= mult;

  rect->width *= mult;
  rect->height *= mult;
}
// }}}

void 
_cfPDFToPDFPageRect_translate(_cfPDFToPDFPageRect *rect, 
			      float tx, 
			      float ty) // {{{
{
  rect->left += tx;
  rect->bottom += ty;
  rect->right += tx;
  rect->top += ty;
}
// }}}

void 
_cfPDFToPDFPageRect_set(_cfPDFToPDFPageRect *rect, 
			const _cfPDFToPDFPageRect *rhs) // {{{
{
  if (!isnan(rhs->top))
    rect->top = rhs->top;

  if (!isnan(rhs->left))
    rect->left = rhs->left;

  if (!isnan(rhs->right))
    rect->right = rhs->right;

  if (!isnan(rhs->bottom))
    rect->bottom = rhs->bottom;
}
// }}}

void 
_cfPDFToPDFPageRect_dump(const _cfPDFToPDFPageRect *rect, 
			 pdftopdf_doc_t *doc) // {{{
{
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
            "cfFilterPDFToPDF: top: %f, left: %f, right: %f, bottom: %f, "
            "width: %f, height: %f",
            rect->top, rect->left, rect->right, rect->bottom,
            rect->width, rect->height);
    
}
// }}}
