//
// PWG/Apple Raster to PDF filter function for libcupsfilters.
//
// Copyright 2010 by Neil 'Superna' Armstrong <superna9999@gmail.com>
// Copyright 2012 by Tobias Hoffmann
// Copyright 2014-2022 by Till Kamppeter
// Copyright 2017 by Sahil Arora
// Copyright 2024-2026 by Uddhav Phatak <uddhavphatak@gmail.com>
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
#include <limits.h>

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

// PDF info structure
struct pdf_info{
    pdfio_file_t *pdf;
    pdfio_dict_t *page_dict;
    pdfio_obj_t *page;
    pdfio_stream_t *page_stream;
    char *temp_filename;

    unsigned pagecount;
    unsigned width;
    unsigned height;
    unsigned line_bytes;
    unsigned bpp;
    unsigned bpc;

    unsigned 		pclm_num_strips;
    unsigned 		pclm_strip_height_preferred;
    
    unsigned 		*pclm_strip_height;
    size_t 		pclm_strip_height_size;

    unsigned 		*pclm_strip_height_supported;
    size_t 		pclm_strip_height_supported_size;

    compression_method_t *pclm_compression_method_preferred;
    size_t 		 pclm_compression_method_preferred_size;

    char 		**pclm_source_resolution_supported;
    size_t 		pclm_source_resolution_supported_size;

    char 		*pclm_source_resolution_default;
    char 		*pclm_raster_back_side;

    char 		**pclm_strip_data;
    size_t 		*pclm_strip_data_size;

    char 		*render_intent;
    cups_cspace_t 	color_space;

    char 		*page_data;
    size_t 		page_data_size;
    double page_width;
    double page_height;
    cf_filter_out_format_t outformat;
};

//
// 'init_pdf_info()' - initialise pwgtopdf conversion doc
//

void 
init_pdf_info(struct pdf_info *info) 
{
  info->pdf = NULL;

  info->pagecount = 0;
  info->width = 0;
  info->height = 0;
  info->line_bytes = 0;
  info->bpp = 0;
  info->bpc = 0;

  info->pclm_num_strips = 0;
  info->pclm_strip_height_preferred = 16;

  info->pclm_strip_height = (unsigned*) malloc(2000 * sizeof(unsigned));
  info->pclm_strip_height_size = 0;

  info->pclm_strip_height_supported = (unsigned*) malloc(2000 * sizeof(unsigned));
  info->pclm_strip_height_supported[0] = 1;
  info->pclm_strip_height_supported[1] = 16;
  info->pclm_strip_height_supported_size = 2;

  info->pclm_compression_method_preferred = (compression_method_t*) malloc(2000 * sizeof(compression_method_t));
  info->pclm_compression_method_preferred[0] = 0;
  info->pclm_compression_method_preferred_size = 1;
 
  info->pclm_source_resolution_supported = NULL;
  info->pclm_source_resolution_supported_size = 0;

  info->pclm_source_resolution_default = strdup("");
  info->pclm_raster_back_side = strdup("");
  info->render_intent = strdup("");
  
  info->pclm_strip_data = (char**) malloc(2000 * sizeof(char*));
  info->pclm_strip_data_size = (size_t *) malloc(2000 * sizeof(size_t));

  info->page_dict = NULL;
  info->page = NULL;
  info->page_data = strdup("");
  info->page_data_size = 0;
  info->color_space = CUPS_CSPACE_K;
  info->page_width = 0.0;
  info->page_height = 0.0;
  info->outformat = CF_FILTER_OUT_FORMAT_PDF;
}

// PDF color conversion function
typedef void (*pdf_convert_function)(struct pdf_info *info,
				     pwgtopdf_doc_t *doc);

// 
// 'free_pdf_info()' - Freeing the dynamically allocated memory
//

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

  if (info->page_data)
  {
    free(info->page_data);
    info->page_data = NULL;
  }
}

//
// Bit conversion functions
//

static unsigned char*			  // O - output string of pixels
invert_bits(unsigned char *src,		// I - source chars	
	    unsigned char *dst,		// O - destination chars
	    unsigned int pixels)	// I - pixels
{ 
  unsigned int i;

  // Invert black to grayscale...
  for (i = pixels, dst = src; i > 0; i --, dst ++)
    *dst = ~*dst;

  return (dst);
}	


static unsigned char*			  // O - Output string of bits
no_bit_conversion(unsigned char *src,	// I - Source chars
		  unsigned char *dst,  	// O - destination chars	
		  unsigned int pixels)	// I - Pixesl
{
  return (src);
}


//
// Color conversion functions
//

static unsigned char*			
rgb_to_cmyk(unsigned char *src,
	    unsigned char *dst,
	    unsigned int pixels)
{
  cfImageRGBToCMYK(src, dst, pixels);
  return (dst);
}


static unsigned char*
white_to_cmyk(unsigned char *src,
	      unsigned char *dst,
	      unsigned int pixels)
{
  cfImageWhiteToCMYK(src, dst, pixels);
  return (dst);
}

static unsigned char*
cmyk_to_rgb(unsigned char *src,
	    unsigned char *dst,
	    unsigned int pixels)
{
  cfImageCMYKToRGB(src, dst, pixels);
  return (dst);
}


static unsigned char*
white_to_rgb(unsigned char *src,
	     unsigned char *dst,
	     unsigned int pixels)
{
  cfImageWhiteToRGB(src, dst, pixels);
  return (dst);
}


static unsigned char*
rgb_to_white(unsigned char *src,
	     unsigned char *dst,
	     unsigned int pixels)
{
  cfImageRGBToWhite(src, dst, pixels);
  return (dst);
}


static unsigned char*
cmyk_to_white(unsigned char *src,
	      unsigned char *dst,
	      unsigned int pixels)
{
  cfImageCMYKToWhite(src, dst, pixels);
  return (dst);
}


static unsigned char*
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

char** 
split_strings(const char *str, 
	      const char *delimiters, 
	      size_t *count) 
{
  *count = 0;
  if (!str || *str == '\0') 
    return NULL;

  // Use comma as default delimiter if not specified
  if (!delimiters || *delimiters == '\0') delimiters = ",";

  char **result = NULL;
  char *current = NULL;
  size_t current_len = 0;
  size_t max_tokens = 0;
  bool in_token = false;

  while (*str) 
  {
    if (strchr(delimiters, *str)) 
    {
      if (in_token) 
      {
        current = realloc(current, current_len + 1);
       	current[current_len] = '\0';
      
        if (*count >= max_tokens) 
	{
      	  max_tokens = (*count == 0) ? 4 : max_tokens * 2;
	  result = realloc(result, max_tokens * sizeof(char *));
       	}
       	result[(*count)++] = current;
 
        current = NULL;
        current_len = 0;
        in_token = false;
      }
    } 
    else 
    {
      if (!in_token) 
	in_token = true;
      current = realloc(current, current_len + 1);
      current[current_len++] = *str;
    }
    str++;
  }

  if (in_token) 
  {
    current = realloc(current, current_len + 1);
    current[current_len] = '\0';
    if (*count >= max_tokens) 
    {
      result = realloc(result, (*count + 1) * sizeof(char *));
    }
    result[(*count)++] = current;
  } 
  else 
  {
    free(current);
  }

  result = realloc(result, *count * sizeof(char *));
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
int_to_fwstring(int n, 
		int width) 
{
  int num_zeroes = width - num_digits(n);
  if (num_zeroes < 0)
    num_zeroes = 0;

  int str_size = num_zeroes + num_digits(n) + 1;
  char *result = (char *)malloc(str_size * sizeof(char));
  if (!result) {
    return NULL; // Handle memory allocation failure
  }

  memset(result, '0', num_zeroes);
  snprintf(result + num_zeroes, str_size - num_zeroes, "%d", n);
  return result;
}

//
// 'create_pdf_file()' - create temporar PDF file for output
//

static int
create_pdf_file(struct pdf_info *info,
                cf_filter_out_format_t outformat,
		FILE *outputfp)
{
  char tmp_filename[] = "/tmp/tmpfileXXXXXX";

  int tmp_fd = mkstemp(tmp_filename);
  close(tmp_fd);

  info->pdf = NULL;
  if (outformat == CF_FILTER_OUT_FORMAT_PCLM)
  {
    info->pdf = pdfioFileCreate(tmp_filename, "PCLm-1.0", NULL, NULL, NULL, NULL);
  }
  else
  {
    info->pdf = pdfioFileCreate(tmp_filename, NULL, NULL, NULL, NULL, NULL);
  }

  if (!info->pdf)
    return 1; 

  info->temp_filename = strdup(tmp_filename);
  info->outformat = outformat;
  return 0;
}

static pdfio_rect_t*
make_real_box(double x1,
	      double y1,
	      double x2,
	      double y2)
{
  pdfio_rect_t *rect = (pdfio_rect_t *)malloc(sizeof(pdfio_rect_t));
  rect->x1 = x1;
  rect->y1 = y1;
  rect->x2 = x2;
  rect->y2 = y2;
 
  return rect; 
}

static pdfio_rect_t*
make_integer_box(int x1,
	      int y1,
	      int x2,
	      int y2)
{
  pdfio_rect_t *rect = (pdfio_rect_t *)malloc(sizeof(pdfio_rect_t));
  rect->x1 = x1;
  rect->y1 = y1;
  rect->x2 = x2;
  rect->y2 = y2;
 
  return rect; 
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

static pdfio_obj_t*
embed_icc_profile(pdfio_file_t *pdf, pwgtopdf_doc_t *doc)
{
  if (!doc->colorProfile)
    return NULL;

  pdfio_array_t *array = pdfioArrayCreate(pdf);
  pdfio_dict_t *stream_dict = pdfioDictCreate(pdf);
  const char *n_value = NULL;
  const char *alternate_cs = NULL;
  unsigned char *buff = NULL;
  pdfio_stream_t *stream = NULL;

#ifdef USE_LCMS1
  size_t profile_size;
#else
  unsigned int profile_size;
#endif
    
  // Get color space information
  cmsColorSpaceSignature css = cmsGetColorSpace(doc->colorProfile);
  switch (css) 
  {
    case cmsSigGrayData:
      n_value = "1";
      alternate_cs = "DeviceGray";
      break;
    case cmsSigRgbData:
      n_value = "3";
      alternate_cs = "DeviceRGB";
      break;
    case cmsSigCmykData:
      n_value = "4";
      alternate_cs = "DeviceCMYK";
      break;
    default:
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                        "Failed to embed ICC Profile: Unsupported colorspace");
    return NULL;
 }

  // Create stream dictionary
  pdfioDictSetName(stream_dict, "Alternate", alternate_cs);
  pdfioDictSetName(stream_dict, "N", n_value);

  // Get profile data
  cmsSaveProfileToMem(doc->colorProfile, NULL, &profile_size); 

  buff = (unsigned char*) calloc(profile_size, sizeof(unsigned char));
  cmsSaveProfileToMem(doc->colorProfile, buff, &profile_size);
  
  pdfio_obj_t *stream_obj = pdfioFileCreateObj(pdf, stream_dict);
  stream = pdfioObjCreateStream(stream_obj, PDFIO_FILTER_FLATE);

  pdfioStreamWrite(stream, buff, profile_size);
  pdfioStreamClose(stream);

  array = pdfioArrayCreateColorFromICCObj(pdf, stream_obj);

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		    "ICC Profile embedded in PDF");

  free(buff);
  return pdfioArrayGetObj(array, 0);
}

static pdfio_obj_t*
embed_srgb_profile(pdfio_file_t *pdf,
		   pwgtopdf_doc_t *doc)
{
  pdfio_obj_t *iccbased_reference;

  // Create an sRGB profile from lcms
  doc->colorProfile = cmsCreate_sRGBProfile();
  // Embed it into the profile
  iccbased_reference = embed_icc_profile(pdf, doc);

  return(iccbased_reference);
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
//

static pdfio_array_t*
get_calibration_array(pdfio_file_t *pdf,
		      const char *color_space,
                      double wp[],
                      double gamma[],
                      double matrix[],
                      double bp[])
{ 
  if ((!strcmp("/CalGray", color_space) && matrix != NULL) || 
      wp == NULL)
    return NULL;

  pdfio_array_t *calArray = pdfioArrayCreate(pdf);
  pdfio_dict_t *calDict = pdfioDictCreate(pdf);
  pdfioArrayAppendName(calArray, color_space);

  // WhitePoint
  pdfio_array_t *whitepoint_array = pdfioArrayCreate(pdf);
  pdfioArrayAppendNumber(whitepoint_array, wp[0]);
  pdfioArrayAppendNumber(whitepoint_array, wp[1]);
  pdfioArrayAppendNumber(whitepoint_array, wp[2]);
  pdfioDictSetArray(calDict, "WhitePoint", whitepoint_array);

  fprintf(stderr, "the value of color_space is %s", color_space);
  // Gamma 
  if (!strcmp("CalGray", color_space) && gamma != NULL)
  {
    fprintf(stderr, "Goes in this Gamma1");
    pdfioDictSetNumber(calDict, "Gamma", gamma[0]);
  }
  else if (!strcmp("CalRGB", color_space) && gamma != NULL) 
  {
    fprintf(stderr, "Goes in this Gamma2");
    pdfio_array_t *gamma_array = pdfioArrayCreate(pdf);
    if (gamma_array)
    {
      pdfioArrayAppendNumber(gamma_array, gamma[0]);
      pdfioArrayAppendNumber(gamma_array, gamma[1]);
      pdfioArrayAppendNumber(gamma_array, gamma[2]);
      pdfioDictSetArray(calDict, "Gamma", gamma_array);
    }
  }

  // BlackPoint

  if (bp != NULL)
  {
    fprintf(stderr, "Goes in this BlackPoint1");
    pdfio_array_t *bp_array = pdfioArrayCreate(pdf);
    if (bp_array)
    {
      pdfioArrayAppendNumber(bp_array, bp[0]);
      pdfioArrayAppendNumber(bp_array, bp[1]);
      pdfioArrayAppendNumber(bp_array, bp[2]);
      pdfioDictSetArray(calDict, "BlackPoint", bp_array);
    }
  }

  // Matrix
  if (strcmp(color_space, "CalRGB") == 0 && matrix != NULL)
  {
    fprintf(stderr, "Goes in this Matrix1");
    pdfio_array_t *matrix_array = pdfioArrayCreate(pdf);
    if (matrix_array)
    {
      for (int i = 0; i < 9; i++)
      {
        pdfioArrayAppendNumber(matrix_array, matrix[i]);
      }
      pdfioDictSetArray(calDict, "Matrix", matrix_array);
    }
  }

  pdfioArrayAppendDict(calArray, calDict);
  return calArray;
}

static pdfio_array_t*
get_cal_rgb_array(pdfio_file_t *pdf,
		  double wp[3],
		  double gamma[3],
		  double matrix[9],
		  double bp[3])
{
  pdfio_array_t *ret = get_calibration_array(pdf, "CalRGB", wp, gamma, matrix, bp);
  return ret;
}

static pdfio_array_t*
get_cal_gray_array(pdfio_file_t *pdf,
		   double wp[3],
		   double gamma[1],
		   double bp[3])
{
  pdfio_array_t *ret = get_calibration_array(pdf, "CalGray", wp, gamma, 0, bp);
  return ret;
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
static pdfio_obj_t**
make_pclm_strips(pdfio_file_t *pdf,
		 unsigned num_strips,
		 char **strip_data,
		 size_t *strip_data_size,
		 compression_method_t *compression_methods,
		 unsigned width, unsigned *strip_height,
		 cups_cspace_t cs,
		 unsigned bpc,
		 pwgtopdf_doc_t *doc)
{
  pdfio_obj_t **strips = (pdfio_obj_t **)malloc(num_strips * sizeof(pdfio_obj_t *));

  // Determine color space
  const char *color_space;
  switch (cs) 
  {
    case CUPS_CSPACE_K:
    case CUPS_CSPACE_SW:
      color_space = "DeviceGray";
      break;
    case CUPS_CSPACE_RGB:
    case CUPS_CSPACE_SRGB:
    case CUPS_CSPACE_ADOBERGB:
      color_space = "DeviceRGB";
      break;
    default:
      if (doc->logfunc) doc->logfunc(doc->logdata, 1, 
		     		     "Unsupported color space");
      free(strips);
     return NULL;
  }

  //
  // We deliver already compressed content (instead of letting QPDFWriter
  // do it) to avoid using excessive memory. For that we first get preferred
  // compression method to pre-compress content for strip streams.
  //
  // Use the compression method with highest priority of the available methods
  // __________________
  // Priority | Method
  // ------------------
  // 0        | DCT
  // 1        | FLATE
  // 2        | RLE
  // ------------------
  //

  compression_method_t compression = compression_methods[0];
  for (unsigned i = 1; i < num_strips; i++) {
        if (compression_methods[i] > compression) {
            compression = compression_methods[i];
        }
    }


  for (size_t i = 0; i < num_strips; i ++)
  {
    pdfio_dict_t *dict = pdfioDictCreate(pdf);
    pdfioDictSetName(dict, "Type", "XObject");
    pdfioDictSetName(dict, "Subtype", "Image");
    pdfioDictSetNumber(dict, "Width", width);
    pdfioDictSetNumber(dict, "Height", strip_height[i]);
    pdfioDictSetName(dict, "ColorSpace", color_space);
    pdfioDictSetNumber(dict, "BitsPerComponent", bpc);
    
    pdfio_obj_t *ret=NULL;
    if (compression == FLATE_DECODE)
    {
      pdfioDictSetName(dict, "Filter", "FlateDecode");
      ret =  pdfioFileCreateObj(pdf, dict);
      pdfio_stream_t *stream = pdfioObjCreateStream(ret, PDFIO_FILTER_FLATE);
      pdfioStreamWrite(stream, strip_data[i], strip_data_size[i]);
      pdfioStreamClose(stream);
    }
    else if (compression == RLE_DECODE)
    {
      pdfioDictSetName(dict, "Filter", "RunLengthDecode");
      ret =  pdfioFileCreateObj(pdf, dict);
      pdfio_stream_t *stream = pdfioObjCreateStream(ret, PDFIO_FILTER_FLATE);
      pdfioStreamWrite(stream, strip_data[i], strip_data_size[i]);
      pdfioStreamClose(stream);
    }
    else if (compression == DCT_DECODE)
    {
      pdfioDictSetName(dict, "Filter", "DCTDecode");
      ret =  pdfioFileCreateObj(pdf, dict);
      pdfio_stream_t *stream = pdfioObjCreateStream(ret, PDFIO_FILTER_DCT);
      pdfioStreamWrite(stream, strip_data[i], strip_data_size[i]);
      pdfioStreamClose(stream);
    }

    if(ret != NULL)
      strips[i] = ret;
    else
      fprintf(stderr, "ERROR: strip values are blank");
  }
  return strips;
}


static pdfio_obj_t* 
make_image(pdfio_file_t *pdf,
	   char *page_data,
	   size_t page_data_size,
	   unsigned width,
	   unsigned height,
	   char *render_intent,
	   cups_cspace_t cs,
	   unsigned bpc,
	   pwgtopdf_doc_t *doc)
{
  pdfio_obj_t *icc_ref;
  pdfio_dict_t *image_dict = pdfioDictCreate(pdf);
  pdfioDictSetName(image_dict, "Type", "XObject");
  pdfioDictSetName(image_dict, "Subtype", "Image");
  pdfioDictSetNumber(image_dict, "Width", width);
  pdfioDictSetNumber(image_dict, "Height", height);
  pdfioDictSetNumber(image_dict, "BitsPerComponent", bpc);
  int use_blackpoint = 0;

  if (!doc->cm_disabled)
  {
    // Write rendering intent into the PDF based on raster settings
    if (strcmp(render_intent, "Perceptual") == 0)
      pdfioDictSetName(image_dict, "Intent", "Perceptual");
    else if (strcmp(render_intent, "Absolute") == 0)
      pdfioDictSetName(image_dict, "Intent", "AbsoluteColorimetric");
    else if (strcmp(render_intent, "Relative") == 0)
      pdfioDictSetName(image_dict, "Intent", "RelativeColorimetric");
    else if (strcmp(render_intent, "Saturation") == 0)
      pdfioDictSetName(image_dict, "Intent", "Saturation");
    else if (strcmp(render_intent, "RelativeBpc") == 0)
    {
      // Enable blackpoint compensation
      pdfioDictSetName(image_dict, "Intent", "RelativeColorimetric");
      use_blackpoint = 1;
    }
  }

  // Write "/ColorSpace" dictionary based on raster input
  if (doc->colorProfile != NULL && !doc->cm_disabled)
  {
    icc_ref = embed_icc_profile(pdf, doc);

    if (icc_ref != NULL)
    {
      pdfioDictSetObj(image_dict, "ColorSpace", icc_ref);
    }
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
	  // For right now, DeviceN will use /DeviceCMYK in the PDF
          pdfioDictSetName(image_dict, "ColorSpace", "DeviceCMYK");	
	  break;
      case CUPS_CSPACE_K:
	  pdfioDictSetName(image_dict, "ColorSpace", "DeviceGray");
	  break;
      case CUPS_CSPACE_SW:
	  if (use_blackpoint)
	  {
	    pdfio_array_t *gray_array = get_cal_gray_array(pdf, 
			    			           cfCmWhitePointSGray(), 
							   cfCmGammaSGray(), 
							   cfCmBlackPointDefault());
	    pdfioDictSetArray(image_dict, "ColorSpace",  gray_array);
	  }
	  else
	  {
            pdfio_array_t *gray_array = get_cal_gray_array(pdf,
			   			 	   cfCmWhitePointSGray(),
							   cfCmGammaSGray(), 0);
	    pdfioDictSetArray(image_dict, "ColorSpace", gray_array); 
	  }
	  break;
      case CUPS_CSPACE_CMYK:
	  pdfioDictSetName(image_dict, "ColorSpace", "DeviceCMYK");
	  break;
      case CUPS_CSPACE_RGB:
	  pdfioDictSetName(image_dict, "ColorSpace", "DeviceRGB");
	  break;
      case CUPS_CSPACE_SRGB:
	  icc_ref = embed_srgb_profile(pdf, doc);
	  if(icc_ref!=NULL)
	    pdfioDictSetObj(image_dict, "ColorSpace", icc_ref);
	  else
	    pdfioDictSetName(image_dict, "ColorSpace", "DeviceRGB");
	  break;
      case CUPS_CSPACE_ADOBERGB:
          if(use_blackpoint)
	    pdfioDictSetArray(image_dict, "ColorSpace", get_cal_rgb_array(pdf, 
				             				cfCmWhitePointAdobeRGB(),
								       	cfCmGammaAdobeRGB(), 
									cfCmMatrixAdobeRGB(),
								       	cfCmBlackPointDefault()));
	  else
	    pdfioDictSetArray(image_dict, "ColorSpace", get_cal_rgb_array(pdf,
				    					cfCmWhitePointAdobeRGB(),
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
          pdfioDictSetName(image_dict, "ColorSpace", "DeviceGray");
	  break;
      case CUPS_CSPACE_RGB:
      case CUPS_CSPACE_SRGB:
      case CUPS_CSPACE_ADOBERGB:
          pdfioDictSetName(image_dict, "ColorSpace", "DeviceRGB");
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
          pdfioDictSetName(image_dict, "ColorSpace", "DeviceCMYK");
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
	

#ifdef PRE_COMPRESS
  // we deliver already compressed content (instead of letting QPDFWriter
  // do it), to avoid using excessive memory
  pdfioDictSetName(image_dict, "Filter", "FlateDecode");
  pdfio_obj_t *ret =  pdfioFileCreateObj(pdf, image_dict);
  pdfio_stream_t *stream = pdfioObjCreateStream(ret, PDFIO_FILTER_FLATE);
  pdfioStreamWrite(stream, page_data, page_data_size);  
  pdfioStreamClose(stream);

#else
  pdfio_obj_t *ret =  pdfioFileCreateObj(pdf, image_dict);
  pdfio_stream_t *stream = pdfioObjCreateStream(ret, PDFIO_FILTER_NONE);
  pdfioStreamWrite(stream, page_data, page_data_size);
  pdfioStreamClose(stream);
#endif

  return ret;
}
	    

static int
finish_page(struct pdf_info *info,
	    pwgtopdf_doc_t *doc)
{
  if (info->outformat == CF_FILTER_OUT_FORMAT_PDF)
  {
    // Finish PDF page
    if (!info->page_data)
      return 0;

    pdfio_obj_t *image = make_image(info->pdf, info->page_data, info->page_data_size,
		    		    info->width, info->height,
				    info->render_intent,
				    info->color_space, info->bpc, doc);
    if (!image)
    {
      if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                            "cfFilterPWGToPDF: Unable to load image data");
      return 1;
    }

    pdfioPageDictAddImage(info->page_dict, "I", image);
    info->page_stream = pdfioFileCreatePage(info->pdf, info->page_dict);  
  }
  else if (info->outformat == CF_FILTER_OUT_FORMAT_PCLM)
  {
    // Finish previous PCLm page
    if (info->pclm_num_strips == 0)
      return (0);
 
    for (unsigned i = 0; i < info->pclm_strip_data_size[i]; i ++)
    {
      if(info->pclm_strip_data[i])
        return 0; 
    }

    pdfio_obj_t **strips = 
      make_pclm_strips(info->pdf, info->pclm_num_strips, info->pclm_strip_data, 
		       info->pclm_strip_data_size, 
		       info->pclm_compression_method_preferred, info->width,
		       info->pclm_strip_height, info->color_space, info->bpc,
		       doc);

    for (unsigned i = 0; i < info->pclm_num_strips; i ++)
    {
      if (!strips[i])
      {
	if (doc->logfunc)
	  doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		       "cfFilterPWGToPDF: Unable to load strip data");
	return (1);
      }
    }

    //Add it
    for (size_t i = 0; i < info->pclm_num_strips; i ++)
      pdfioPageDictAddImage(info->page_dict, "I", strips[i]);
  }

  // Draw it
  if(info->outformat == CF_FILTER_OUT_FORMAT_PDF)
  {
    char transform_cmd[256];
    int bytes = snprintf(transform_cmd, sizeof(transform_cmd),
                         "q\n%.2f 0 0 %.2f 0 0 cm\n/I Do\niQ\n",
                         info->page_width, info->page_height);
    if (bytes < 0 || bytes >= (int)sizeof(transform_cmd) ||
        !pdfioStreamWrite(info->page_stream, transform_cmd, (size_t)bytes))
      goto draw_error;
  }
  else if(info->outformat == CF_FILTER_OUT_FORMAT_PCLM)
  {
    const char *resolution = info->pclm_source_resolution_default;
    char scale_cmd[64];
    char strip_cmd[512];
    int id_width = num_digits(info->pclm_num_strips - 1);
    unsigned yAnchor = info->height;
    if (!resolution || !*resolution)
      resolution = "300";
    long resolution_integer = strtol(resolution, NULL, 10);
    if (resolution_integer <= 0)
      resolution_integer = 300;
    double d = 72.0 / (double)resolution_integer;
    int bytes = snprintf(scale_cmd, sizeof(scale_cmd), "%.2f 0 0 %.2f 0 0 cm\n", d, d);
    if (bytes < 0 || bytes >= (int)sizeof(scale_cmd) ||
        !pdfioStreamWrite(info->page_stream, scale_cmd, (size_t)bytes))
      goto draw_error;
    for (unsigned i = 0; i < info->pclm_num_strips; i++)
    {
      yAnchor -= info->pclm_strip_height[i];
      bytes = snprintf(strip_cmd, sizeof(strip_cmd),
                       "/P <</MCID 0>> BDC q\n%u 0 0 %u 0 %u cm\n/Image%0*d Do Q\n",
                       info->width,
                       info->pclm_strip_height[i],
                       yAnchor,
                       id_width, i);
      if (bytes < 0 || bytes >= (int)sizeof(strip_cmd) ||
          !pdfioStreamWrite(info->page_stream, strip_cmd, (size_t)bytes))
        goto draw_error;
    }
  }

  pdfioStreamClose(info->page_stream);
  return 0;

draw_error:
  if (doc->logfunc)
    doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR,
                 "cfFilterPWGToPDF: Failed to write page drawing commands.");
  pdfioStreamClose(info->page_stream);
  return 1;
}

//
// Perform modifications
//
// Perform modifications to PDF if color space conversions are needed
//

static int
prepare_pdf_page(struct pdf_info *info,
		 unsigned width,
		 unsigned height,
		 unsigned bpl,
		 unsigned bpp,
		 unsigned bpc,
		 char *render_intent,
		 cups_cspace_t color_space,
		 pwgtopdf_doc_t *doc)
{
#define IMAGE_CMYK_8   (bpp == 32 && bpc == 8)
#define IMAGE_CMYK_16  (bpp == 64 && bpc == 16)
#define IMAGE_RGB_8    (bpp == 24 && bpc == 8)
#define IMAGE_RGB_16   (bpp == 48 && bpc == 16)
#define IMAGE_WHITE_1  (bpp == 1 && bpc == 1)
#define IMAGE_WHITE_8  (bpp == 8 && bpc == 8)
#define IMAGE_WHITE_16 (bpp == 16 && bpc == 16)    

  int error = 0;
  pdf_convert_function fn = convert_pdf_no_conversion;
  cmsColorSpaceSignature css;

  // Register available raster information into the PDF
  info->width = width;
  info->height = height;
  info->line_bytes = bpl;
  info->bpp = bpp;
  info->bpc = bpc;
  info->render_intent = render_intent;
  info->color_space = color_space;
  if (info->outformat == CF_FILTER_OUT_FORMAT_PCLM)
  {
    info->pclm_num_strips =
      (height / info->pclm_strip_height_preferred) +
      (height % info->pclm_strip_height_preferred ? 1 : 0);

    info->pclm_strip_height = (unsigned *)realloc(info->pclm_strip_height,
						  info->pclm_num_strips * sizeof(unsigned));
    info->pclm_strip_data = (char **)realloc(info->pclm_strip_data,
					     info->pclm_num_strips * sizeof(char *));
    info->pclm_strip_data_size = (size_t *)realloc(info->pclm_strip_data_size,
						   info->pclm_num_strips * sizeof(size_t));
    if (!info->pclm_strip_height || !info->pclm_strip_data || !info->pclm_strip_data_size)
    {
      if (doc->logfunc)
        doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR,
                     "cfFilterPWGToPDF: Unable to allocate strip metadata");
      return (1);
    }
    memset(info->pclm_strip_data, 0, info->pclm_num_strips * sizeof(char *));
    memset(info->pclm_strip_data_size, 0, info->pclm_num_strips * sizeof(size_t));
    for (size_t i = 0; i < info->pclm_num_strips; i ++)
    {
      info->pclm_strip_height[i] =
	info->pclm_strip_height_preferred < height ?
	info->pclm_strip_height_preferred : height;
      height -= info->pclm_strip_height[i];
    }
  }

  // Invert grayscale by default
  if (color_space == CUPS_CSPACE_K)
    fn = convert_pdf_invert_colors;

  if (doc->colorProfile != NULL)
  {
    css = cmsGetColorSpace(doc->colorProfile);

    // Convert image and PDF color space to an embedded ICC Profile color
    // space
    switch (css)
    {
      // Convert PDF to Grayscale when using a gray profile
      case cmsSigGrayData:
          if (color_space == CUPS_CSPACE_CMYK)
	    fn = convert_pdf_cmyk_8_to_white_8;
	  else if (color_space == CUPS_CSPACE_RGB) 
	    fn = convert_pdf_rgb_8_to_white_8;
	  else              
	    fn = convert_pdf_invert_colors;
	  info->color_space = CUPS_CSPACE_K;
	  break;
      // Convert PDF to RGB when using an RGB profile
      case cmsSigRgbData:
          if (color_space == CUPS_CSPACE_CMYK) 
	    fn = convert_pdf_cmyk_8_to_rgb_8;
	  else if (color_space == CUPS_CSPACE_K) 
	   fn = convert_pdf_white_8_to_rgb_8;
	  info->color_space = CUPS_CSPACE_RGB;
	  break;
      // Convert PDF to CMYK when using an RGB profile
      case cmsSigCmykData:
          if (color_space == CUPS_CSPACE_RGB)
            fn = convert_pdf_rgb_8_to_cmyk_8;
          else if (color_space == CUPS_CSPACE_K) 
            fn = convert_pdf_white_8_to_cmyk_8;
          info->color_space = CUPS_CSPACE_CMYK;
          break;
      default:
	  if (doc->logfunc)
	    doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
			 "cfFilterPWGToPDF: Unable to convert PDF from profile.");
          doc->colorProfile = NULL;
          error = 1;
    }
  }
  else if (!doc->cm_disabled)
  {
    // Perform conversion of an image color space 
    switch (color_space)
    {
      // Convert image to CMYK
      case CUPS_CSPACE_CMYK:
          if (IMAGE_RGB_8)
	    fn = convert_pdf_rgb_8_to_cmyk_8;  
	  else if (IMAGE_RGB_16)
	    fn = convert_pdf_no_conversion;
	  else if (IMAGE_WHITE_8)
	    fn = convert_pdf_white_8_to_cmyk_8;  
	  else if (IMAGE_WHITE_16) 
	    fn = convert_pdf_no_conversion;
	  break;
      // Convert image to RGB
      case CUPS_CSPACE_ADOBERGB:
      case CUPS_CSPACE_RGB:
      case CUPS_CSPACE_SRGB:
           if (IMAGE_CMYK_8)
             fn = convert_pdf_cmyk_8_to_rgb_8;
           else if (IMAGE_CMYK_16)
             fn = convert_pdf_no_conversion;  
           else if (IMAGE_WHITE_8)
             fn = convert_pdf_white_8_to_rgb_8;
           else if (IMAGE_WHITE_16) 
             fn = convert_pdf_no_conversion;       
           break;
      // Convert image to Grayscale
      case CUPS_CSPACE_SW:
      case CUPS_CSPACE_K:
	  if (IMAGE_CMYK_8)
	    fn = convert_pdf_cmyk_8_to_white_8;
	  else if (IMAGE_CMYK_16)
	    fn = convert_pdf_no_conversion;
	  else if (IMAGE_RGB_8) 
	    fn = convert_pdf_rgb_8_to_white_8;
	  else if (IMAGE_RGB_16) 
	    fn = convert_pdf_no_conversion;
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
          // No conversion for right now
	  fn = convert_pdf_no_conversion;
	  break;
      default:
	  if (doc->logfunc)
	    doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
			 "cfFilterPWGToPDF: Color space not supported.");
	  error = 1;
	  break;
    }
  }

  if (!error)
    fn(info, doc);

  return (error);
}


static int
add_pdf_page(struct pdf_info *info,
	     int pagen,
	     unsigned width,
	     unsigned height,
	     int bpp,
	     int bpc,
	     int bpl,
	     char *render_intent,
	     cups_cspace_t color_space,
	     unsigned xdpi,
	     unsigned ydpi,
	     pwgtopdf_doc_t *doc)
{
  prepare_pdf_page(info, width, height, bpl, bpp,
		   bpc, render_intent, color_space, doc);  

  pdfio_dict_t *page_dict = pdfioDictCreate(info->pdf); 
  info->page_width = ((double)info->width / xdpi) * DEFAULT_PDF_UNIT;
  info->page_height = ((double)info->height / ydpi) * DEFAULT_PDF_UNIT;
  if (info->outformat == CF_FILTER_OUT_FORMAT_PDF)
  {
    pdfio_dict_t *content_dict = pdfioDictCreate(info->pdf);
    pdfioDictSetObj(page_dict, "Contents", pdfioFileCreateObj(info->pdf, content_dict));
    pdfioDictSetRect(page_dict, "MediaBox", make_real_box(0, 0, info->page_width,
			    				     info->page_height));
    pdfioDictSetRect(page_dict, "CropBox", make_real_box(0, 0, info->page_width,
			    				     info->page_height));
  }
  else if(info->outformat == CF_FILTER_OUT_FORMAT_PCLM)
  {
    pdfio_dict_t *content_dict = pdfioDictCreate(info->pdf);
    pdfioDictSetObj(page_dict, "Contents", pdfioFileCreateObj(info->pdf, content_dict));
    pdfioDictSetRect(page_dict, "MediaBox", make_integer_box(0, 0, info->page_width + 0.5,
			    				     info->page_height + 0.5));
    pdfioDictSetRect(page_dict, "CropBox", make_integer_box(0, 0, info->page_width + 0.5,
			    				     info->page_height + 0.5));
  }
  
  info->page_dict = page_dict;
 
  if(info->height > UINT_MAX / info->line_bytes)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		     "cfFilterPWGToPDF: Page too big");
    return (1);
  }
  
  if (info->outformat == CF_FILTER_OUT_FORMAT_PDF) 
  {
    info->page_data_size = info->line_bytes * info->height;
    info->page_data = malloc(info->page_data_size);
    memset(info->page_data, 0, info->page_data_size);
  } 
  else if (info->outformat == CF_FILTER_OUT_FORMAT_PCLM) 
  {
      // reserve space for PCLm strips
    for (size_t i = 0; i < info->pclm_num_strips; i ++)
    {
      info->pclm_strip_data_size[i] = (size_t)(info->line_bytes * info->pclm_strip_height[i]);
      info->pclm_strip_data[i] = (char *)malloc(info->pclm_strip_data_size[i] * sizeof(char));
      memset(info->pclm_strip_data[i], 0, info->pclm_strip_data_size[i]);
    }
  }

  return 0;
}

static int 
close_pdf_file(struct pdf_info *info,
	       pwgtopdf_doc_t *doc)
{
  if (!pdfioFileClose(info->pdf))
  {
    if (doc->logfunc)
      doc->logfunc(doc->logdata, CF_LOGLEVEL_ERROR,
                   "Failed to finalize PDF file");
    return 1;
  }

  return 0;

}

static void
pdf_set_line(struct pdf_info *info,
             unsigned line_n,
             unsigned char *line,
             pwgtopdf_doc_t *doc)
{
  if (line_n > info->height) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                    "cfFilterPWGToPDF: Bad line %d", line_n);
    return;
  }

  if (info->outformat == CF_FILTER_OUT_FORMAT_PCLM) 
  {
    // copy line data into appropriate pclm strip
    size_t strip_num = line_n / info->pclm_strip_height_preferred;
    unsigned line_strip = line_n - strip_num * info->pclm_strip_height_preferred;
    memcpy(info->pclm_strip_data[strip_num] + (line_strip * info->line_bytes), line,
           info->line_bytes);
    info->pclm_strip_data_size[strip_num] = info->line_bytes;
 
  } 
  else 
  {
    if (info->page_data) 
    {
      unsigned char *page_ptr = (unsigned char*)info->page_data + (line_n * info->line_bytes);
      memcpy(page_ptr, line, info->line_bytes);
    }
  }
}

static int
convert_raster(cups_raster_t *ras,
               unsigned width,
               unsigned height,
               int bpp,
               int bpl,
               struct pdf_info *info,
               pwgtopdf_doc_t *doc)
{
  int i;
  unsigned cur_line = 0;
  unsigned char *PixelBuffer, *ptr = NULL, *buff;

  if (!ras || !info || bpl <= 0) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG, 
		    "Invalid raster conversion parameters");
    return 1;
  }

  PixelBuffer = (unsigned char *)malloc(bpl);
  buff = (unsigned char *)malloc(info->line_bytes);
  
  while (cur_line < height) 
  {
    // Read raster data...
    cupsRasterReadPixels(ras, PixelBuffer, bpl);

#if !ARCH_IS_BIG_ENDIAN
    if (info->bpc == 16) 
    {
      // Swap byte pairs for endianess (cupsRasterReadPixels() switches
      // from Big Endian back to the system's Endian)
      for (i = bpl, ptr = PixelBuffer; i > 0; i -= 2, ptr += 2) 
      {
        unsigned char swap = *ptr;
	*ptr = *(ptr + 1);
	*(ptr + 1) = swap;
      }
    }
#endif

    // perform bit operations if necessary
    doc->bit_function(PixelBuffer, buff, width);

    // write lines and color convert when necessary
    pdf_set_line(info, cur_line, doc->conversion_function(PixelBuffer, 
			     				  buff, width),
		 doc);
    ++cur_line;
  }
  
  free(buff);
  free(PixelBuffer);
 
  return 0;
}
   
static int
set_profile(const char *path,
	    pwgtopdf_doc_t *doc)
{
  if (path != NULL)
    doc->colorProfile = cmsOpenProfileFromFile(path, "r");

  if (doc->colorProfile != NULL)
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		   "cfFilterPWGToPDF: Load profile successful.");
    return (0);
  }

  if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
		   "cfFilterPWGToPDF: Unable to load profile.");
  return (1);
}

int                            // O - Error status
cfFilterPWGToPDF(int inputfd,  // I - File descriptor input stream
       int outputfd,           // I - File descriptor output stream
       int inputseekable,      // I - Is input stream seekable? (unused)
       cf_filter_data_t *data, // I - Job and printer data
       void *parameters)       // I - Filter-specific parameters (outformat)
{
  cups_len_t i;
  char *t;
  pwgtopdf_doc_t	doc;		// Document information
  FILE          	*outputfp;      // Output data stream
  cf_filter_out_format_t outformat;     // Output format
  int			Page, empty = 1;

  // Initialize doc structure to prevent use of uninitialized memory
  memset(&doc, 0, sizeof(doc));
  cf_cm_calibration_t	cm_calibrate;   // Status of CUPS color management
					// ("on" or "off")
  struct pdf_info pdf;
  cups_raster_t		*ras;		// Raster stream for printing
  cups_page_header_t	header;		// Page header from file
  ipp_t *printer_attrs = data->printer_attrs; // Printer attributes from
					// printer data
  ipp_attribute_t	*ipp_attr;	// Printer attribute
  const char		*profile_name = NULL; // IPP Profile Name
  cf_logfunc_t		log = data->logfunc;
  void			*ld = data->logdata;
  cf_filter_iscanceledfunc_t iscanceled = data->iscanceledfunc;
  void			*icd = data->iscanceleddata;
  int			total_attrs;
  char			buf[1024];
  const char		*kw;
  int			ret = -1;	// Return value

  init_pdf_info(&pdf);

  (void)inputseekable;

  if (parameters)
  {
    outformat = *(cf_filter_out_format_t *)parameters;
    if (outformat != CF_FILTER_OUT_FORMAT_PCLM)
      outformat = CF_FILTER_OUT_FORMAT_PDF;
  }
  else
  {
    t = data->final_content_type;
    if (t)
    {
      if (strcasestr(t, "pclm"))
	outformat = CF_FILTER_OUT_FORMAT_PCLM;
      else if (strcasestr(t, "pdf"))
	outformat = CF_FILTER_OUT_FORMAT_PDF;
      else
	outformat = CF_FILTER_OUT_FORMAT_PDF;
    }
    else
      outformat = CF_FILTER_OUT_FORMAT_PDF;
  }

  if (log) log(ld, CF_LOGLEVEL_DEBUG,
	       "cfFilterPWGToPDF: OUTFORMAT=\"%s\"",
	       outformat == CF_FILTER_OUT_FORMAT_PDF ? "PDF" : "PCLM");

  //
  // Open the output data stream specified by the outputfd...
  //

  if ((outputfp = fdopen(outputfd, "w")) == NULL)
  {
    if (!iscanceled || !iscanceled(icd))
    {
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
		   "cfFilterPWGToPDF: Unable to open output data stream.");
    }

    return (1);
  }

  doc.outputfp = outputfp;
  // Logging function
  doc.logfunc = log;
  doc.logdata = ld;
  // Job-is-canceled function
  doc.iscanceledfunc = iscanceled;
  doc.iscanceleddata = icd;

  // support the CUPS "cm-calibration" option
  cm_calibrate = cfCmGetCupsColorCalibrateMode(data);

  if (outformat == CF_FILTER_OUT_FORMAT_PCLM ||
      cm_calibrate == CF_CM_CALIBRATION_ENABLED)
    doc.cm_disabled = 1;
  else
    doc.cm_disabled = cfCmIsPrinterCmDisabled(data);

  if (outformat == CF_FILTER_OUT_FORMAT_PCLM && printer_attrs == NULL)
  {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
      "cfFilterPWGToPDF: PCLm output: No printer IPP attributes are supplied, PCLm output not possible.");
    fclose(outputfp);
    return (1);
  }

  // Transform
  ras = cupsRasterOpen(inputfd, CUPS_RASTER_READ);

  // Process pages as needed...
  Page = 0;

  // Get PCLm parameters from printer IPP attributes
  if (outformat == CF_FILTER_OUT_FORMAT_PCLM)
  {
    if (log)
    {
      log(ld, CF_LOGLEVEL_DEBUG, "PCLm-related printer IPP attributes:");
      total_attrs = 0;
      ipp_attr = ippGetFirstAttribute(printer_attrs);
      while (ipp_attr)
      {
        if (strncmp(ippGetName(ipp_attr), "pclm-", 5) == 0)
        {
          total_attrs ++;
          ippAttributeString(ipp_attr, buf, sizeof(buf));
          log(ld, CF_LOGLEVEL_DEBUG, "  Attr: %s",ippGetName(ipp_attr));
          log(ld, CF_LOGLEVEL_DEBUG, "  Value: %s", buf);
          for (i = 0; i < ippGetCount(ipp_attr); i ++)
            if ((kw = ippGetString(ipp_attr, i, NULL)) != NULL)
	      log(ld, CF_LOGLEVEL_DEBUG, "  Keyword: %s", kw);
	}
	ipp_attr = ippGetNextAttribute(printer_attrs);
      }
      log(ld, CF_LOGLEVEL_DEBUG, "  %d attributes", total_attrs);
    }
    
    //setting up of pclm-strip-height-preferred
    char *attr_name = (char *)"pclm-strip-height-preferred";
    if ((ipp_attr = ippFindAttribute(printer_attrs, attr_name,
				     IPP_TAG_ZERO)) != NULL)
    {
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
		  "cfFilterPWGToPDF: Printer PCLm attribute \"%s\" with value %d",
		  attr_name, ippGetInteger(ipp_attr, 0));
      pdf.pclm_strip_height_preferred = ippGetInteger(ipp_attr, 0);
    }
    else
      pdf.pclm_strip_height_preferred = 16; // default strip height

    //setting up of pclm-strip-height-supported
    attr_name = (char *)"pclm-strip-height-supported";
    if ((ipp_attr = ippFindAttribute(printer_attrs, attr_name,
				     IPP_TAG_ZERO)) != NULL)
    {
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
		   "cfFilterPWGToPDF: Printer PCLm attribute \"%s\"",
		   attr_name);
      
      free(pdf.pclm_strip_height_supported);   // remove default value = 16
      pdf.pclm_strip_height_supported = NULL;  // remove default value = 16
      pdf.pclm_strip_height_supported_size = 0; // remove default value = 16

      int count = ippGetCount(ipp_attr);
      if (count > 0)
      {
        pdf.pclm_strip_height_supported = (unsigned *)realloc(
          pdf.pclm_strip_height_supported, count * sizeof(unsigned));


        for (int i = 0; i < count; i++)
        {
          pdf.pclm_strip_height_supported[i] = ippGetInteger(ipp_attr, i);
        }
        pdf.pclm_strip_height_supported_size = count;
      }
    }

    //setting up of pclm-raster-back-side
    attr_name = (char *)"pclm-raster-back-side";
    if ((ipp_attr = ippFindAttribute(printer_attrs, attr_name,
				     IPP_TAG_ZERO)) != NULL)
    {
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
		   "cfFilterPWGToPDF: Printer PCLm attribute \"%s\" with value \"%s\"",
		   attr_name, ippGetString(ipp_attr, 0, NULL));
      pdf.pclm_raster_back_side = (char *)ippGetString(ipp_attr, 0, NULL);
    }
    
    //setting up of pclm-source-resolution-supported
    attr_name = (char *) "pclm-source-resolution-supported";
    if ((ipp_attr = ippFindAttribute(printer_attrs, attr_name,
				     IPP_TAG_ZERO)) != NULL)
    {
      ippAttributeString(ipp_attr, buf, sizeof(buf));
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
                   "cfFilterPWGToPDF: Printer PCLm attribute \"%s\" with value \"%s\"",
                   attr_name, buf);
      size_t num_resolutions = 0;
      if (pdf.pclm_source_resolution_supported)
      {
        for (size_t i = 0; i < pdf.pclm_source_resolution_supported_size; i++)
          free(pdf.pclm_source_resolution_supported[i]);
        free(pdf.pclm_source_resolution_supported);
      }
      pdf.pclm_source_resolution_supported = split_strings(buf, ",", &num_resolutions);
      pdf.pclm_source_resolution_supported_size = num_resolutions;
    }

    //setting up of pclm-source-resolution-default
    attr_name = (char *)"pclm-source-resolution-default";
    if ((ipp_attr = ippFindAttribute(printer_attrs, attr_name,
				     IPP_TAG_ZERO)) != NULL)
    {
      ippAttributeString(ipp_attr, buf, sizeof(buf));
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
                "cfFilterPWGToPDF: Printer PCLm attribute \"%s\" with value \"%s\"",
                attr_name, buf);
      free(pdf.pclm_source_resolution_default);
      pdf.pclm_source_resolution_default = strdup(buf);
    }
    else if (pdf.pclm_source_resolution_supported_size > 0)
    {
      free(pdf.pclm_source_resolution_default);
      pdf.pclm_source_resolution_default = strdup(pdf.pclm_source_resolution_supported[0]);
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
                "cfFilterPWGToPDF: Printer PCLm attribute \"%s\" missing, taking first item of \"pclm-source-resolution-supported\" as default resolution",
                attr_name);
    }
    else
    {
      if (log) log(ld, CF_LOGLEVEL_ERROR,
		   "cfFilterPWGToPDF: PCLm output: Printer IPP attributes do not contain printer resolution information for PCLm.");
      ret = 1;
      goto error;
    }

    //setting up of pclm-compression-method-preferred
    attr_name = (char *)"pclm-compression-method-preferred";
    if ((ipp_attr = ippFindAttribute(printer_attrs, attr_name,
				     IPP_TAG_ZERO)) != NULL)
    {
      ippAttributeString(ipp_attr, buf, sizeof(buf));
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
                   "cfFilterPWGToPDF: Printer PCLm attribute \"%s\" with value \"%s\"",
                   attr_name, buf);
      size_t num_methods = 0;
      char **methods = split_strings(buf, ",", &num_methods);
      pdf.pclm_compression_method_preferred_size = 0;
      for (size_t i = 0; i < num_methods; i++)
      {
        char *method = methods[i];
        // Convert to lowercase
        for (char *p = method; *p; p++)
          *p = tolower(*p);
        if (strcmp(method, "flate") == 0)
        {
          if (pdf.pclm_compression_method_preferred_size < 2000)
            pdf.pclm_compression_method_preferred[pdf.pclm_compression_method_preferred_size++] = FLATE_DECODE;
        }
        else if (strcmp(method, "rle") == 0)
        {
          if (pdf.pclm_compression_method_preferred_size < 2000)
            pdf.pclm_compression_method_preferred[pdf.pclm_compression_method_preferred_size++] = RLE_DECODE;
        }
        else if (strcmp(method, "jpeg") == 0)
        {
          if (pdf.pclm_compression_method_preferred_size < 2000)
            pdf.pclm_compression_method_preferred[pdf.pclm_compression_method_preferred_size++] = DCT_DECODE;
        }
        free(methods[i]);
      }
      free(methods);
    }
      
    // If the compression methods is none of the above or is erreneous
    // use FLATE as compression method and show a warning.
    if (pdf.pclm_compression_method_preferred_size == 0)
    {
      if (log) log(ld, CF_LOGLEVEL_WARN,
                   "(pwgtopclm) Unable to parse Printer attribute \"%s\". Using FLATE for encoding image streams.",
                   attr_name);
      pdf.pclm_compression_method_preferred[0] = FLATE_DECODE;
      pdf.pclm_compression_method_preferred_size = 1;
    }
  }

  while (cupsRasterReadHeader(ras, &header))
  {
    if (iscanceled && iscanceled(icd))
    {
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
		   "cfFilterPWGToPDF: Job canceled");
      break;
    }

    if (empty)
    {
      empty = 0;
      if (create_pdf_file(&pdf, outformat, outputfp) != 0)
      {
	if (log) log(ld, CF_LOGLEVEL_ERROR,
		     "cfFilterPWGToPDF: Unable to create PDF file");
	ret = 1;
	goto error;
      }
    }

    // Write a status message with the page number
    Page ++;
    if (log) log(ld, CF_LOGLEVEL_INFO,
		 "cfFilterPWGToPDF: Starting page %d.", Page);

    // Update rendering intent with user settings or the default
    cfGetPrintRenderIntent(data, header.cupsRenderingIntent,
			   sizeof(header.cupsRenderingIntent));

    // Use "profile=profile_name.icc" to embed 'profile_name.icc' into the PDF
    // for testing. Forces color management to enable.
    if (outformat == CF_FILTER_OUT_FORMAT_PDF &&
        (profile_name = cupsGetOption("profile", data->num_options,
				      data->options)) != NULL)
    {
      set_profile(profile_name, &doc);
      doc.cm_disabled = 0;
    }
    if (doc.colorProfile != NULL)
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
		   "cfFilterPWGToPDF: TEST ICC Profile specified (color "
		   "management forced ON): \n[%s]", profile_name);

    // Add a new page to PDF file
    if (add_pdf_page(&pdf, Page, header.cupsWidth, header.cupsHeight,
		     header.cupsBitsPerPixel, header.cupsBitsPerColor, 
		     header.cupsBytesPerLine, header.cupsRenderingIntent, 
		     header.cupsColorSpace, header.HWResolution[0],
		     header.HWResolution[1], &doc) != 0)
    {
      if (log) log(ld, CF_LOGLEVEL_ERROR,
		    "cfFilterPWGToPDF: Unable to start new PDF page");
      ret = 1;
      goto error;
    }

    // Write the bit map into the PDF file
    if (convert_raster(ras, header.cupsWidth, header.cupsHeight,
		       header.cupsBitsPerPixel, header.cupsBytesPerLine, 
		       &pdf, &doc) != 0)
    {
      if (log) log(ld, CF_LOGLEVEL_ERROR,
		   "cfFilterPWGToPDF: Failed to convert page bitmap");
      ret = 1;
      goto error;
    }

    if(finish_page(&pdf, &doc) != 0)
    {
      return 1;
    }
  }
  if (empty)
  {
    if (log) log(ld, CF_LOGLEVEL_DEBUG,
		 "cfFilterPWGToPDF: Input is empty, outputting empty file.");
    ret = 0;
    goto error;
  }

  close_pdf_file(&pdf, &doc); // output to outputfp
			      //

  int tmp_fd_read = open(pdf.temp_filename, O_RDONLY);

  char buffer[1024];
  ssize_t bytes_read;
  while ((bytes_read = read(tmp_fd_read, buffer, sizeof(buffer))) > 0) 
  {
    if (write(outputfd, buffer, bytes_read) != bytes_read) 
    {
      fprintf(stderr, "write to outputfd failed");
      break;
    }
  }
  close(tmp_fd_read);
  unlink(pdf.temp_filename);


  if (doc.colorProfile != NULL)
    cmsCloseProfile(doc.colorProfile);

  cupsRasterClose(ras);
  fclose(outputfp);

  return (Page == 0);

error:
  cupsRasterClose(ras);
  fclose(outputfp);

  return (ret);
}



