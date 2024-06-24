//
//
//
//
#include <pdfio.h>
#include "C-pptypes-private.h"

//helper functions

_cfPDFToPDFPageRect _cfPDFToPDFGetBoxAsRect(pdfio_rect_t *box);
pdfio_rect_t _cfPDFToPDFGetRectAsBox(const _cfPDFToPDFPageRect *rect);

// Note that PDF specification is CW, but our Rotation is CCW
pdftopdf_rotation_e _cfPDFToPDFGetRotate(pdfio_obj_t *page);
int _cfPDFToPDFMakeRotate(pdftopdf_rotation_e rot); // integer, change all calls

double _cfPDFToPDFGetUserUnit(pdfio_obj_t *page);

//class
typedef struct 
{
  double ctm[6];
} _cfPDFToPDFMatrix;

void _cfPDFToPDFMatrix_init(_cfPDFToPDFMatrix *matrix);

void _cfPDFToPDFMatrix_init_with_handle(_cfPDFToPDFMatrix *matrix, 
					pdfio_array_t *ar);

_cfPDFToPDFMatrix _cfPDFToPDFMatrix_rotate_function(_cfPDFToPDFMatrix *matrix, 
						     pdftopdf_rotation_e rot);

_cfPDFToPDFMatrix _cfPDFToPDFMatrix_rotate_move(_cfPDFToPDFMatrix *matrix, 
						pdftopdf_rotation_e rot, 
						double width, 
						double height);

_cfPDFToPDFMatrix _cfPDFToPDFMatrix_rotate_rad(_cfPDFToPDFMatrix *matrix, 
					       double rad);

_cfPDFToPDFMatrix _cfPDFToPDFMatrix_translate(_cfPDFToPDFMatrix *matrix, 
					      double tx, 
					      double ty);

_cfPDFToPDFMatrix _cfPDFToPDFMatrix_scale(_cfPDFToPDFMatrix *matrix, 
					  double sx, 
					  double sy);

_cfPDFToPDFMatrix _cfPDFToPDFMatrix_scale_uniform(_cfPDFToPDFMatrix *matrix, 
						  double s);


_cfPDFToPDFMatrix _cfPDFToPDFMatrix_multiply(_cfPDFToPDFMatrix *matrix, 
					     const _cfPDFToPDFMatrix *rhs);

pdfio_array_t *_cfPDFToPDFMatrix_get_array(const _cfPDFToPDFMatrix *matrix);

char _cfPDFToPDFMatrix_get_string(const _cfPDFToPDFMatrix *matrix);



