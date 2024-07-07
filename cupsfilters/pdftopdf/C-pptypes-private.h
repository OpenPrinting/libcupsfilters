#include "pdftopdf-private.h"
#include <math.h>

typedef enum {
    X,
    Y
} pdftopdf_axis_e;

typedef enum {
    CENTER = 0,
    LEFT = -1,
    RIGHT = 1,
    TOP = 1,
    BOTTOM = -1
} pdftopdf_position_e;

void _cfPDFToPDFPositionDump(pdftopdf_position_e pos, pdftopdf_doc_t *doc);
void _cfPDFToPDFPositionAndAxisDump(pdftopdf_position_e pos, pdftopdf_axis_e axis,
				    pdftopdf_doc_t *doc);

typedef enum {
    ROT_0,
    ROT_90,
    ROT_180,
    ROT_270,
} pdftopdf_rotation_e;

void _cfPDFToPDFRotationDump(pdftopdf_rotation_e rot, pdftopdf_doc_t *doc);

pdftopdf_rotation_e add_rotations(pdftopdf_rotation_e lhs, pdftopdf_rotation_e rhs);
pdftopdf_rotation_e subtract_rotations(pdftopdf_rotation_e lhs, pdftopdf_rotation_e rhs);
pdftopdf_rotation_e negate_rotation(pdftopdf_rotation_e rhs);

typedef enum{
    NONE = 0,
    ONE_THIN = 2,
    ONE_THICK = 3,
    TWO_THIN = 4, 
    TWO_THICK = 5,
    ONE = 0x02,
    TWO = 0x04,
    THICK = 0x01  
} pdftopdf_border_type_e;

void _cfPDFToPDFBorderTypeDump(pdftopdf_border_type_e border,
                               pdftopdf_doc_t *doc);

typedef struct {
    float top, left, right, bottom; // i.e. margins
    float width, height;
} _cfPDFToPDFPageRect;

void _cfPDFToPDFPageRect_init(_cfPDFToPDFPageRect *rect);

void _cfPDFToPDFPageRect_rotate_move(_cfPDFToPDFPageRect *rect, 
				     pdftopdf_rotation_e r, 
				     float pwidth, 
				     float pheight);

void _cfPDFToPDFPageRect_scale(_cfPDFToPDFPageRect *rect, 
			       float mult);

void _cfPDFToPDFPageRect_translate(_cfPDFToPDFPageRect *rect, 
				   float tx, 
				   float ty);

void _cfPDFToPDFPageRect_set(_cfPDFToPDFPageRect *rect, const 
			     _cfPDFToPDFPageRect *rhs);

void _cfPDFToPDFPageRect_dump(const _cfPDFToPDFPageRect *rect, 
			      pdftopdf_doc_t *doc);

