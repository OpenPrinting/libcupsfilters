//
// JPEG‑XL image routines for libcupsfilters.
//
// Copyright 2007-2011 by Apple Inc.
// Copyright 1993-2005 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Contents:
//
//   _cfIsJPEGXL()                          - Check if the file header indicates JPEG‑XL format.
//   _cf_image_create_from_jxl_decoder()    - Create a cf_image_t from a libjxl decoder.
//   _cfImageReadJPEGXL()                   - Read a JPEG‑XL image file using libjxl and fill a 
//                                            cf_image_t structure.
//   _cfOpenJPEGXL()                        - Open a JPEG‑XL image file and read it into memory.
//


//
// Include necessary headers
//


#include "config.h"

#ifdef HAVE_LIBJXL
#include "image-jpeg-xl.h"
#include <jxl/decode.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>  // For PRIu64 


//
// _cfIsJPEGXL() - Check if the header bytes indicate a JPEG‑XL file.
// Checks both the container signature ("0000000C4A584C20") and the stream signature ("FF0A").
//

int
_cfIsJPEGXL(const unsigned char *header, size_t len)
{
  if (len < 12)
    return 0;
  if (!memcmp(header, "\x00\x00\x00\x0C\x4A\x58\x4C\x20", 8))
    return 1;
  if (!memcmp(header, "\xFF\x0A", 2))
    return 1;
  return 0;
}


//
// _cf_image_create_from_jxl_decoder() - Create a cf_image_t from a libjxl decoder.
// Retrieves basic image information, allocates an 8‑bit output buffer, processes the image,
// and copies the decoded rows into the image’s tile cache.
//

static cf_image_t *
_cf_image_create_from_jxl_decoder(JxlDecoder *decoder)
{
  cf_image_t *img = NULL;
  size_t buffer_size = 0;
  void *output_buffer = NULL;
  JxlDecoderStatus status;

  //
  // Retrieve basic image information.
  //

  JxlBasicInfo basic_info;
  // Request 8-bit output (3 channels) for compatibility with the rest of the library.
  JxlPixelFormat pixel_format = { .num_channels = 3,
                                  .data_type = JXL_TYPE_UINT16,
                                  .endianness = JXL_BIG_ENDIAN,
                                  .align = 0 };
  status = JxlDecoderGetBasicInfo(decoder, &basic_info);
  if (status != JXL_DEC_SUCCESS) {
    DEBUG_printf("DEBUG: _cf_image_create_from_jxl_decoder: Unable to retrieve basic info.\n");
    return NULL;
  }
  
  DEBUG_printf(("DEBUG: Basic info: xsize=%" PRIu32 ", ysize=%" PRIu32 
                  ", bits_per_sample=%u, uses_original_profile=%d\n",
                  basic_info.xsize, basic_info.ysize,
                  basic_info.bits_per_sample, basic_info.uses_original_profile));

  //
  // Allocate the cf_image_t structure and set dimensions.
  //

  img = malloc(sizeof(cf_image_t));
  if (!img) {
      DEBUG_printf(("DEBUG: _cf_image_create_from_jxl_decoder: Memory allocation for image structure failed.\n"));
      return NULL;
  }
  img->xsize = (int) basic_info.xsize;
  img->ysize = (int) basic_info.ysize;
  img->xppi  = (basic_info.uses_original_profile) ? 300 : 72;
  img->yppi  = (basic_info.uses_original_profile) ? 300 : 72;
  img->colorspace = CF_IMAGE_RGB;
  
  //
  // Determine the output buffer size.
  //

  status = JxlDecoderImageOutBufferSize(decoder, &pixel_format, &buffer_size);
  if (status != JXL_DEC_SUCCESS || buffer_size == 0) {
    DEBUG_printf(("DEBUG: Unable to determine output buffer size (status=%d, buffer_size=%zu)\n", 
                      status, buffer_size));
    return NULL;
  }
  DEBUG_printf(("DEBUG: Output buffer size: %zu bytes\n", buffer_size));

  //
  // Allocate memory for the decoded pixel data
  //
  
  output_buffer = malloc(buffer_size);
  if (!output_buffer) {
    DEBUG_printf("DEBUG: _cf_image_create_from_jxl_decoder: Memory allocation for output buffer failed.\n");
    return NULL;
  }

  //
  // Set the output buffer in the decoder.
  //

  status = JxlDecoderSetImageOutBuffer(decoder, &pixel_format, output_buffer, buffer_size);
  if (status != JXL_DEC_SUCCESS) {
    DEBUG_printf("DEBUG: _cf_image_create_from_jxl_decoder: Failed to set output buffer.\n");
    free(output_buffer);
    free(img);
    return NULL;
  }

  //
  // Process decoder events until the full image is decoded.
  //

  while ((status = JxlDecoderProcessInput(decoder)) != JXL_DEC_FULL_IMAGE) {
    if (status == JXL_DEC_ERROR) {
      DEBUG_printf("DEBUG: _cf_image_create_from_jxl_decoder: JPEG‑XL decoding error.\n");
      free(output_buffer);
      free(img);
      return NULL;
    }
  }

  DEBUG_printf(("DEBUG: _cf_image_create_from_jxl_decoder: Decoding complete.\n"));

  //
  // Initialize tile cache for the image.
  //
  
  cfImageSetMaxTiles(img, 0);

  //
  // Copy decoded data row by row into the image’s tile cache.
  //
  
  int row;
  int bytes_per_pixel = pixel_format.num_channels * 1; /* 1 byte per channel */
  int row_stride = img->xsize * bytes_per_pixel;
  for (row = 0; row < img->ysize; row++) {
    unsigned char *row_data = ((unsigned char *) output_buffer) + row * row_stride;
    _cfImagePutRow(img, 0, row, img->xsize, row_data);
  }

  free(output_buffer);
  return img;
}


//
// _cfImageReadJPEGXL() - Read a JPEG‑XL image using libjxl.
// Reads the entire file from the given FILE pointer, decodes it using libjxl,
// and fills the provided cf_image_t structure. Returns 0 on success, nonzero on failure.
//

int
_cfImageReadJPEGXL(cf_image_t *img, FILE *fp,
                      cf_icspace_t primary, cf_icspace_t secondary,
                      int saturation, int hue, const cf_ib_t *lut)
{
  unsigned char *data = NULL;
  size_t filesize = 0, bytesRead = 0;
  JxlDecoder *decoder = NULL;
  JxlDecoderStatus status;
  
  //
  // Determine file size.
  //

  if (fseek(fp, 0, SEEK_END) != 0) {
    DEBUG_printf("DEBUG: JPEG‑XL: fseek failed.\n");
    return -1;
  }
  filesize = ftell(fp);
  rewind(fp);

  data = malloc(filesize);
  if (!data) {
    DEBUG_printf("DEBUG: JPEG‑XL: Memory allocation failed.\n");
    return -1;
  }
  bytesRead = fread(data, 1, filesize, fp);
  DEBUG_printf(("DEBUG: JPEG‑XL bytes read: %zu\n", bytesRead));
  if (bytesRead != filesize) {
      DEBUG_puts("DEBUG: JPEG‑XL: File read error.");
      free(data);
      return -1;
  }

  //
  // Create the libjxl decoder.
  //
  
  decoder = JxlDecoderCreate(NULL);
  if (!decoder) {
    DEBUG_printf("DEBUG: JPEG‑XL: Failed to create decoder.\n");
    free(data);
    return -1;
  }
  
  status = JxlDecoderSetInput(decoder, data, filesize);
  if (status != JXL_DEC_SUCCESS) {
    DEBUG_printf("DEBUG: JPEG‑XL: Failed to set input buffer.\n");
    JxlDecoderDestroy(decoder);
    free(data);
    return -1;
  }

  //
  // Process input until full image is decoded.
  //
  
  while ((status = JxlDecoderProcessInput(decoder)) != JXL_DEC_FULL_IMAGE) {
    if (status == JXL_DEC_ERROR) {
      DEBUG_printf("DEBUG: JPEG‑XL: Decoding error.\n");
      JxlDecoderDestroy(decoder);
      free(data);
      return -1;
    }
  }

  //
  // Create a new image from the decoded data and copy it into the provided structure.
  //
  
  cf_image_t *new_img = _cf_image_create_from_jxl_decoder(decoder);
  if (!new_img) {
    DEBUG_printf(("DEBUG: _cfImageReadJPEGXL: Failed to create image from decoder data.\n"));
    JxlDecoderDestroy(decoder);
    free(data);
    return -1;
  }
  memcpy(img, new_img, sizeof(cf_image_t));
  free(new_img);

  JxlDecoderDestroy(decoder);
  free(data);
  return 0;
}


//
// _cfImageOpenJPEGXL() - Open a JPEG‑XL image from a FILE pointer.
// Called by cfImageOpenFP() when a JPEG‑XL file is detected.
// Returns a pointer to a cf_image_t on success, or NULL on failure.
//

cf_image_t *
_cfImageOpenJPEGXL(FILE *fp, cf_icspace_t primary, cf_icspace_t secondary,
                      int saturation, int hue, const cf_ib_t *lut)
{
  cf_image_t *img = calloc(1, sizeof(cf_image_t));
  if (img == NULL)
    return NULL;

  if (_cfImageReadJPEGXL(img, fp, primary, secondary, saturation, hue, lut)) {
    cfImageClose(img);
    return NULL;
  }
  return img;
}
#endif
