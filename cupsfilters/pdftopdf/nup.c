//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "nup-private.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "cupsfilters/debug-internal.h"

void 
_cfPDFToPDFNupParameters_init(_cfPDFToPDFNupParameters *nupParams) // {{{
{
  nupParams->nupX = 1;
  nupParams->nupY = 1;
  nupParams->width = NAN;
  nupParams->height = NAN;
  nupParams->landscape = false;
  nupParams->first = X;
  nupParams->xstart = LEFT;
  nupParams->ystart = TOP;
  nupParams->xalign = CENTER;
  nupParams->yalign = CENTER;
}
// }}}

void 
_cfPDFToPDFNupParameters_dump(const _cfPDFToPDFNupParameters *nupParams, 
			      pdftopdf_doc_t *doc) // {{{
{
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, "cfFilterPDFToPDF: NupX: %d, NupY: %d, "
				 "width: %f, height: %f",
                     		 nupParams->nupX, nupParams->nupY, 
				 nupParams->width, nupParams->height);

  int opos = -1, 
      fpos = -1, 
      spos = -1;

  if (nupParams->xstart == LEFT)
    fpos = 0; 
  else if (nupParams->xstart == RIGHT)
    fpos = 1;

  if (nupParams->ystart == LEFT)
    spos = 0;
  else if (nupParams->ystart == RIGHT)
    spos = 1;

  if (nupParams->first == X)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		      		     "cfFilterPDFToPDF: First Axis: X");
    opos = 0;
  }
  else if (nupParams->first == Y)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		    		   "cfFilterPDFToPDF: First Axis: Y");
    opos = 2;
    int temp = fpos;
    fpos = spos;
    spos = temp;
  }

  if ((opos == -1) || (fpos == -1) || (spos == -1))
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                         	   "cfFilterPDFToPDF: Bad Spec: %d; start: %d, %d",
                         	   nupParams->first, nupParams->xstart, nupParams->ystart);
  }
  else
  {
    static const char *order[4] = {"lr", "rl", "bt", "tb"};
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                        	   "cfFilterPDFToPDF: Order: %s%s",
                         	   order[opos + fpos], 
				   order[(opos + 2) % 4 + spos]);
  }

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		  		 "cfFilterPDFToPDF: Alignment:");
    _cfPDFToPDFPositionAndAxisDump(nupParams->xalign, X, doc);
    _cfPDFToPDFPositionAndAxisDump(nupParams->yalign, Y, doc);
}
// }}}

bool 
_cfPDFToPDFNupParameters_possible(int nup) // {{{
{
  return ((nup >= 1) && (nup <= 16) &&
          ((nup != 5) && (nup != 7) && (nup != 11) && (nup != 13) &&
           (nup != 14)));
}
// }}}

void 
_cfPDFToPDFNupParameters_preset(int nup, 
				_cfPDFToPDFNupParameters *ret) // {{{
{
  switch (nup)
  {
  case 1:
    ret->nupX = 1;
    ret->nupY = 1;
    break;
  case 2:
    ret->nupX = 2;
    ret->nupY = 1;
    ret->landscape = true;
    break;
  case 3:
    ret->nupX = 3;
    ret->nupY = 1;
    ret->landscape = true;
    break;
  case 4:
    ret->nupX = 2;
    ret->nupY = 2;
    break;
  case 6:
    ret->nupX = 3;
    ret->nupY = 2;
    ret->landscape = true;
    break;
  case 8:
    ret->nupX = 4;
    ret->nupY = 2;
    ret->landscape = true;
    break;
  case 9:
    ret->nupX = 3;
    ret->nupY = 3;
    break;
  case 10:
    ret->nupX = 5;
    ret->nupY = 2;
    ret->landscape = true;
    break;
  case 12:
    ret->nupX = 3;
    ret->nupY = 4;
    break;
  case 15:
    ret->nupX = 5;
    ret->nupY = 3;
    ret->landscape = true;
    break;
  case 16:
    ret->nupX = 4;
    ret->nupY = 4;
    break;
  }
}
// }}}

void 
_cfPDFToPDFNupState_init(_cfPDFToPDFNupState *state, 
			 const _cfPDFToPDFNupParameters *param) // {{{
{
  state->param = *param;
  state->in_pages = 0;
  state->out_pages = 0;
  state->nup = param->nupX * param->nupY;
  state->subpage = state->nup;
}
// }}}

void 
_cfPDFToPDFNupState_reset(_cfPDFToPDFNupState *state) // {{{
{
  state->in_pages = 0;
  state->out_pages = 0;
  state->subpage = state->nup;
}
// }}}

void 
_cfPDFToPDFNupPageEdit_dump(const _cfPDFToPDFNupPageEdit *edit, 
			    pdftopdf_doc_t *doc) // {{{
{
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
	      		         "cfFilterPDFToPDF: xpos: %f, ypos: %f, scale: %f",
				 edit->xpos, edit->ypos, edit->scale);
  _cfPDFToPDFPageRect_dump(&edit->sub, doc);
}
// }}}

typedef struct {
  int first;
  int second;
} int_pair;

static int_pair 
_cfPDFToPDFNupState_convert_order(const _cfPDFToPDFNupState *state, 
				  int subpage) // {{{
{
  int subx, suby;
  if (state->param.first == X)
  {
    subx = subpage % state->param.nupX;
    suby = subpage / state->param.nupX;
  }
  else
  {
    subx = subpage / state->param.nupY;
    suby = subpage % state->param.nupY;
  }

  subx = (state->param.nupX - 1) * (state->param.xstart + 1) / 2 - state->param.xstart * subx;
  suby = (state->param.nupY - 1) * (state->param.ystart + 1) / 2 - state->param.ystart * suby;
  
  int_pair result;
  result.first = subx;
  result.second = suby;
  
  return result;
}
// }}}

static float 
lin(pdftopdf_position_e pos, float size) // {{{
{
  if (pos == -1)
    return 0;
  else if (pos == 0)
    return size / 2;
  else if (pos == 1)
    return size;
  return size * (pos + 1) / 2;
}
// }}}

void 
_cfPDFToPDFNupState_calculate_edit(const _cfPDFToPDFNupState *state, 
				   int subx, int suby, 
				   _cfPDFToPDFNupPageEdit *ret) // {{{
{
  const float width = state->param.width / state->param.nupX;
  const float height = state->param.height / state->param.nupY;

  ret->xpos = subx * width;
  ret->ypos = suby * height;

  const float scalex = width / ret->sub.width;
  const float scaley = height / ret->sub.height;
  float subwidth = ret->sub.width * scaley;
  float subheight = ret->sub.height * scalex;

  if (scalex > scaley)
  {
    ret->scale = scaley;
    subheight = height;
    ret->xpos += lin(state->param.xalign, width - subwidth);
  }
  else
  {
    ret->scale = scalex;
    subwidth = width;
    ret->ypos += lin(state->param.yalign, height - subheight);
  }

  ret->sub.left = ret->xpos;
  ret->sub.bottom = ret->ypos;
  ret->sub.right = ret->sub.left + subwidth;
  ret->sub.top = ret->sub.bottom + subheight;
}
// }}}

bool 
_cfPDFToPDFNupState_next_page(_cfPDFToPDFNupState *state, 
			      float in_width, float in_height, 
			      _cfPDFToPDFNupPageEdit *ret) // {{{
{
  state->in_pages++;
  state->subpage++;
  if (state->subpage >= state->nup)
  {
    state->subpage = 0;
    state->out_pages++;
  }

  ret->sub.width = in_width;
  ret->sub.height = in_height;

  int_pair *sub = (int_pair*)malloc(sizeof(int_pair));
  *sub = _cfPDFToPDFNupState_convert_order(state, state->subpage);
  _cfPDFToPDFNupState_calculate_edit(state, sub->first, sub->second, ret);
  
  free(sub);
  return (state->subpage == 0);
}
// }}}

static int_pair 
parsePosition(char a, char b) // {{{
{
  a |= 0x20; // make lowercase
  b |= 0x20;
  if ((a == 'l') && (b == 'r'))
    return (int_pair){X, LEFT};
  else if ((a == 'r') && (b == 'l'))
    return (int_pair){X, RIGHT};
  else if ((a == 't') && (b == 'b'))
    return (int_pair){Y, TOP};
  else if ((a == 'b') && (b == 't'))
    return (int_pair){Y, BOTTOM};
  return (int_pair){X, CENTER};
}
// }}}

bool 
_cfPDFToPDFParseNupLayout(const char *val, 
			  _cfPDFToPDFNupParameters *ret) // {{{
{
  int_pair pos0 = parsePosition(val[0], val[1]);
  if (pos0.second == CENTER)
    return false;
  int_pair pos1 = parsePosition(val[2], val[3]);
  if ((pos1.second == CENTER) || (pos0.first == pos1.first))
    return false;

  ret->first = pos0.first;
  if (ret->first == X)
  {
    ret->xstart = pos0.second;
    ret->ystart = pos1.second;
  }
  else
  {
    ret->xstart = pos1.second;
    ret->ystart = pos0.second;
  }

  return (val[4] == 0); 
}
// }}}

