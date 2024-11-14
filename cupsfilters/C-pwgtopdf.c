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
#include <zlib.h>

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
  cmsHPROFILE          colorProfile;    // ICC Profile to be applied to
					       // PDF
  int                  cm_disabled;        // Flag raised if color
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

// PDF color conversion function
typedef void (*pdf_convert_function)(struct pdf_info *info,
				     pwgtopdf_doc_t *doc);


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

//
// PDF color conversion functons...
//

static void
modify_pdf_color(struct pdf_info *info,
		 int bpp,
		 int bpc,
		 convert_function fn,
		 pwgtopdf_doc_t *doc)
{
  unsigned old_bpp = info->bpp;
  unsigned old_bpc = info->bpc;
  double old_ncolor = old_bpp / old_bpc;

  unsigned old_line_bytes = info->line_bytes;

  double new_ncolor = bpp / bpc;

  info->line_bytes = (unsigned)old_line_bytes * (new_ncolor / old_ncolor);
  info->bpp = bpp;
  info->bpc = bpc;
  doc->conversion_function = fn; 
}

static void
convert_pdf_no_conversion(struct pdf_info *info,
			  pwgtopdf_doc_t *doc)
{
  doc->conversion_function = no_color_conversion;
  doc->bit_function = no_bit_conversion;
}


static void
convert_pdf_cmyk_8_to_white_8(struct pdf_info *info,
			      pwgtopdf_doc_t *doc)
{
  modify_pdf_color(info, 8, 8, cmyk_to_white, doc);
  doc->bit_function = no_bit_conversion;
}


static void
convert_pdf_rgb_8_to_white_8(struct pdf_info *info,
			     pwgtopdf_doc_t *doc)
{
  modify_pdf_color(info, 8, 8, rgb_to_white, doc);
  doc->bit_function = no_bit_conversion;
}


static void
convert_pdf_cmyk_8_to_rgb_8(struct pdf_info *info,
			    pwgtopdf_doc_t *doc)
{
  modify_pdf_color(info, 24, 8, cmyk_to_rgb, doc);
  doc->bit_function = no_bit_conversion;
}


static void
convert_pdf_white_8_to_rgb_8(struct pdf_info *info,
			     pwgtopdf_doc_t *doc)
{
  modify_pdf_color(info, 24, 8, white_to_rgb, doc);
  doc->bit_function = invert_bits;
}


static void
convert_pdf_rgb_8_to_cmyk_8(struct pdf_info *info,
			    pwgtopdf_doc_t *doc)
{
  modify_pdf_color(info, 32, 8, rgb_to_cmyk, doc);
  doc->bit_function = no_bit_conversion;
}


static void
convert_pdf_white_8_to_cmyk_8(struct pdf_info *info,
			      pwgtopdf_doc_t *doc)
{
  modify_pdf_color(info, 32, 8, white_to_cmyk, doc);
  doc->bit_function = invert_bits;
}


static void
convert_pdf_invert_colors(struct pdf_info *info,
			  pwgtopdf_doc_t *doc)
{
  doc->conversion_function = no_color_conversion;
  doc->bit_function = invert_bits;
}

//
// Create an '/ICCBased' array and embed a previously 
// set ICC Profile in the PDF
//
// TODO: HOW IS THIS cmsHPROFILE CALLED
pdfio_obj_t *
embed_icc_profile(pdfio_file_t *pdf, pwgtopdf_doc_t *doc)
{
  pdfio_dict_t *streamdict;
  pdfio_obj_t *icc_stream;
  char *n_value = NULL;
  char *alternate_cs = NULL;
  unsigned char *buff;
  cmsColorSpaceSignature css;

#ifdef USE_LCMS1
  size_t profile_size;
#else
  unsigned int profile_size;
#endif

   // Determine color space signature
  css = cmsGetColorSpace(doc->colorProfile);

  // Determine color component number for /ICCBased array
  switch (css)
  {
    case cmsSigGrayData:
      n_value = "1";
      alternate_cs = "/DeviceGray";
      break;
    case cmsSigRgbData:
      n_value = "3";
      alternate_cs = "/DeviceRGB";
      break;
    case cmsSigCmykData:
      n_value = "4";
      alternate_cs = "/DeviceCMYK";
      break; 
    default:
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                     "cfFilterPWGToPDF: Failed to embed ICC Profile.");
      return NULL;
  }

  cmsSaveProfileToMem(doc->colorProfile, NULL, &profile_size);
  buff = (unsigned char *)calloc(profile_size, sizeof(unsigned char));
  cmsSaveProfileToMem(doc->colorProfile, buff, &profile_size);

  streamdict = pdfioDictCreate(pdf);
  pdfioDictSetName(streamdict, "Alternate", alternate_cs);
  pdfioDictSetName(streamdict, "N", n_value);

  icc_stream = pdfioFileCreateObj(pdf, streamdict);

  if (!icc_stream)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                         	   "cfFilterPWGToPDF: Failed to create ICC profile stream.");
    free(buff);
    return NULL;
  }

  free(buff);
  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                      		 "cfFilterPWGToPDF: ICC Profile embedded in PDF.");

  return icc_stream;
}

pdfio_obj_t*
embed_srgb_profile(pdfio_file_t *pdf,
		   pwgtopdf_doc_t *doc)
{
  pdfio_obj_t *iccbased_reference;

  doc->colorProfile = cmsCreate_sRGBProfile();
  iccbased_reference = embed_icc_profile(pdf, doc);

  return iccbased_reference;
}

//
// Calibration function for non-Lab PDF color spaces
// Requires white point data, and if available, gamma or matrix numbers.
//
// Output:
//   [/'color_space'
//      << /Gamma ['gamma[0]'...'gamma[n]']
//         /WhitePoint ['wp[0]' 'wp[1]' 'wp[2]']
//         /Matrix ['matrix[0]'...'matrix[n*n]']
//      >>
//   ]

pdfio_array_t*
get_calibration_array(pdfio_file_t *pdf,
                      const char *color_space,
                      double wp[],
                      double gamma[],
                      double matrix[],
                      double bp[])
{
  if ((!strcmp("/CalGray", color_space) && matrix != NULL) || wp == NULL) 
    return NULL;
    
  pdfio_array_t *calibration_array = pdfioArrayCreate(pdf);
  if (!calibration_array) 
    return NULL;
    
  pdfioArrayAppendName(calibration_array, color_space);

  pdfio_dict_t *calibration_dict = pdfioDictCreate(pdf);

  if (wp != NULL) 
  {
    pdfio_array_t *white_point_array = pdfioArrayCreate(pdf);
    pdfioArrayAppendNumber(white_point_array, wp[0]);
    pdfioArrayAppendNumber(white_point_array, wp[1]);
    pdfioArrayAppendNumber(white_point_array, wp[2]);
    pdfioDictSetArray(calibration_dict, "WhitePoint", white_point_array);
  }

  if (!strcmp("/CalGray", color_space) && gamma != NULL) 
    pdfioDictSetNumber(calibration_dict, "Gamma", gamma[0]);
    
  else if (!strcmp("/CalRGB", color_space) && gamma != NULL) 
  {
    pdfio_array_t *gamma_array = pdfioArrayCreate(pdf);
    pdfioArrayAppendNumber(gamma_array, gamma[0]);
    pdfioArrayAppendNumber(gamma_array, gamma[1]);
    pdfioArrayAppendNumber(gamma_array, gamma[2]);
    pdfioDictSetArray(calibration_dict, "Gamma", gamma_array);
  }

  if (bp != NULL) 
  {
    pdfio_array_t *black_point_array = pdfioArrayCreate(pdf);
    pdfioArrayAppendNumber(black_point_array, bp[0]);
    pdfioArrayAppendNumber(black_point_array, bp[1]);
    pdfioArrayAppendNumber(black_point_array, bp[2]);
    pdfioDictSetArray(calibration_dict, "BlackPoint", black_point_array);
  }

  if (!strcmp("CalRGB", color_space) && matrix != NULL) 
  {
    pdfio_array_t *matrix_array = pdfioArrayCreate(pdf);
    for (int i = 0; i < 9; i++) 
      pdfioArrayAppendNumber(matrix_array, matrix[i]);
    pdfioDictSetArray(calibration_dict, "Matrix", matrix_array);
  } 

  pdfioArrayAppendDict(calibration_array, calibration_dict);
  return calibration_array;
}


pdfio_array_t*
get_cal_rgb_array(pdfio_file_t *pdf,
		  double wp[3],
		  double gamma[3],
		  double matrix[9],
		  double bp[3])
{
  pdfio_array_t *ret = get_calibration_array(pdf, "CalRGB", wp, gamma, matrix,
				             bp);
  return (ret);
}

pdfio_array_t*
get_cal_gray_array(pdfio_file_t *pdf,
		   double wp[3],
		   double gamma[1],
		   double bp[3])
{
  pdfio_array_t *ret = get_calibration_array(pdf, "CalGray", wp, gamma, 0, bp);
  return (ret);
}

//
// 'make_pclm_strips()' - Return an std::vector of QPDFObjectHandle, each
//                        containing the stream data of the various strips
//                        which make up a PCLm page.
//
// O - std::vector of QPDFObjectHandle
// I - QPDF object
// I - number of strips per page
// I - std::vector of std::shared_ptr<Buffer> containing data for each strip
// I - strip width
// I - strip height
// I - color space
// I - bits per component
// I - document information
//

pdfio_stream_t**
make_pclm_strips(pdfio_file_t *pdf, 
		 unsigned num_strips, 
		 unsigned char **strip_data, 
		 compression_method_t *compression_methods, 
		 unsigned width, unsigned *strip_height, 
		 cups_cspace_t cs, 
		 unsigned bpc,
		 pwgtopdf_doc_t *doc)
{
  pdfio_stream_t **ret = (pdfio_stream_t **)malloc(num_strips * sizeof(pdfio_stream_t *));
  pdfio_dict_t *dict;
  const char *color_space_name;
  unsigned components = 0;

  switch (cs)
  {
    case CUPS_CSPACE_K:
    case CUPS_CSPACE_SW:
      color_space_name = "DeviceGray";
      components = 1;
      break;
    case CUPS_CSPACE_RGB:
    case CUPS_CSPACE_SRGB:
    case CUPS_CSPACE_ADOBERGB:
      color_space_name = "DeviceRGB";
      components = 3;
      break;
    default:
      if (doc->logfunc)  doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		       		      "cfFilterPWGToPDF: Color space not supported.");
      return NULL;
    }
    
  for (size_t i = 0; i < num_strips; i++)
  {
    dict = pdfioDictCreate(pdf);
    
    pdfioDictSetName(dict, "Type", "XObject");
    pdfioDictSetName(dict, "Subtype", "Image");
    pdfioDictSetNumber(dict, "Width", width);
    pdfioDictSetNumber(dict, "BitsPerComponent", bpc); 
    pdfioDictSetName(dict, "ColorSpace", color_space_name);
    pdfioDictSetNumber(dict, "Height", strip_height[i]);
        
    pdfio_obj_t *streamObj = pdfioFileCreateObj(pdf, dict); 
    ret[i] = pdfioObjCreateStream(streamObj, PDFIO_FILTER_NONE);

    compression_method_t compression = compression_methods[0];
    for (unsigned j = 0; j < num_strips; j++)
      compression = compression > compression_methods[j] ? compression : compression_methods[j];

       
    if (compression == FLATE_DECODE)
    {
      pdfioStreamWrite(ret[i], strip_data[i], strip_height[i] * width * components);
      pdfioDictSetName(dict, "Filter", "FlateDecode");
    }
    else if (compression == RLE_DECODE)
    {
      pdfioStreamWrite(ret[i], strip_data[i], strip_height[i] * width * components);
      pdfioDictSetName(dict, "Filter", "RunLengthDecode");
    }
    else if (compression == DCT_DECODE)
    {
      pdfioStreamWrite(ret[i], strip_data[i], strip_height[i] * width * components);
      pdfioDictSetName(dict, "Filter", "DCTDecode");
    }
  }
  return ret;
}

pdfio_obj_t*
make_image(pdfio_file_t *pdf,
           unsigned char *page_data,
           size_t data_size,
           unsigned width,
           unsigned height,
           const char *render_intent,
           cups_cspace_t cs,
           unsigned bpc,
           pwgtopdf_doc_t *doc)
{
  pdfio_dict_t *dict = pdfioDictCreate(pdf);
  pdfio_obj_t *image_obj;
  pdfio_obj_t *icc_ref;
  int use_blackpoint = 0;

  pdfioDictSetName(dict, "Type", "XObject");
  pdfioDictSetName(dict, "Subtype", "Image");
  pdfioDictSetNumber(dict, "Width", width);
  pdfioDictSetNumber(dict, "Height", height);
  pdfioDictSetNumber(dict, "BitsPerComponent", bpc);

  if (!doc->cm_disabled && render_intent)
  {
    if (strcmp(render_intent, "Perceptual") == 0)
      pdfioDictSetName(dict, "Intent", "Perceptual");
    else if (strcmp(render_intent, "Absolute") == 0)
      pdfioDictSetName(dict, "Intent", "AbsoluteColorimetric");
    else if (strcmp(render_intent, "Relative") == 0)
      pdfioDictSetName(dict, "Intent", "RelativeColorimetric");
    else if (strcmp(render_intent, "Saturation") == 0)
      pdfioDictSetName(dict, "Intent", "Saturation");
    else if (strcmp(render_intent, "RelativeBpc") == 0)
    {
      pdfioDictSetName(dict, "Intent", "RelativeColorimetric");
      use_blackpoint = 1;
    }
  }

  if (doc->colorProfile != NULL && !doc->cm_disabled)
  {
      icc_ref = embed_icc_profile(pdf, doc);
      pdfioDictSetObj(dict, "ColorSpace", icc_ref);
  }
  else if (!doc->cm_disabled)
  {
    switch (cs)
    {
      case CUPS_CSPACE_DEVICE1:
      case CUPS_CSPACE_DEVICE2:
      case CUPS_CSPACE_DEVICE3:
      case CUPS_CSPACE_DEVICE4:
      case CUPS_CSPACE_DEVICE5:
      case CUPS_CSPACE_DEVICE6:
      case CUPS_CSPACE_DEVICE7:
      case CUPS_CSPACE_DEVICE8: 
      case CUPS_CSPACE_DEVICE9:
      case CUPS_CSPACE_DEVICEA:
      case CUPS_CSPACE_DEVICEB:
      case CUPS_CSPACE_DEVICEC:
      case CUPS_CSPACE_DEVICED:
      case CUPS_CSPACE_DEVICEE:
      case CUPS_CSPACE_DEVICEF:
	pdfioDictSetName(dict, "ColorSpace", "DeviceCMYK");
       	break;
      case CUPS_CSPACE_K:
       	pdfioDictSetName(dict, "ColorSpace", "DeviceGray");
       	break;
      case CUPS_CSPACE_SW:
	if (use_blackpoint)
       	  pdfioDictSetArray(dict, "ColorSpace", get_cal_gray_array(pdf, cfCmWhitePointSGray(),
								        cfCmGammaSGray(),
								       	cfCmBlackPointDefault()));
	
	else
       	  pdfioDictSetArray(dict, "ColorSpace", get_cal_gray_array(pdf, cfCmWhitePointSGray(),
								        cfCmGammaSGray(), 0));
       	break;
      case CUPS_CSPACE_CMYK:
       	pdfioDictSetName(dict, "ColorSpace", "DeviceCMYK");
       	break;
      case CUPS_CSPACE_RGB:
       	pdfioDictSetName(dict, "ColorSpace", "DeviceRGB");
       	break;
      case CUPS_CSPACE_SRGB:
       	icc_ref = embed_srgb_profile(pdf, doc);
	if(icc_ref != NULL)
       	  pdfioDictSetObj(dict, "ColorSpace", icc_ref);
        else
	  pdfioDictSetName(dict, "ColorSpace", "DeviceRGB");	
	break;
      case CUPS_CSPACE_ADOBERGB:
	if (use_blackpoint)
	  pdfioDictSetArray(dict, "ColorSpace", get_cal_rgb_array(pdf, cfCmWhitePointAdobeRGB(),
				  				       cfCmGammaAdobeRGB(),
								       cfCmMatrixAdobeRGB(),
								       cfCmBlackPointDefault()));
        else	
	  pdfioDictSetArray(dict, "ColorSpace", get_cal_rgb_array(pdf, cfCmWhitePointAdobeRGB(),
				  				       cfCmGammaAdobeRGB(),
								       cfCmMatrixAdobeRGB(), 0));
        break;
        default:
          if (doc->logfunc)
	    doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
			 "cfFilterPWGToPDF: Color space not supported.");
          return NULL;
    }
  }
  else if(doc->cm_disabled)
  {
    switch(cs)
    {
      case CUPS_CSPACE_K:
      case CUPS_CSPACE_SW:
	  pdfioDictSetName(dict, "ColorSpace", "DeviceGray");	
	  break;
      case CUPS_CSPACE_RGB:
      case CUPS_CSPACE_SRGB:
      case CUPS_CSPACE_ADOBERGB:
	  pdfioDictSetName(dict, "ColorSpace", "DeviceRGB");	
	  break;
      case CUPS_CSPACE_DEVICE1:
      case CUPS_CSPACE_DEVICE2:
      case CUPS_CSPACE_DEVICE3:
      case CUPS_CSPACE_DEVICE4:
      case CUPS_CSPACE_DEVICE5:
      case CUPS_CSPACE_DEVICE6:
      case CUPS_CSPACE_DEVICE7:
      case CUPS_CSPACE_DEVICE8:
      case CUPS_CSPACE_DEVICE9:
      case CUPS_CSPACE_DEVICEA:
      case CUPS_CSPACE_DEVICEB:
      case CUPS_CSPACE_DEVICEC:
      case CUPS_CSPACE_DEVICED:
      case CUPS_CSPACE_DEVICEE:
      case CUPS_CSPACE_DEVICEF:
      case CUPS_CSPACE_CMYK:
	  pdfioDictSetName(dict, "ColorSpace", "DeviceCMYK");	
	  break;
      default:
	  if (doc->logfunc)
	    doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
			 "cfFilterPWGToPDF: Color space not supported.");
	  return NULL;
    }
  }
  else
    return NULL;

  image_obj = pdfioFileCreateObj(pdf, dict);

#ifdef PRE_COMPRESS
    uLongf compressed_size = compressBound(data_size);
    unsigned char *compressed_data = (unsigned char *)malloc(compressed_size);
    if (compress(compressed_data, &compressed_size, page_data, data_size) != Z_OK)
    {
        if (doc->logfunc)
            doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR,
                         "cfFilterPWGToPDF: Compression failed.");
        free(compressed_data);
        return NULL;
    }

    pdfio_stream_t *stream = pdfioObjCreateStream(image_obj, PDFIO_FILTER_NONE);
    pdfioStreamWrite(stream, compressed_data, compressed_size);
    pdfioStreamClose(stream);

    free(compressed_data);
#else
    pdfio_stream_t *stream = pdfioStreamCreate(pdf, image_obj);
    pdfioStreamWrite(stream, page_data, page_data_size);
    pdfioStreamClose(stream);
#endif
    return image_obj;
}

static int 
finish_page(struct pdf_info *info, 
     	    pwgtopdf_doc_t *doc)
{
    pdfio_obj_t *image_obj;
    char content[1024];
    size_t content_length = 0;

  if (info->outformat == CF_FILTER_OUT_FORMAT_PDF)
  {
        if (!info->page_data)
            return 0;

        image_obj = make_image(info->pdf, info->page_data, info->width, info->height,
                               info->render_intent, info->color_space, info->bpc, doc);
        if (!image_obj)
        {
            if (doc->logfunc)
                doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                             "cfFilterPWGToPDF: Unable to load image data");
            return 1;
        }

        // Associate image object with the page resources
        pdfio_dict_t *resources = pdfioDictCreate(info->pdf);
        pdfioDictSetObj(resources, "XObject", image_obj);
    }
    else if (info->outformat == CF_FILTER_OUT_FORMAT_PCLM)
    {
        // Handle PCLm specific strips, similar to PDF, but requires strip-specific handling
        if (info->pclm_num_strips == 0)
            return 0;

        for (size_t i = 0; i < info->pclm_num_strips; i++)
        {
            if (!info->pclm_strip_data[i])
                return 0;
        }

        // Further PCLm-specific handling for strips would be implemented here.
    }

    // Add transformation and draw command content
    if (info->outformat == CF_FILTER_OUT_FORMAT_PDF)
    {
        content_length += snprintf(content + content_length, sizeof(content) - content_length,
                                   "%.2f 0 0 %.2f 0 0 cm\n", info->page_width, info->page_height);
        content_length += snprintf(content + content_length, sizeof(content) - content_length, "/I Do\n");
    }
    else if (info->outformat == CF_FILTER_OUT_FORMAT_PCLM)
    {
        double d = DEFAULT_PDF_UNIT / atoi(info->pclm_source_resolution_default);
        content_length += snprintf(content + content_length, sizeof(content) - content_length,
                                   "%.2f 0 0 %.2f 0 0 cm\n", d, d);

        // Iterate through strips and create content for each
        for (unsigned i = 0; i < info->pclm_num_strips; i++)
        {
            unsigned yAnchor = info->height - info->pclm_strip_height[i];
            content_length += snprintf(content + content_length, sizeof(content) - content_length,
                                       "/P <</MCID 0>> BDC q\n%.2f 0 0 %.2f 0 %u cm\n/Image%d Do Q\n",
                                       info->width, info->pclm_strip_height[i], yAnchor, i);
        }
    }

    // Write content stream to PDF page
    pdfio_stream_t *page_content = pdfioStreamCreate(info->pdf, NULL);
    pdfioStreamWrite(page_content, content, content_length);
    pdfioStreamClose(page_content);

    // Reset data after finishing the page
    info->page_data = NULL;
    memset(info->pclm_strip_data, 0, sizeof(info->pclm_strip_data));

    return 0;
}
