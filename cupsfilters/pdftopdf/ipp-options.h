//
// Option support definitions for the IPP tools.
//
// Copyright © 2023 by OpenPrinting.
// Copyright © 2022 by the Printer Working Group.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef IPP_OPTIONS_H
#  define IPP_OPTIONS_H
#  include <cups/cups.h>
#  include <stdbool.h>

#define _CUPS_MAXSAVE   32              // Maximum number of saves

//
// Structures and types...
//

typedef struct _cups_array_s cups_array_t;
                                        // CUPS array type
typedef int (*cups_array_cb_t)(void *first, void *second, void *data);
                                        // Array comparison function
typedef size_t (*cups_ahash_cb_t)(void *element, void *data);
                                        // Array hash function
typedef void *(*cups_acopy_cb_t)(void *element, void *data);
                                        // Array element copy function
typedef void (*cups_afree_cb_t)(void *element, void *data);
                                        // Array element free function

struct _cups_array_s                    // CUPS array structure
{
  // The current implementation uses an insertion sort into an array of
  // sorted pointers.  We leave the array type private/opaque so that we
  // can change the underlying implementation without affecting the users
  // of this API.

  size_t                num_elements,   // Number of array elements
                        alloc_elements, // Allocated array elements
                        current,        // Current element
                        insert,         // Last inserted element
                        num_saved,      // Number of saved elements
                        saved[_CUPS_MAXSAVE];
                                        // Saved elements
  void                  **elements;     // Array elements
  cups_array_cb_t       compare;        // Element comparison function
  bool                  unique;         // Are all elements unique?
  void                  *data;          // User data passed to compare
  cups_ahash_cb_t       hashfunc;       // Hash function
  size_t                hashsize,       // Size of hash
                        *hash;          // Hash array
  cups_acopy_cb_t       copyfunc;       // Copy function
  cups_afree_cb_t       freefunc;       // Free function
};

typedef struct cups_media_s             // Media information
{
  char          media[128],             // Media name to use
                color[128],             // Media color (blank for any/auto)
                source[128],            // Media source (blank for any/auto)
                type[128];              // Media type (blank for any/auto)
  int           width,                  // Width in hundredths of millimeters
                length,                 // Length in hundredths of millimeters
                bottom,                 // Bottom margin in hundredths of millimeters
                left,                   // Left margin in hundredths of millimeters
                right,                  // Right margin in hundredths of millimeters
                top;                    // Top margin in hundredths of millimeters
} cups_media_t;

typedef enum ippopt_delivery_e		// "page-delivery" values
{
  IPPOPT_DELIVERY_SAME_ORDER_FACE_DOWN,	// 'same-order-face-down'
  IPPOPT_DELIVERY_SAME_ORDER_FACE_UP,	// 'same-order-face-up'
  IPPOPT_DELIVERY_REVERSE_ORDER_FACE_DOWN,
					// 'reverse-order-face-down'
  IPPOPT_DELIVERY_REVERSE_ORDER_FACE_UP	// 'reverse-order-face-up'
} ippopt_delivery_t;

typedef enum ippopt_error_report_e	// Combination of "job-error-sheet-type" and "job-error-sheet-when" values
{
  IPPOPT_ERROR_REPORT_NONE,		// "job-error-sheet-type" = 'none'
  IPPOPT_ERROR_REPORT_ON_ERROR,		// "job-error-sheet-type" = 'standard' and "job-error-sheet-when" = 'on-error'
  IPPOPT_ERROR_REPORT_ALWAYS		// "job-error-sheet-type" = 'standard' and "job-error-sheet-when" = 'always'
} ippopt_error_report_t;

typedef struct ippopt_error_sheet_s	// "job-error-sheet" value
{
  ippopt_error_report_t report;		// "job-error-sheet-type/when" value
  cups_media_t	media;			// "media" or "media-col" value, if any
} ippopt_error_sheet_t;

typedef enum ippopt_handling_e		// "multiple-document-handling" values
{
  IPPOPT_HANDLING_COLLATED_COPIES,	// 'separate-documents-collated-copies'
  IPPOPT_HANDLING_UNCOLLATED_COPIES,	// 'separate-documents-uncollated-copies'
  IPPOPT_HANDLING_SINGLE_DOCUMENT,	// 'single-document'
  IPPOPT_HANDLING_SINGLE_NEW_SHEET,	// 'single-document-new-sheet'
} ippopt_handling_t;

typedef struct ippopt_override_s		// "overrides" value
{
  int		first_document,		// Lower document-numbers value
		last_document,		// Upper document-numbers value
		first_page,		// Lower page-numbers value
		last_page;		// Upper page-numbers value
  cups_media_t	media;			// "media" or "media-col" value, if any
  ipp_orient_t	orientation_requested;	// "orientation-requested" value, if any
} ippopt_override_t;

typedef enum ippopt_imgpos_e		// "x/y-image-position" values
{
  IPPOPT_IMGPOS_NONE,			// 'none'
  IPPOPT_IMGPOS_BOTTOM_LEFT,		// 'bottom' or 'left'
  IPPOPT_IMGPOS_CENTER,			// 'center'
  IPPOPT_IMGPOS_TOP_RIGHT		// 'top' or 'right'
} ippopt_imgpos_t;

typedef struct ippopt_range_s		// rangeOfInteger value
{
  int		lower,			// Lower value
		upper;			// Upper value
} ippopt_range_t;

typedef enum ippopt_scaling_e		// "print-scaling" values
{
  IPPOPT_SCALING_AUTO,			// 'auto'
  IPPOPT_SCALING_AUTO_FIT,		// 'auto-fit'
  IPPOPT_SCALING_FILL,			// 'fill'
  IPPOPT_SCALING_FIT,			// 'fit'
  IPPOPT_SCALING_NONE			// 'none'
} ippopt_scaling_t;

typedef enum ippopt_septype_e		// "separator-sheets-type" values
{
  IPPOPT_SEPTYPE_NONE,			// 'none'
  IPPOPT_SEPTYPE_SLIP_SHEETS,		// 'slip-sheets'
  IPPOPT_SEPTYPE_START_SHEET,		// 'start-sheet'
  IPPOPT_SEPTYPE_END_SHEET,		// 'end-sheet'
  IPPOPT_SEPTYPE_BOTH_SHEETS,		// 'both-sheets'
} ippopt_septype_t;

typedef struct ipp_options_s		// All IPP options in one structure
{
  int		copies;			// "copies" value
  size_t	num_force_front_side;	// Number of "force-front-side" values
  int		force_front_side[100];	// "force-front-side" values
  ipp_orient_t	image_orientation;	// "image-orientation" value
  char		imposition_template[128];
					// "imposition-template" value, if any
  ippopt_error_sheet_t job_error_sheet;	// "job-error-sheet" value
  char		job_name[256];		// "job-name" value
  char		job_originating_user_name[256];
					// "job-originating-user-name" value
  int		job_pages_per_set;	// "job-pages-per-set" value
  char		job_sheet_message[1024];// "job-sheet-message" value
  char		job_sheets[128];	// "job-sheets" value
  cups_media_t	job_sheets_media;	// "job-sheets-col" "media" or "media-col" value
  cups_media_t	media;			// "media" or "media-col" value
  ippopt_handling_t multiple_document_handling;
					// "multiple-document-handling" value
  int		number_up;		// "number-up" value
  ipp_orient_t	orientation_requested;	// "orientation-requested" value
  char		output_bin[128];	// "output-bin" value
  cups_array_t	*overrides;		// "overrides" value(s)
  ippopt_delivery_t page_delivery;	// "page-delivery" value
  size_t	num_page_ranges;	// Number of "page-ranges" values
  ippopt_range_t page_ranges[100];	// "page-ranges" values
  char		print_color_mode[128];	// "print-color-mode" value
  char		print_content_optimize[128];
					// "print-content-optimize" value
  ipp_quality_t	print_quality;		// "print-quality" value
  char		print_rendering_intent[128];
					// "print-rendering-intent" value
  ippopt_scaling_t print_scaling;	// "print-scaling" value
  int		printer_resolution[2];	// "printer-resolution" values (DPI)
  ippopt_septype_t separator_type;	// "separator-sheets-type" value
  cups_media_t	separator_media;	// "separator-sheets" "media" or "media-col" value
  char		sides[128];		// "sides" value
  ippopt_imgpos_t x_image_position;	// "x-image-position" value
  int		x_side1_image_shift,	// "x-side1-image-shift" or "x-image-shift" value
		x_side2_image_shift;	// "x-side2-image-shift" or "x-image-shift" value
  ippopt_imgpos_t y_image_position;	// "y-image-position" value
  int		y_side1_image_shift,	// "y-side1-image-shift" or "y-image-shift" value
		y_side2_image_shift;	// "y-side2-image-shift" or "y-image-shift" value
} ipp_options_t;


//
// Functions...
//

extern bool		ippOptionsCheckPage(ipp_options_t *ippo, int page);
extern void		ippOptionsDelete(ipp_options_t *ippo);
extern int		ippOptionsGetFirstPage(ipp_options_t *ippo);
extern int		ippOptionsGetLastPage(ipp_options_t *ippo);
extern ipp_orient_t	ippOptionGetOverrides(ipp_options_t *ippo, int document, int page, cups_media_t *media);
extern ipp_options_t	*ippOptionsNew(size_t num_options, cups_option_t *options);


#endif // !IPP_OPTIONS_H
