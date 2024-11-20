//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pptypes-private.h"
#include <math.h>
#include <string.h>
#include <pdfio.h>

#ifndef _CUPS_FILTERS_PDFTOPDF_PDFIO_PDFTOPDF_H
#define _CUPS_FILTERS_PDFTOPDF_PDFIO_PDFTOPDF_H

// helper functions

_cfPDFToPDFPageRect _cfPDFToPDFGetBoxAsRect(pdfio_rect_t *box);
pdfio_rect_t* _cfPDFToPDFGetRectAsBox(_cfPDFToPDFPageRect *rect);

// Note that PDF specification is CW, but our Rotation is CCW
pdftopdf_rotation_e _cfPDFToPDFGetRotate(pdfio_obj_t *page);
double _cfPDFToPDFMakeRotate(pdftopdf_rotation_e rot);	

double _cfPDFToPDFGetUserUnit(pdfio_obj_t *page);

// PDF CTM
typedef struct {
    double ctm[6];
} _cfPDFToPDFMatrix;

void _cfPDFToPDFMatrix_init(_cfPDFToPDFMatrix *matrix); // identity
void _cfPDFToPDFMatrix_init_with_array(_cfPDFToPDFMatrix *matrix, pdfio_array_t *array);

void _cfPDFToPDFMatrix_rotate(_cfPDFToPDFMatrix *matrix, pdftopdf_rotation_e rot);
void _cfPDFToPDFMatrix_rotate_move(_cfPDFToPDFMatrix *matrix, pdftopdf_rotation_e rot, double width, double height);
void _cfPDFToPDFMatrix_rotate_rad(_cfPDFToPDFMatrix *matrix, double rad);

void _cfPDFToPDFMatrix_translate(_cfPDFToPDFMatrix *matrix, double tx, double ty);
void _cfPDFToPDFMatrix_scale(_cfPDFToPDFMatrix *matrix, double sx, double sy);

void _cfPDFToPDFMatrix_multiply(_cfPDFToPDFMatrix *lhs, const _cfPDFToPDFMatrix *rhs);

void _cfPDFToPDFMatrix_get(const _cfPDFToPDFMatrix *matrix, double *array);
void _cfPDFToPDFMatrix_get_string(const _cfPDFToPDFMatrix *matrix, char *buffer, size_t bufsize);

#endif // !_CUPS_FILTERS_PDFTOPDF_PDFIO_PDFTOPDF_H
