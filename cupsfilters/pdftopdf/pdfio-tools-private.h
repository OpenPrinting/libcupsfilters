//
//
//
//
#ifndef _PDFIO_H_

#define _PDFIO_H_
#include <pdfio.h>
#endif

pdfio_rect_t _cfPDFToPDFGetMediaBox1(pdfio_obj_t *page);
pdfio_rect_t _cfPDFToPDFGetCropBox1(pdfio_obj_t *page);
pdfio_rect_t _cfPDFToPDFGetBleedBox1(pdfio_obj_t *page);
pdfio_rect_t _cfPDFToPDFGetTrimBox(pdfio_obj_t *page);
pdfio_rect_t _cfPDFToPDFGetArtBox1(pdfio_obj_t *page);

pdfio_rect_t _cfPDFToPDFMakeBox(double x1, double y1, double x2, double y2);
