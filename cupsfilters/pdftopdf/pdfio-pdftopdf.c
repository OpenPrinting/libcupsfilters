//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-pdftopdf-private.h"
#include "pdfio-tools-private.h"
#include "cupsfilters/debug-internal.h"

#include <stdio.h>
#include <stdlib.h>

_cfPDFToPDFPageRect 
_cfPDFToPDFGetBoxAsRect(pdfio_rect_t *box) // {{{
{
  _cfPDFToPDFPageRect ret;

  ret.left = box->x1;
  ret.bottom = box->y1;
  ret.right = box->x2;
  ret.top = box->y2;

  ret.width = ret.right - ret.left;
  ret.height = ret.top - ret.bottom;

  return ret;
}
// }}}

pdfio_rect_t* 
_cfPDFToPDFGetRectAsBox(_cfPDFToPDFPageRect *rect) // {{{
{
  return (_cfPDFToPDFMakeBox(rect->left, rect->bottom, rect->right, rect->top));
}
// }}}

pdftopdf_rotation_e 
_cfPDFToPDFGetRotate(pdfio_obj_t *page) // {{{
{
  pdfio_dict_t *pageDict = pdfioObjGetDict(page);
  double rotate = pdfioDictGetNumber(pageDict, "Rotate");
  if (!rotate)
    return ROT_0;

  double rot = fmod(rotate, 360.0);
  if (rot < 0)
    rot += 360.0;

  if (rot == 90.0) // CW 
    return ROT_270; // CCW
  else if (rot == 180.0)
    return ROT_180;
  else if (rot == 270.0)
    return ROT_90;
  else if (rot != 0.0)
    fprintf(stderr, "Unexpected /Rotate value: %f\n", rot);
    
  return ROT_0;
}
// }}}

double 
_cfPDFToPDFGetUserUnit(pdfio_obj_t *page) // {{{
{
  pdfio_dict_t *pageDict = pdfioObjGetDict(page);
  double userUnit = pdfioDictGetNumber(pageDict, "UserUnit");
  if(!userUnit)
    return 1.0;
  return userUnit;
}
// }}}

double 
_cfPDFToPDFMakeRotate(pdftopdf_rotation_e rot) // {{{ 
{ 
    switch (rot)                                                                                          
    { 
        case ROT_0:
	    return 0;
        case ROT_90:
	    return 270.0;
        case ROT_180:
	    return 180.0;
        case ROT_270:
	    return 90.0;
        default:
            fprintf(stderr, "Bad rotation value\n");
	    return NAN;
    }
}
// }}}

void 
_cfPDFToPDFMatrix_init(_cfPDFToPDFMatrix *matrix) // {{{
{
  matrix->ctm[0] = 1.0;
  matrix->ctm[1] = 0.0;
  matrix->ctm[2] = 0.0;
  matrix->ctm[3] = 1.0;
  matrix->ctm[4] = 0.0;
  matrix->ctm[5] = 0.0;
}
// }}}

void 
_cfPDFToPDFMatrix_init_with_array(_cfPDFToPDFMatrix *matrix, 
				  pdfio_array_t *array) // {{{
{
  if (pdfioArrayGetSize(array) != 6)
    fprintf(stderr, "Not a ctm matrix");
    
  for (int iA = 0; iA < 6; iA ++)
    matrix->ctm[iA] = pdfioArrayGetNumber(array, iA);
}
// }}}

void 
_cfPDFToPDFMatrix_rotate(_cfPDFToPDFMatrix *matrix, 
			 pdftopdf_rotation_e rot) // {{{
{
  double tmp[6];
  memcpy(tmp, matrix->ctm, sizeof(tmp));

  switch (rot) 
  {
    case ROT_0:
      break;
    case ROT_90:
      matrix->ctm[0] = tmp[2];
      matrix->ctm[1] = tmp[3];
      matrix->ctm[2] = -tmp[0];
      matrix->ctm[3] = -tmp[1];
      break;
    case ROT_180:
      matrix->ctm[0] = -tmp[0];
      matrix->ctm[3] = -tmp[3];
      break;
    case ROT_270:
      matrix->ctm[0] = -tmp[2];
      matrix->ctm[1] = -tmp[3];
      matrix->ctm[2] = tmp[0];
      matrix->ctm[3] = tmp[1];
      break;
    default:
    DEBUG_assert(0);
  }
}
// }}}

void 
_cfPDFToPDFMatrix_rotate_move(_cfPDFToPDFMatrix *matrix, 
			      pdftopdf_rotation_e rot, 
			      double width, double height) // {{{
{
  _cfPDFToPDFMatrix_rotate(matrix, rot);
  switch (rot) 
  {
    case ROT_0:
      break;
    case ROT_90:
      _cfPDFToPDFMatrix_translate(matrix, width, 0);
      break;
    case ROT_180:
      _cfPDFToPDFMatrix_translate(matrix, width, height);
      break;
    case ROT_270:
      _cfPDFToPDFMatrix_translate(matrix, 0, height);
      break;
  }
}
// }}}

void 
_cfPDFToPDFMatrix_rotate_rad(_cfPDFToPDFMatrix *matrix, 
			     double rad) // {{{
{
  _cfPDFToPDFMatrix tmp;
  _cfPDFToPDFMatrix_init(&tmp);

  tmp.ctm[0] = cos(rad);
  tmp.ctm[1] = sin(rad);
  tmp.ctm[2] = -sin(rad);
  tmp.ctm[3] = cos(rad);

  _cfPDFToPDFMatrix_multiply(matrix, &tmp);
}
// }}}

void 
_cfPDFToPDFMatrix_translate(_cfPDFToPDFMatrix *matrix, 
			    double tx, double ty) // {{{
{
  matrix->ctm[4] += matrix->ctm[0] * tx + matrix->ctm[2] * ty;
  matrix->ctm[5] += matrix->ctm[1] * tx + matrix->ctm[3] * ty;
}
// }}}

void 
_cfPDFToPDFMatrix_scale(_cfPDFToPDFMatrix *matrix, 
			double sx, double sy) // {{{
{
  matrix->ctm[0] *= sx;
  matrix->ctm[1] *= sx;
  matrix->ctm[2] *= sy;
  matrix->ctm[3] *= sy;
}
// }}}

void 
_cfPDFToPDFMatrix_multiply(_cfPDFToPDFMatrix *lhs, 
			   const _cfPDFToPDFMatrix *rhs) // {{{
{
  double tmp[6];
  memcpy(tmp, lhs->ctm, sizeof(tmp));

  lhs->ctm[0] = tmp[0] * rhs->ctm[0] + tmp[2] * rhs->ctm[1];
  lhs->ctm[1] = tmp[1] * rhs->ctm[0] + tmp[3] * rhs->ctm[1];

  lhs->ctm[2] = tmp[0] * rhs->ctm[2] + tmp[2] * rhs->ctm[3];
  lhs->ctm[3] = tmp[1] * rhs->ctm[2] + tmp[3] * rhs->ctm[3];

  lhs->ctm[4] = tmp[0] * rhs->ctm[4] + tmp[2] * rhs->ctm[5] + tmp[4];
  lhs->ctm[5] = tmp[1] * rhs->ctm[4] + tmp[3] * rhs->ctm[5] + tmp[5];
}
// }}}

void 
_cfPDFToPDFMatrix_get(const _cfPDFToPDFMatrix *matrix, 
		      double *array) // {{{
{
  memcpy(array, matrix->ctm, sizeof(double) * 6);
}
// }}}

void 
_cfPDFToPDFMatrix_get_string(const _cfPDFToPDFMatrix *matrix, 
			     char *buffer, size_t bufsize) // {{{
{
  snprintf(buffer, bufsize, "%f %f %f %f %f %f", 
	   matrix->ctm[0], matrix->ctm[1], matrix->ctm[2], 
	   matrix->ctm[3], matrix->ctm[4], matrix->ctm[5]);
  buffer[bufsize - 1] = '\0'; // Ensure null-termination
}
// }}}
