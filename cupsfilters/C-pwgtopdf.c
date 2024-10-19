//
// PWG/Apple Raster to PDF filter function for libcupsfilters.
//
// Copyright 2010 by Neil 'Superna' Armstrong <superna9999@gmail.com>
// Copyright 2012 by Tobias Hoffmann
// Copyright 2014-2022 by Till Kamppeter
// Copyright 2017 by Sahil Arora
// Copyright 2024 by Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//


#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits>
#include <signal.h>
#include <cups/cups.h>
#include <cups/raster.h>
#include <cupsfilters/filter.h>
#include <cupsfilters/colormanager.h>
#include <cupsfilters/image.h>
#include <cupsfilters/ipp.h>
#include <cupsfilters/libcups2-private.h>

#include <arpa/inet.h>   // ntohl

#include <pdfio.h>
#include <pdfio-content.h>

#ifdef USE_LCMS1
#include <lcms.h>
#define cmsColorSpaceSignature icColorSpaceSignature
#define cmsSetLogErrorHandler cmsSetErrorHandler
#define cmsSigXYZData icSigXYZData
#define cmsSigLuvData icSigLuvData
#define cmsSigLabData icSigLabData
#define cmsSigYCbCrData icSigYCbCrData
#define cmsSigYxyData icSigYxyData
#define cmsSigRgbData icSigRgbData
#define cmsSigHsvData icSigHsvData
#define cmsSigHlsData icSigHlsData
#define cmsSigCmyData icSigCmyData
#define cmsSig3colorData icSig3colorData
#define cmsSigGrayData icSigGrayData
#define cmsSigCmykData icSigCmykData
#define cmsSig4colorData icSig4colorData
#define cmsSig2colorData icSig2colorData
#define cmsSig5colorData icSig5colorData
#define cmsSig6colorData icSig6colorData
#define cmsSig7colorData icSig7colorData
#define cmsSig8colorData icSig8colorData
#define cmsSig9colorData icSig9colorData
#define cmsSig10colorData icSig10colorData
#define cmsSig11colorData icSig11colorData
#define cmsSig12colorData icSig12colorData
#define cmsSig13colorData icSig13colorData
#define cmsSig14colorData icSig14colorData
#define cmsSig15colorData icSig15colorData
#define cmsSaveProfileToMem _cmsSaveProfileToMem
#else
#include <lcms2.h>
#endif

#define DEFAULT_PDF_UNIT 72   // 1/72 inch

#define PRE_COMPRESS

// Compression method for providing data to PCLm Streams.
typedef enum compression_method_e
{
  DCT_DECODE = 0,
  FLATE_DECODE,
  RLE_DECODE
} compression_method_t;

// Color conversion function
typedef unsigned char *(*convert_function)(unsigned char *src,
					   unsigned char *dst,
					   unsigned int pixels);

// Bit conversion function
typedef unsigned char *(*bit_convert_function)(unsigned char *src,
					       unsigned char *dst,
					       unsigned int pixels);

typedef struct pwgtopdf_doc_s                  // **** Document information ****
{
  cmsHPROFILE          colorProfile = NULL;    // ICC Profile to be applied to
					       // PDF
  int                  cm_disabled = 0;        // Flag raised if color
					       // management is disabled
  convert_function     conversion_function;    // Raster color conversion
					       // function
  bit_convert_function bit_function;           // Raster bit function
  FILE		       *outputfp;	       // Temporary file, if any
  cf_logfunc_t         logfunc;                // Logging function, NULL for no
					       // logging
  void                 *logdata;               // User data for logging
					       // function, can be NULL
  cf_filter_iscanceledfunc_t iscanceledfunc;   // Function returning 1 when
                                               // job is canceled, NULL for not
                                               // supporting stop on cancel
  void                 *iscanceleddata;        // User data for is-canceled
					       // function, can be NULL
} pwgtopdf_doc_t;

// PDF color conversion function
typedef void (*pdf_convert_function)(struct pdf_info * info,
				     pwgtopdf_doc_t *doc);

struct pdf_info 
{
  // PDFio-specific members
  pdfio_file_t *pdf;  // PDFio file handle
  pdfio_obj_t *page; // PDFio page handle
   
  unsigned pagecount;
  unsigned width;
  unsigned height;
  unsigned line_bytes;
  unsigned bpp;
  unsigned bpc;
  unsigned 		pclm_num_strips;
  unsigned 		pclm_strip_height_preferred;
  unsigned 		*pclm_strip_height;  // Dynamically allocated array in C
  unsigned 		*pclm_strip_height_supported;  // Dynamically allocated array 
  compression_method_t 	*pclm_compression_method_preferred;
  size_t 		num_compression_methods; 
  char 			**pclm_source_resolution_supported;  // Array of dynamically allocated strings
  char 			*pclm_source_resolution_default;     // Pointer to dynamically allocated string
  char 			*pclm_raster_back_side;              // Pointer to dynamically allocated string
  unsigned char 	**pclm_strip_data;  // Array of pointers to raw data (buffers)
  char 			*render_intent;                      // Pointer to dynamically allocated string
  cups_cspace_t 	color_space;
  unsigned char 	*page_data;  // Pointer to raw page data
  double 		page_width, page_height;
  cf_filter_out_format_t outformat;
};

void 
init_pdf_info(struct pdf_info *info, 
	      size_t num_methods, 
	      size_t num_strips_supported)
{
  // Initialize primitive types
  info->pagecount = 0;
  info->width = 0;
  info->height = 0;
  info->line_bytes = 0;
  info->bpp = 0;
  info->bpc = 0;
  info->pclm_num_strips = 0;
  info->pclm_strip_height_preferred = 16;  // Default strip height
  info->page_width = 0;
  info->page_height = 0;
  info->outformat = CF_FILTER_OUT_FORMAT_PDF;

  // Allocate memory for pclm_strip_height (for strip height handling)
  info->pclm_strip_height = (unsigned *)malloc(num_strips_supported * sizeof(unsigned));
  if (info->pclm_strip_height)
  {
    for (size_t i = 0; i < num_strips_supported; ++i)
    {
      info->pclm_strip_height[i] = 0;  // Initialize to 0 or a specific value as needed
    }
  }

  // Allocate memory for pclm_strip_height_supported
  info->pclm_strip_height_supported = (unsigned *)malloc(num_strips_supported * sizeof(unsigned));
  if (info->pclm_strip_height_supported)
  {
    for (size_t i = 0; i < num_strips_supported; ++i)
    {
      info->pclm_strip_height_supported[i] = 16;  // Initialize to default value
    }
  }

  // Allocate memory for multiple compression methods
  info->num_compression_methods = num_methods;
  info->pclm_compression_method_preferred = (compression_method_t *)malloc(num_methods * sizeof(compression_method_t));
  if (info->pclm_compression_method_preferred)
  {
    for (size_t i = 0; i < num_methods; ++i)
    {
      info->pclm_compression_method_preferred[i] = 0;  // Initialize to default or specific compression method
    }
  }

  info->pclm_source_resolution_default = (char *)malloc(64 * sizeof(char));
  if (info->pclm_source_resolution_default)
  {
    strcpy(info->pclm_source_resolution_default, "");  // Initialize to empty string
  }

  info->pclm_raster_back_side = (char *)malloc(64 * sizeof(char));
  if (info->pclm_raster_back_side)
  {
    strcpy(info->pclm_raster_back_side, "");  // Initialize to empty string
  }

  info->render_intent = (char *)malloc(64 * sizeof(char));
  if (info->render_intent)
  {
    strcpy(info->render_intent, "");  // Initialize to empty string
  }

  info->pclm_source_resolution_supported = NULL; 
  info->pclm_strip_data = NULL;  // Assuming this will be dynamically allocated elsewhere

  info->color_space = CUPS_CSPACE_K;  // Default color space
  info->page_data = NULL;  // Will be allocated when needed

  info->pdf = NULL;  // Initialize to NULL, will be set when opening a file
  info->page = NULL;  // Initialize to NULL, will be set when reading a page
}

// Freeing the dynamically allocated memory
void free_pdf_info(struct pdf_info *info)
{
  if (info->pclm_strip_height)
  {
    free(info->pclm_strip_height);
    info->pclm_strip_height = NULL;
  }

  if (info->pclm_strip_height_supported)
  {
    free(info->pclm_strip_height_supported);
    info->pclm_strip_height_supported = NULL;
  }

  if (info->pclm_compression_method_preferred)
  {
    free(info->pclm_compression_method_preferred);
    info->pclm_compression_method_preferred = NULL;
  }

  // Free dynamically allocated strings
  if (info->pclm_source_resolution_default)
  {
    free(info->pclm_source_resolution_default);
    info->pclm_source_resolution_default = NULL;
  }

  if (info->pclm_raster_back_side)
  {
    free(info->pclm_raster_back_side);
    info->pclm_raster_back_side = NULL;
  }

  if (info->render_intent)
  {
    free(info->render_intent);
    info->render_intent = NULL;
  }

  // Free any other dynamically allocated memory as necessary
  if (info->pclm_strip_data)
  {
    free(info->pclm_strip_data);  // Assuming this array will be dynamically allocated elsewhere
    info->pclm_strip_data = NULL;
  }

  if (info->page_data)
  {
    free(info->page_data);
    info->page_data = NULL;
  }
}

//
// Bit conversion functions
//
static unsigned char *
invert_bits(unsigned char *src,
	    unsigned char *dst,
	    unsigned int pixels)
{ 
  unsigned int i;

  // Invert black to grayscale...
  for (i = pixels, dst = src; i > 0; i --, dst ++)
    *dst = ~*dst;

  return (dst);
}	

static unsigned char *
no_bit_conversion(unsigned char *src,
		  unsigned char *dst,
		  unsigned int pixels)
{
  return (src);
}

//
// Color conversion functions
//

static unsigned char *
rgb_to_cmyk(unsigned char *src,
	    unsigned char *dst,
	    unsigned int pixels)
{
  cfImageRGBToCMYK(src, dst, pixels);
  return (dst);
}


static unsigned char *
white_to_cmyk(unsigned char *src,
	      unsigned char *dst,
	      unsigned int pixels)
{
  cfImageWhiteToCMYK(src, dst, pixels);
  return (dst);
}


static unsigned char *
cmyk_to_rgb(unsigned char *src,
	    unsigned char *dst,
	    unsigned int pixels)
{
  cfImageCMYKToRGB(src, dst, pixels);
  return (dst);
}


static unsigned char *
white_to_rgb(unsigned char *src,
	     unsigned char *dst,
	     unsigned int pixels)
{
  cfImageWhiteToRGB(src, dst, pixels);
  return (dst);
}


static unsigned char *
rgb_to_white(unsigned char *src,
	     unsigned char *dst,
	     unsigned int pixels)
{
  cfImageRGBToWhite(src, dst, pixels);
  return (dst);
}


static unsigned char *
cmyk_to_white(unsigned char *src,
	      unsigned char *dst,
	      unsigned int pixels)
{
  cfImageCMYKToWhite(src, dst, pixels);
  return (dst);
}


static unsigned char *
no_color_conversion(unsigned char *src,
		    unsigned char *dst,
		    unsigned int pixels)
{
  return (src);
}

//
// 'split_strings()' - Split a string to a vector of strings given some
//                     delimiters
//
// O - std::vector of std::string after splitting
// I - input string to be split
// I - string containing delimiters
//
// Function to split strings by delimiters

char**
split_strings(const char *str, 
	      const char *delimiters, 
	      int *size) 
{
  if (delimiters == NULL || strlen(delimiters) == 0) 
    delimiters = ",";
    

  int capacity = 10;
  char **result = malloc(capacity * sizeof(char *));

  char *value = malloc(strlen(str) + 1);
    
  int token_count = 0;
  int index = 0;
  bool push_flag = false;

  for (size_t i = 0; i < strlen(str); i++) 
  {
    if (strchr(delimiters, str[i]) != NULL) 
    { 
      if (push_flag && index > 0) 
      {
        value[index] = '\0';
        result[token_count] = malloc(strlen(value) + 1);
        strcpy(result[token_count], value);
        token_count++;

        if (token_count >= capacity) 
	{
          capacity *= 2;
	  result = realloc(result, capacity * sizeof(char *));
       	}
      
	index = 0;
       	push_flag = false;
      }
    } 
    else 
    {
      value[index++] = str[i];
      push_flag = true;
    }
  }

  if (push_flag && index > 0) 
  {
    value[index] = '\0';
    result[token_count] = malloc(strlen(value) + 1);
    strcpy(result[token_count], value);
    token_count++;
  }

  *size = token_count;

  free(value);
  return result;
}

//
// 'num_digits()' - Calculates the number of digits in an integer
//
// O - number of digits in the input integer
// I - the integer whose digits needs to be calculated
//

static int
num_digits(int n)
{
  if (n == 0)
    return (1);
  int digits = 0;
  while (n)
  {
    ++digits;
    n /= 10;
  }
  return (digits);
}

//
// 'int_to_fwstring()' - Convert a number to fixed width string by padding
//                       with zeroes
// O - converted string
// I - the integee which needs to be converted to string
// I - width of string required
//

char*
int_to_fwstring(int n, int width) 
{
  int num_zeroes = width - num_digits(n);
  if (num_zeroes < 0) 
    num_zeroes = 0;

  int result_length = num_zeroes + num_digits(n) + 1; 
  char *result = malloc(result_length * sizeof(char));
   
  for (int i = 0; i < num_zeroes; i++) 
    result[i] = '0';

  sprintf(result + num_zeroes, "%d", n);
  return result;
}

static int 
create_pdf_file(struct pdf_info *info, 
		const cf_filter_out_format_t outformat)
{
  if (!info || !info->pdf) 
    return 1;  // Error handling
  
  pdfio_file_t *temp = pdfioFileCreate(pdfioFileGetName(info->pdf), NULL, NULL, NULL, NULL, NULL);

  info->pdf = temp;  
  info->outformat = outformat;

  return 0; 
}

static pdfio_rect_t
make_real_box(double x1,
	      double y1,
	      double x2,
	      double y2)
{
  pdfio_rect_t ret;

  ret.x1 = x1;
  ret.y1 = y1;
  ret.x2 = x2;
  ret.y2 = y2;

  return (ret);
}

static pdfio_rect_t
make_integer_box(double x1,
	      double y1,
	      double x2,
	      double y2)
{
  pdfio_rect_t ret;

  ret.x1 = x1;
  ret.y1 = y1;
  ret.x2 = x2;
  ret.y2 = y2;

  return (ret);
}


