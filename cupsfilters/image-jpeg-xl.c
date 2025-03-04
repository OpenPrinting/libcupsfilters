#include "config.h"


#ifdef HAVE_LIBJXL
#include "image-jpeg-xl.h"
#include <jxl/decode.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>  /* For PRIu64 */
#include "log.h"       /* Use the internal logging functions */


/*
 * _cfIsJPEGXL() - Check if the header bytes indicate a JPEG‑XL file.
 *
 * This function checks for both the JPEG‑XL container signature and the
 * JPEG‑XL stream signature.
 */
int
_cfIsJPEGXL(const unsigned char *header, 
            size_t len)
{
  if (len < 12)
    return 0;
  if (!memcmp(header, "\x00\x00\x00\x0C\x4A\x58\x4C\x20", 8))
    return 1;
  if (!memcmp(header, "\xFF\x0A", 2))
    return 1;
  return 0;
}


/*
 * cfImageCreateFromJxlDecoder() - Create a cf_image_t from a libjxl decoder.
 *
 * This function retrieves basic image information, allocates the output buffer,
 * sets it in the decoder, processes the input until the full image is decoded,
 * and then allocates and populates a new cf_image_t structure.
 *
 * Returns: Pointer to a cf_image_t on success, or NULL on failure.
 */
static cf_image_t *
cf_image_create_from_jxl_decoder(JxlDecoder *decoder)
{
  cf_image_t *img = NULL;
  size_t buffer_size = 0;
  void *output_buffer = NULL;
  JxlDecoderStatus status;

  
  /* Retrieve basic image information directly. */
  JxlBasicInfo basic_info;
  JxlPixelFormat pixel_format = {
    .num_channels = 3,
    .data_type = JXL_TYPE_UINT16,
    .endianness = JXL_BIG_ENDIAN,
    .align = 0
  };
  
  status = JxlDecoderGetBasicInfo(decoder, &basic_info);
  
  if (status != JXL_DEC_SUCCESS) {
    DEBUG_printf(("cf_image_create_from_jxl_decoder: Unable to retrieve basic info. Status: %d\n", status));
    return NULL;
  }

  DEBUG_printf(("DEBUG: Basic info: xsize=%" PRIu64 ", ysize=%" PRIu64
                basic_info.xsize, basic_info.ysize));

  
  /* Determine the size needed for the output buffer.
     We request output as 16-bit unsigned integers to preserve high color depth. */
  status = JxlDecoderImageOutBufferSize(decoder, &pixel_format, &buffer_size);
  if (status != JXL_DEC_SUCCESS || buffer_size == 0) {
    DEBUG_printf(("cf_image_create_from_jxl_decoder: Unable to determine output buffer size. Status: %d, buffer_size: %zu\n",
                  status, buffer_size));
    return NULL;
  }
  
  DEBUG_printf(("DEBUG: Output buffer size determined: %zu bytes\n", buffer_size));

  
  /* Allocate memory for the decoded pixel data */
  output_buffer = malloc(buffer_size);
  if (!output_buffer) {
    DEBUG_printf(("cf_image_create_from_jxl_decoder: Memory allocation for output buffer failed.\n"));
    return NULL;
  }

  
  /* Set the output buffer in the decoder */
  status = JxlDecoderSetImageOutBuffer(decoder, &pixel_format, output_buffer, buffer_size);
  
  if (status != JXL_DEC_SUCCESS) {
    DEBUG_printf(("cf_image_create_from_jxl_decoder: Failed to set output buffer. Status: %d\n", status));
    free(output_buffer);
    return NULL;
  }

  
  /* Process decoder events until the full image is decoded. */
  while ((status = JxlDecoderProcessInput(decoder)) != JXL_DEC_FULL_IMAGE) {
    if (status == JXL_DEC_ERROR) {
      DEBUG_printf(("cf_image_create_from_jxl_decoder: JPEG‑XL decoding error. Status: %d\n", status));
      free(output_buffer);
      return NULL;
    }
    DEBUG_printf(("cf_image_create_from_jxl_decoder: Processing input. Current status: %d\n", status));
  }

  
  /* Allocate and populate the cf_image_t structure */
  img = malloc(sizeof(cf_image_t));
  if (!img) {
    DEBUG_printf(("cf_image_create_from_jxl_decoder: Memory allocation for image structure failed.\n"));
    free(output_buffer);
    return NULL;
  }

  img->xsize  = (int)basic_info.xsize;
  img->ysize  = (int)basic_info.ysize;
  img->xppi   = (basic_info.uses_original_profile) ? 300 : 72; /* Adjust if necessary */
  img->yppi   = (basic_info.uses_original_profile) ? 300 : 72; /* Adjust if necessary */

  /* For JPEG‑XL, we assume the decoded format is RGB. */
  img->colorspace = CF_IMAGE_RGB;

#ifdef HAVE_IMAGE_DATA
  img->data = output_buffer;
#else
  free(output_buffer);
  free(img);
  DEBUG_printf(("cfImageFromJxlDecoder: Error: cf_image_t does not have a 'data' field.\n"));
  return NULL;
#endif

  DEBUG_printf(("DEBUG: d image: %dx%d, xppi=%d, yppi=%d, bitsPerComponent=%d\n",
                img->xsize, img->ysize, img->xppi, img->yppi, img->bitsPerComponent));
  return img;
}

/*
 * __cf_image_read_jpegxl() - Read a JPEG‑XL image using libjxl.
 *
 * This function reads the entire file from the current file pointer,
 * decodes it using libjxl, and fills the provided cf_image_t structure.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
_cf_image_read_jpegxl(cf_image_t *img, FILE *fp,
                   cf_icspace_t primary, cf_icspace_t secondary,
                   int saturation, int hue, const cf_ib_t *lut)
{
  unsigned char *data = NULL;
  size_t filesize = 0, bytesRead = 0;
  JxlDecoder *decoder = NULL;
  JxlDecoderStatus status;
  
  /* Determine file size */
  if (fseek(fp, 0, SEEK_END) != 0) {
    DEBUG_printf(("DEBUG: _cf_image_read_jpegxl: fseek failed.\n"));
    return -1;
  }
  filesize = ftell(fp);
  DEBUG_printf(("DEBUG: _cf_image_read_jpegxl: File size: %zu bytes\n", filesize));
  rewind(fp);

  data = malloc(filesize);
  if (!data) {
    DEBUG_printf(("DEBUG: _cf_image_read_jpegxl: Memory allocation failed.\n"));
    return -1;
  }
  bytesRead = fread(data, 1, filesize, fp);
  DEBUG_printf(("DEBUG: Bytes read: %zu bytes\n", bytesRead));
  if (bytesRead != filesize) {
    DEBUG_printf(("DEBUG: _cf_image_read_jpegxl: File read error. Expected %zu bytes, got %zu bytes\n", filesize, bytesRead));
    free(data);
    return -1;
  }

  /*  the libjxl decoder */
  decoder = JxlDecoderCreate(NULL);
  if (!decoder) {
    DEBUG_printf(("DEBUG: _cf_image_read_jpegxl: Failed to  decoder.\n"));
    free(data);
    return -1;
  }
  
  status = JxlDecoderSetInput(decoder, data, filesize);
  if (status != JXL_DEC_SUCCESS) {
    DEBUG_printf(("DEBUG: _cf_image_read_jpegxl: Failed to set input buffer. Status: %d\n", status));
    JxlDecoderDestroy(decoder);
    free(data);
    return -1;
  }

  /* Process input until full image is decoded. */
  while ((status = JxlDecoderProcessInput(decoder)) != JXL_DEC_FULL_IMAGE) {
    if (status == JXL_DEC_ERROR) {
      DEBUG_printf(("DEBUG: _cf_image_read_jpegxl: Decoding error. Status: %d\n", status));
      JxlDecoderDestroy(decoder);
      free(data);
      return -1;
    }
    DEBUG_printf(("DEBUG: _cf_image_read_jpegxl: Processing input. Current status: %d\n", status));
  }

  cf_image_t *new_img = _cf_image_create_from_jxl_decoder(decoder);
  if (!new_img) {
    DEBUG_printf(("DEBUG: _cf_image_read_jpegxl: Failed to create image from decoder data.\n"));
    JxlDecoderDestroy(decoder);
    free(data);
    return -1;
  }
  
  /* Copy the new image into the provided structure */
  memcpy(img, new_img, sizeof(cf_image_t));
  free(new_img);

  JxlDecoderDestroy(decoder);
  free(data);
  return 0;
}

/*
 * _cf_image_open_jpegxl() - Open a JPEG‑XL image from a FILE pointer.
 *
 * This function is provided for internal use and is called by cfImageOpenFP()
 * in image.c when a JPEG‑XL file is detected.
 *
 * Returns a pointer to a cf_image_t on success, or NULL on failure.
 */
cf_image_t *
_cf_image_open_jpegxl(FILE *fp, cf_icspace_t primary, cf_icspace_t secondary,
                   int saturation, int hue, const cf_ib_t *lut)
{
  cf_image_t *img = calloc(1, sizeof(cf_image_t));
  if (img == NULL)
    return NULL;

  if (_cf_image_read_jpegxl(img, fp, primary, secondary, saturation, hue, lut)) {
    cfImageClose(img);
    return NULL;
  }
  return img;
}
#endif
