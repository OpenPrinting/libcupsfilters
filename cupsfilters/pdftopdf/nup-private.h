//
//Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_FILTERS_PDFTOPDF_NUP_H_
#define _CUPS_FILTERS_PDFTOPDF_NUP_H_

#include "pptypes-private.h"
#include <stdbool.h>

typedef struct _cfPDFToPDFNupParameters
{
  int nupX, nupY;
  float width, height;
  bool landscape;

  pdftopdf_axis_e first;
  pdftopdf_position_e xstart, ystart;
  pdftopdf_position_e xalign, yalign;
} _cfPDFToPDFNupParameters;

void _cfPDFToPDFNupParameters_init(_cfPDFToPDFNupParameters *nupParams);
bool _cfPDFToPDFNupParameters_possible(int nup);
void _cfPDFToPDFNupParameters_preset(int nup, _cfPDFToPDFNupParameters *ret);
void _cfPDFToPDFNupParameters_dump(const _cfPDFToPDFNupParameters *nupParams, pdftopdf_doc_t *doc);

typedef struct _cfPDFToPDFNupPageEdit
{
    float xpos, ypos;
    float scale;

    _cfPDFToPDFPageRect sub;
} _cfPDFToPDFNupPageEdit;

void _cfPDFToPDFNupPageEdit_dump(const _cfPDFToPDFNupPageEdit *edit, pdftopdf_doc_t *doc);

typedef struct _cfPDFToPDFNupState
{
    _cfPDFToPDFNupParameters param;
    int in_pages, out_pages;
    int nup;
    int subpage;
} _cfPDFToPDFNupState;

void _cfPDFToPDFNupState_init(_cfPDFToPDFNupState *state, const _cfPDFToPDFNupParameters *param);
void _cfPDFToPDFNupState_reset(_cfPDFToPDFNupState *state);
bool _cfPDFToPDFNupState_next_page(_cfPDFToPDFNupState *state, float in_width, float in_height, _cfPDFToPDFNupPageEdit *ret);

bool _cfPDFToPDFParseNupLayout(const char *val, _cfPDFToPDFNupParameters *ret);

#endif // !_CUPS_FILTERS_PDFTOPDF_NUP_H_

