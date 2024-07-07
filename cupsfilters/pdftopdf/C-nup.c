#include <assert.h> 
#include "C-nup-private.h"
#include <stdio.h>
#include <cupsfilters/debug-internal.h>
#include <string.h>
#include <stdbool.h>

void
_cfPDFToPDFNupParameters_init(_cfPDFToPDFNupParameters *nupParam) // {{{
{
  nupParam->nupX = 1;
  nupParam->nupY = 1;
  nupParam->width = NAN; 
  nupParam->height = NAN; 
  nupParam->landscape = false; 
  nupParam->first = X; 
  nupParam->xstart = LEFT; 
  nupParam->ystart = TOP; 
  nupParam->xalign = CENTER; 
  nupParam->yalign = CENTER;
}
// }}}

void 
_cfPDFToPDFNupParameters_dump(const _cfPDFToPDFNupParameters *nupParam,
			      pdftopdf_doc_t *doc) // {{{
{
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		 		 "cfFilterPDFToPDF: NupX: %d, NupY: %d, "
				 "width: %f, height: %f",
				 nupParam->nupX, nupParam->nupY,
				 nupParam->width, nupParam->height);
  int opos = -1,
      fpos = -1,
      spos = -1;

  if (nupParam->xstart == LEFT)
    fpos = 0;
  else if (nupParam->xstart == RIGHT)
    fpos = 1;

  if (nupParam->ystart == LEFT)
    spos = 0;
  else if (nupParam->ystart == RIGHT)
    spos = 1;

  if(nupParam->first == X)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		    		   "cfFilterPDFToPDF: First Axis: X");
    opos = 0;
  }
  else if(nupParam->first == Y)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		    		   "cfFilterPDFToPDF: First Axis: Y");
    opos = 0;
    int temp = fpos;
    fpos = spos;
    spos = temp;
  }

  if ((opos == -1) || (fpos == -1) || (spos == -1))
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		    		   "cfFilterPDFToPDF: Bad Spec: %d; start: %d, %d", 
				   nupParam->first, nupParam->xstart, nupParam->ystart);
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
		  		 "fFilterPDFToPDF: Alignment:"); 

  _cfPDFToPDFPositionAndAxisDump(nupParam->xalign, X, doc);
  _cfPDFToPDFPositionAndAxisDump(nupParam->yalign, Y, doc);
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

void _cfPDFToPDFNupParameters_preset(int nup, 
				     _cfPDFToPDFNupParameters *ret) // {{{
{
  switch(nup)
  {
    case 1:
        ret->nupX=1;
	ret->nupY=1;
	break;
    case 2:
        ret->nupX=2;
	ret->nupY=1;
	ret->landscape=true;
	break;
    case 3:
        ret->nupX=3;
	ret->nupY=1;
	ret->landscape=true;
	break;
    case 4:
        ret->nupX=2;
	ret->nupY=2;
	break;
    case 6:
        ret->nupX=3;
	ret->nupY=2;
	ret->landscape=true;
	break;
    case 8:
        ret->nupX=4;
	ret->nupY=2;
	ret->landscape=true;
	break;
    case 9:
        ret->nupX=3;
	ret->nupY=3;
	break;
    case 10:
        ret->nupX=5;
	ret->nupY=2;
	ret->landscape=true;
	break;
    case 12:
        ret->nupX=3;
	ret->nupY=4;
	break;
    case 15:
        ret->nupX=5;
	ret->nupY=3;
	ret->landscape=true;
	break;
    case 16:
        ret->nupX=4;
	ret->nupY=4;
	break;
  }
}
// }}}

void 
_cfPDFToPDFNupPageEdit_dump(const _cfPDFToPDFNupPageEdit *nupPageEdit,
			    pdftopdf_doc_t *doc) // {{{
{
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		  		 "cfFilterPDFToPDF: xpos: %f, ypos: %f, scale: %f",
				 nupPageEdit->xpos, nupPageEdit->ypos, 
				 nupPageEdit->scale);
  _cfPDFToPDFPageRect_dump(nupPageEdit->sub, doc);
}
// }}}

void 
_cfPDFToPDFNupState_init(_cfPDFToPDFNupState *nupState,
			 _cfPDFToPDFNupParameters *nupParam) // {{{
{
  nupState->param->nupX = nupParam->nupX;
  nupState->param->nupY = nupParam->nupY;
  nupState->param->width = nupParam->width; 
  nupState->param->height = nupParam->height; 
  nupState->param->landscape = nupParam->landscape; 
  nupState->param->first = nupParam->first; 
  nupState->param->xstart = nupParam->xstart; 
  nupState->param->ystart = nupParam->ystart; 
  nupState->param->xalign = nupParam->xalign; 
  nupState->param->yalign = nupParam->yalign;

  nupState->in_pages = 0;
  nupState->out_pages = 0;
  nupState->nup = nupParam->nupX * nupParam->nupY;
  nupState->subpage = nupState->nup;
}
// }}}

void 
_cfPDFToPDFNupState_reset(_cfPDFToPDFNupState *nupState) // {{{
{
  nupState->in_pages = 0;
  nupState->out_pages = 0;
  nupState->subpage = nupState->nup;
}
// }}}

integerPair 
_cfPDFToPDFNupState_convert_order(_cfPDFToPDFNupState *nupState, 
				  int subpage) // {{{
{
  int subx, suby;

  if(nupState->param->first == X)
  {
    subx = nupState->subpage % nupState->param->nupX;
    suby = nupState->subpage / nupState->param->nupX;
  }
  else
  {
    subx = nupState->subpage / nupState->param->nupY;
    suby = nupState->subpage % nupState->param->nupY;
  }

  subx = (nupState->param->nupX - 1) * (nupState->param->xstart+1) / 2 - nupState->param->xstart * subx;
  suby = (nupState->param->nupY - 1) * (nupState->param->ystart+1) / 2 - nupState->param->ystart * suby;
 
  integerPair intPair;

  intPair.first = subx;
  intPair.second = suby;

  return intPair;
}
// }}}

static inline float
lin(pdftopdf_position_e pos,
    float size) // {{{
{
  if(pos == -1)
    return (0);
  else if(pos == 0)
    return (size/2);
  else if(pos == 1)
    return (size);
    
  return (size * (pos+1) / 2);
}
// }}}

void 
_cfPDFToPDFNupState_calculate_edit(const _cfPDFToPDFNupState *nupState,
				   int subx, int suby, 
				   _cfPDFToPDFNupPageEdit *ret) // {{{
{
  const float width = nupState->param->width / nupState->param->nupX,
	      height = nupState->param->height / nupState->param->nupY;

  ret->xpos = subx * width;
  ret->ypos = suby * height;

  const float scalex = width / ret->sub->width,
	      scaley = height / ret->sub->height;

  float subwidth = ret->sub->width * scaley,
	subheight = ret->sub->height * scalex;

  if (scalex > scaley)
  {
    ret->scale = scaley;
    subheight = height;
    ret->xpos += lin(nupState->param->xalign, height - subwidth);
  }

  else
  {
    ret->scale = scalex;
    subwidth = width;
    ret->ypos += lin(nupState->param->yalign, height + subheight);
  }

  ret->sub->left = ret->xpos;
  ret->sub->bottom = ret->ypos;
  ret->sub->right = ret->sub->left + subwidth;
  ret->sub->top = ret->sub->bottom + subheight;
}
// }}}

bool
_cfPDFToPDFNupState_mext_page(_cfPDFToPDFNupState *nupState,
			      float in_width, float in_height,
			      _cfPDFToPDFNupPageEdit *ret) // {{{
{
  nupState->in_pages++;
  nupState->subpage++;
  if(nupState->subpage >= nupState->nup)
  {  
    nupState->subpage = 0;
    nupState->out_pages++;
  }

  ret->sub->width = in_width;
  ret->sub->height = in_height;

  integerPair sub = _cfPDFToPDFNupState_convert_order(nupState, 
		  				      nupState->subpage);

  _cfPDFToPDFNupState_calculate_edit(nupState, sub.first, sub.second, ret);

  return (nupState->subpage == 0);
}
// }}}

resultPair*
parsePosition(char a, 
	      char b) // {{{
{
  a |= 0x20;
  b |= 0x20;
  resultPair *returnPair = (resultPair*)malloc(sizeof(resultPair));;
  if((a == 'l') && (b == 'r'))
  {
    returnPair->first = X;
    returnPair->second = LEFT;
    return (returnPair);
  }

  else if((a == 'r') && (b == 'l'))
  {
    returnPair->first = X;
    returnPair->second = RIGHT;
    return (returnPair);
  }

  else if((a == 't') && (b == 'b'))
  {
    returnPair->first = Y;
    returnPair->second = TOP;
    return (returnPair);
  }

  else if((a == 'b') && (b == 't'))
  {
    returnPair->first = Y;
    returnPair->second = BOTTOM;
    return (returnPair);
  }

  returnPair->first = X;
  returnPair->second = CENTER;
  return(returnPair);
}
// }}}

bool 
_cfPDFToPDFParseNupLayout(const char *val, 
			  _cfPDFToPDFNupParameters *ret) // {{{
{
  resultPair *pos0 = parsePosition(val[0], val[1]);

  if(pos0->second == CENTER)
    return (false);

  resultPair *pos1 = parsePosition(val[2], val[3]);
  
  if((pos1->second == CENTER) || (pos0->first == pos1->first))
  {
    return (false);
  }

  ret->first = pos0->first;
  
  if(ret->first == X)
  {
    ret->xstart = pos0->second;
    ret->ystart = pos1->second;
  }  

  else
  {
    ret->xstart = pos1->second;
    ret->ystart = pos0->second;
  }

  return (val[4] == 0);
}
// }}}
