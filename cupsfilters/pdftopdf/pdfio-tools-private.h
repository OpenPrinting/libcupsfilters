//
//
//
//
#include <pdfio.h>

pdfio_rect_t _cfPDFToPDFGetMediaBox(pdfio_obj_t page);
pdfio_rect_t _cfPDFToPDFGetCropBox(pdfio_obj_t page);
pdfio_rect_t _cfPDFToPDFGetBleedBox(pdfio_obj_t page);
pdfio_rect_t _cfPDFToPDFGetTrimBox(pdfio_obj_t page);
pdfio_rect_t _cfPDFToPDFGetArtBox(pdfio_obj_t page);

pdfio_rect_t _cfPDFToPDFMakeBox(double x1, double y1, double x2, double y2);

