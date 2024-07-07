#include <pdfio.h>

bool _cfPDFToPDFHasOutputIntent(pdfio_file_t *pdf);
void _cfPDFToPDFAddOutputIntent(pdfio_file_t *pdf, const char *filename);

void _cfPDFToPDFAddDefaultRGB(pdfio_file_t *pdf, pdfio_stream_t *srcicc);
pdfio_stream_t* _cfPDFToPDFSetDefaultICC(pdfio_file_t *pdf, const char *filename);


