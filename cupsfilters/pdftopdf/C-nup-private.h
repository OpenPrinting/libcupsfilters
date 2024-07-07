#include "C-pptypes-private.h"
#include <stdbool.h>

typedef struct
{
  int nupX, nupY;
  float width, height;
  bool landscape;  
  pdftopdf_axis_e first;
  pdftopdf_position_e xstart, ystart;
  pdftopdf_position_e xalign, yalign;
} _cfPDFToPDFNupParameters;

void _cfPDFToPDFNupParameters_init(_cfPDFToPDFNupParameters *nupParam);
void _cfPDFToPDFNupParameters_dump(const _cfPDFToPDFNupParameters *nupParam,
				   pdftopdf_doc_t *doc);
bool _cfPDFToPDFNupParameters_possible(int nup);
void _cfPDFToPDFNupParameters_preset(int nup, _cfPDFToPDFNupParameters *ret);


typedef struct 
{
  float xpos, ypos;
  float scale;
  _cfPDFToPDFPageRect *sub;
} _cfPDFToPDFNupPageEdit;

void _cfPDFToPDFNupPageEdit_dump(const _cfPDFToPDFNupPageEdit *nupPageEdit,
	  			 pdftopdf_doc_t *doc);

typedef struct
{
  int first, second;
} integerPair; 

typedef struct
{
  _cfPDFToPDFNupParameters *param;
  int in_pages, out_pages;
  int nup;
  int subpage;
} _cfPDFToPDFNupState;

void _cfPDFToPDFNupState_init(_cfPDFToPDFNupState *nupState,
			      _cfPDFToPDFNupParameters *nupParam);
void _cfPDFToPDFNupState_reset(_cfPDFToPDFNupState *nupState);
integerPair _cfPDFToPDFNupState_convert_order(_cfPDFToPDFNupState *nupState, 
					      int subpage);
void _cfPDFToPDFNupState_calculate_edit(const _cfPDFToPDFNupState *nupState,
					int subx, int suby, _cfPDFToPDFNupPageEdit *ret);
bool _cfPDFToPDFNupState_mext_page(_cfPDFToPDFNupState *nupState,
				   float in_width, float in_height, 
				   _cfPDFToPDFNupPageEdit *ret);

typedef struct
{
  pdftopdf_axis_e first;
  pdftopdf_position_e second;
} resultPair;

bool _cfPDFToPDFParseNupLayout(const char *val, _cfPDFToPDFNupParameters *ret);
