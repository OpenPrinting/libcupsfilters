//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_FILTERS_PDFTOPDF_PDFIO_CM_H_
#define _CUPS_FILTERS_PDFTOPDF_PDFIO_CM_H_

#include <pdfio.h>

bool _cfPDFToPDFHasOutputIntent(pdfio_file_t *pdf);
void _cfPDFToPDFAddOutputIntent(pdfio_file_t *pdf, const char *filename);

void _cfPDFToPDFAddDefaultRGB(pdfio_file_t *pdf, pdfio_obj_t *icc_obj);
pdfio_obj_t* _cfPDFToPDFSetDefaultICC(pdfio_file_t *pdf, const char *filename);

#endif // !_CUPS_FILTERS_PDFTOPDF_PDFIO_CM_H_
