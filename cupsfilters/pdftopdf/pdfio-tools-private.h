//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_FILTERS_PDFTOPDF_QPDF_TOOLS_H_
#define _CUPS_FILTERS_PDFTOPDF_QPDF_TOOLS_H_

#include <pdfio.h>

pdfio_rect_t _cfPDFToPDFGetMediaBox(pdfio_obj_t *page);
pdfio_rect_t _cfPDFToPDFGetCropBox(pdfio_obj_t *page);
pdfio_rect_t _cfPDFToPDFGetBleedBox(pdfio_obj_t *page);
pdfio_rect_t _cfPDFToPDFGetTrimBox(pdfio_obj_t *page);
pdfio_rect_t _cfPDFToPDFGetArtBox(pdfio_obj_t *page);

pdfio_rect_t* _cfPDFToPDFMakeBox(double x1, double y1, double x2, double y2);

#endif // !_CUPS_FILTERS_PDFTOPDF_QPDF_TOOLS_H_

