#include <stdio.h>
#include <pdfio.h>
	
pdfio_rect_t
_cfPDFToPDFGetMediaBox(pdfio_obj_t *page)
{
	pdfio_rect_t mediaBox;
        pdfio_dict_t *page_dict = pdfioObjGetDict(page);
        pdfioDictGetRect(page_dict, "MediaBox", &mediaBox);
        return (mediaBox);
}

pdfio_rect_t
_cfPDFToPDFGetCropBox(pdfio_obj_t *page)
{
    	pdfio_rect_t cropBox;
    	pdfio_dict_t *page_dict = pdfioObjGetDict(page);
    	if (!pdfioDictGetRect(page_dict, "CropBox", &cropBox)) {
        	return _cfPDFToPDFGetMediaBox(page);
    	}
    	return cropBox;
}

pdfio_rect_t
_cfPDFToPDFGetBleedBox(pdfio_obj_t *page)
{
	pdfio_rect_t bleedBox;
    	pdfio_dict_t *page_dict = pdfioObjGetDict(page);
    	if (!pdfioDictGetRect(page_dict, "BleedBox", &bleedBox)) {
        	return _cfPDFToPDFGetCropBox(page);
    	}
   	return bleedBox;
}

pdfio_rect_t
_cfPDFToPDFGetTrimBox(pdfio_obj_t *page)
{
	pdfio_rect_t trimBox;
    	pdfio_dict_t *page_dict = pdfioObjGetDict(page);
    	if (!pdfioDictGetRect(page_dict, "TrimBox", &trimBox)) {
       	 	return _cfPDFToPDFGetCropBox(page);
    	}
    	return trimBox;
}

pdfio_rect_t
_cfPDFToPDFGetArtBox(pdfio_obj_t *page)
{
	pdfio_rect_t artBox;
    	pdfio_dict_t *page_dict = pdfioObjGetDict(page);
    	if (!pdfioDictGetRect(page_dict, "ArtBox", &artBox)) {
        	return _cfPDFToPDFGetCropBox(page);
    	}
    	return artBox;
}

pdfio_rect_t
_cfPDFToPDFMakeBox(double x1, double y1, double x2, double y2)
{
	pdfio_rect_t ret;
	ret.x1 = x1;
	ret.y1 = y1;
	ret.x2 = x2;
	ret.y2 = y2;

	return ret;
}

void printBox(pdfio_rect_t *box) {
	printf("[%.5f, %.5f, %.5f, %.5f]\n", box->x1, box->y1, box->x2, box->y2);
}

int main(){
	  // Open the PDF file
    pdfio_file_t *pdf = pdfioFileOpen("test.pdf", NULL, NULL, NULL, NULL);
    if (!pdf) {
        fprintf(stderr, "Error opening PDF file.\n");
        return 1;
    }

    // Get the first page
    pdfio_obj_t *page = pdfioFileGetPage(pdf, 0);
    if (!page) {
        fprintf(stderr, "Error getting page from PDF.\n");
        pdfioFileClose(pdf);
        return 1;
    }

    // Test _cfPDFToPDFGetMediaBox function
    printf("Testing MediaBox retrieval:\n");
    pdfio_rect_t mediaBox = _cfPDFToPDFGetMediaBox(page);
    printf("MediaBox: ");
    printBox(&mediaBox);

    // Test _cfPDFToPDFGetCropBox function
    printf("Testing CropBox retrieval:\n");
    pdfio_rect_t cropBox = _cfPDFToPDFGetCropBox(page);
    printf("CropBox: ");
    printBox(&cropBox);

    // Test _cfPDFToPDFGetBleedBox function
    printf("Testing BleedBox retrieval:\n");
    pdfio_rect_t bleedBox = _cfPDFToPDFGetBleedBox(page);
    printf("BleedBox: ");
    printBox(&bleedBox);

    // Test _cfPDFToPDFGetTrimBox function
    printf("Testing TrimBox retrieval:\n");
    pdfio_rect_t trimBox = _cfPDFToPDFGetTrimBox(page);
    printf("TrimBox: ");
    printBox(&trimBox);

    // Test _cfPDFToPDFGetArtBox function
    printf("Testing ArtBox retrieval:\n");
    pdfio_rect_t artBox = _cfPDFToPDFGetArtBox(page);
    printf("ArtBox: ");
    printBox(&artBox);

    // Test _cfPDFToPDFMakeBox function
    printf("Testing box creation:\n");
    pdfio_rect_t customBox = _cfPDFToPDFMakeBox(30, 30, 570, 770);
    printf("CustomBox: ");
    printBox(&customBox);

    // Clean up
    pdfioFileClose(pdf);
    return 0;
}
