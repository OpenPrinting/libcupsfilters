#include "config.h"

#ifdef HAVE_LIBJXL
#include "image-jpeg-xl.h"
#include <jxl/decode.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * is_jpegxl() - Check if the header bytes indicate a JPEG‑XL file.
 *
 * This function returns 1 if the header matches either the
 * “box” signature (0x00 0x00 0x00 0x0C "JXL ") or the
 * “stream” signature (0xFF 0x0A).
 */
int
is_jpegxl(const unsigned char *header, size_t len)
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
 * This function retrieves the basic information from the decoder (using
 * JxlDecoderGetBasicInfo) to determine image dimensions and bit depth.
 * It then creates a JxlPixelFormat structure (for 16‑bit per channel, 3‑channel RGB),
 * queries the size needed for the output buffer, allocates that buffer, and
 * sets it in the decoder. It then processes the decoder events until the
 * full image is decoded. Finally, it allocates and populates a cf_image_t
 * structure with the decoded image properties and pixel data.
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

  /* Retrieve basic image information */
  JxlBasicInfo basic_info;
  JxlPixelFormat pixel_format = {
    .num_channels = 3,               /* Assume RGB output */
    .data_type = JXL_TYPE_UINT16,      /* 16-bit per channel */
    .endianness = JXL_BIG_ENDIAN,      /* Big-endian output */
    .align = 0
  };
  status = JxlDecoderGetBasicInfo(decoder, &basic_info);
  if (status != JXL_DEC_SUCCESS) {
    fprintf(stderr, "cf_image_create_from_jxl_decoder: Unable to retrieve basic info.\n");
    return NULL;
  }

  /* Determine the size needed for the output buffer */
  status = JxlDecoderImageOutBufferSize(decoder, &pixel_format, &buffer_size);
  if (status != JXL_DEC_SUCCESS || buffer_size == 0) {
    fprintf(stderr, "cf_image_create_from_jxl_decoder: Unable to determine output buffer size.\n");
    return NULL;
  }

  /* Allocate memory for the decoded pixel data */
  output_buffer = malloc(buffer_size);
  if (!output_buffer) {
    fprintf(stderr, "cf_image_create_from_jxl_decoder: Memory allocation for output buffer failed.\n");
    return NULL;
  }

  /* Set the output buffer in the decoder */
  status = JxlDecoderSetImageOutBuffer(decoder, &pixel_format, output_buffer, buffer_size);
  if (status != JXL_DEC_SUCCESS) {
    fprintf(stderr, "cf_image_create_from_jxl_decoder: Failed to set output buffer.\n");
    free(output_buffer);
    return NULL;
  }

  /* Process decoder events until the full image is decoded */
  while ((status = JxlDecoderProcessInput(decoder)) != JXL_DEC_FULL_IMAGE) {
    if (status == JXL_DEC_ERROR) {
      fprintf(stderr, "cf_image_create_from_jxl_decoder: JPEG‑XL decoding error.\n");
      free(output_buffer);
      return NULL;
    }
  }

  /* Allocate and populate the cf_image_t structure */
  img = malloc(sizeof(cf_image_t));
  if (!img) {
    fprintf(stderr, "cf_image_create_from_jxl_decoder: Memory allocation for image structure failed.\n");
    free(output_buffer);
    return NULL;
  }

  img->xsize  = (int)basic_info.xsize;
  img->ysize  = (int)basic_info.ysize;
  /* libjxl does not provide density; use default values */
  img->xppi   = 72;
  img->yppi   = 72;

#ifdef HAVE_BITS_PER_COMPONENT
  img->bitsPerComponent = (int)basic_info.bits_per_sample;
#endif

  /* For JPEG‑XL, we assume the decoded format is RGB */
  img->colorspace = CF_IMAGE_RGB;

#ifdef HAVE_IMAGE_DATA
  img->data = output_buffer;
#else
  free(output_buffer);
  free(img);
  fprintf(stderr, "Error: cf_image_t does not have a 'data' field.\n");
  return NULL;
#endif

  return img;
}

/*
 * _cfImageReadJPEGXL() - Read a JPEG‑XL image using libjxl.
 *
 * This function reads the entire file from the current file pointer,
 * decodes it using libjxl, and fills the provided cf_image_t structure.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
_cfImageReadJPEGXL(cf_image_t *img, FILE *fp,
                   cf_icspace_t primary, cf_icspace_t secondary,
                   int saturation, int hue, const cf_ib_t *lut)
{
  unsigned char *data = NULL;
  size_t filesize = 0, bytesRead = 0;
  JxlDecoder *decoder = NULL;
  JxlDecoderStatus status;
  
  /* Determine file size */
  if (fseek(fp, 0, SEEK_END) != 0) {
    fprintf(stderr, "JPEG‑XL: fseek failed.\n");
    return -1;
  }
  filesize = ftell(fp);
  rewind(fp);

  data = malloc(filesize);
  if (!data) {
    fprintf(stderr, "JPEG‑XL: Memory allocation failed.\n");
    return -1;
  }
  bytesRead = fread(data, 1, filesize, fp);
  if (bytesRead != filesize) {
    fprintf(stderr, "JPEG‑XL: File read error.\n");
    free(data);
    return -1;
  }

  /* Create the libjxl decoder */
  decoder = JxlDecoderCreate(NULL);
  if (!decoder) {
    fprintf(stderr, "JPEG‑XL: Failed to create decoder.\n");
    free(data);
    return -1;
  }
  
  status = JxlDecoderSetInput(decoder, data, filesize);
  if (status != JXL_DEC_SUCCESS) {
    fprintf(stderr, "JPEG‑XL: Failed to set input buffer.\n");
    JxlDecoderDestroy(decoder);
    free(data);
    return -1;
  }

  /* Process input until full image is decoded */
  while ((status = JxlDecoderProcessInput(decoder)) != JXL_DEC_FULL_IMAGE) {
    if (status == JXL_DEC_ERROR) {
      fprintf(stderr, "JPEG‑XL: Decoding error.\n");
      JxlDecoderDestroy(decoder);
      free(data);
      return -1;
    }
  }

  cf_image_t *new_img = cf_image_create_from_jxl_decoder(decoder);
  if (!new_img) {
    fprintf(stderr, "JPEG‑XL: Failed to create image from decoder data.\n");
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
 * _cfImageOpenJPEGXL() - Open a JPEG‑XL image from a FILE pointer.
 *
 * This function is provided for internal use and is called by cfImageOpenFP()
 * in image.c when a JPEG‑XL file is detected.
 *
 * Returns a pointer to a cf_image_t on success, or NULL on failure.
 */
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
