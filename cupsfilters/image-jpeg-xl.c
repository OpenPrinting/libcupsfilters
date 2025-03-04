#include "config.h"

#ifdef HAVE_LIBJXL
#include "image-jpeg-xl.h"
#include <jxl/decode.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>  /* For PRIu64 */

/*
 * _cfIsJPEGXL() - Check if the header bytes indicate a JPEG‑XL file.
 *
 * This function checks for both the JPEG‑XL container signature and the
 * JPEG‑XL stream signature.
 */
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

/*
 * cfImageCreateFromJxlDecoder() - Create a cf_image_t from a libjxl decoder.
 *
 * This function retrieves the basic information from the decoder to determine
 * image dimensions, bit depth, and resolution. It then queries the output buffer
 * size required for 16-bit per channel output, allocates that buffer, and sets it
 * in the decoder. The decoder is then run until the full image is decoded.
 * Finally, a new cf_image_t structure is allocated and populated with the
 * decoded image data and properties.
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
    fprintf(stderr, "cf_image_create_from_jxl_decoder: Unable to retrieve basic info (status=%d).\n", status);
    return NULL;
  }
  fprintf(stderr, "DEBUG: Basic info: xsize=%" PRIu64 ", ysize=%" PRIu64 ", bits_per_sample=%u, uses_original_profile=%d\n",
          basic_info.xsize, basic_info.ysize, basic_info.bits_per_sample, basic_info.uses_original_profile);

  /* Determine the size needed for the output buffer.
     We request output as 16-bit unsigned integers to preserve high color depth. */
  status = JxlDecoderImageOutBufferSize(decoder, 
                                        &pixel_format, 
                                        &buffer_size);
  if (status != JXL_DEC_SUCCESS || buffer_size == 0) {
    fprintf(stderr, "cf_image_create_from_jxl_decoder: Unable to determine output buffer size (status=%d, size=%zu).\n", status, buffer_size);
    return NULL;
  }
  fprintf(stderr, "DEBUG: Output buffer size: %zu bytes\n", buffer_size);

  /* Allocate memory for the decoded pixel data */
  output_buffer = malloc(buffer_size);
  if (!output_buffer) {
    fprintf(stderr, "cf_image_create_from_jxl_decoder: Memory allocation for output buffer failed.\n");
    return NULL;
  }

  /* Set the output buffer in the decoder */
  status = JxlDecoderSetImageOutBuffer(decoder, &pixel_format,
                                       output_buffer, buffer_size);
  if (status != JXL_DEC_SUCCESS) {
    fprintf(stderr, "cf_image_create_from_jxl_decoder: Failed to set output buffer (status=%d).\n", status);
    free(output_buffer);
    return NULL;
  }

  /* Process decoder events until the full image is decoded. */
  while ((status = JxlDecoderProcessInput(decoder)) != JXL_DEC_FULL_IMAGE) {
    if (status == JXL_DEC_ERROR) {
      fprintf(stderr, "cf_image_create_from_jxl_decoder: JPEG‑XL decoding error (status=%d).\n", status);
      free(output_buffer);
      return NULL;
    }
    /* Optionally, log intermediate statuses here */
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
  img->xppi   = (basic_info.uses_original_profile) ? 300 : 72; /* Adjust as needed */
  img->yppi   = (basic_info.uses_original_profile) ? 300 : 72; /* Adjust as needed */

  /* If your cf_image_t structure has a bitsPerComponent field, set it.
     Otherwise, you may need to remove or adapt this part based on your structure. */
#ifdef HAVE_BITS_PER_COMPONENT
  img->bitsPerComponent = (int)basic_info.bits_per_sample;
#else
  /* For now, set a default value if the field exists.
     If cf_image_t doesn't support this field, then remove this line. */
  /* img->bitsPerComponent = (int)basic_info.bits_per_sample; */
#endif

  /* For JPEG‑XL, we assume the decoded format is RGB. */
  img->colorspace = CF_IMAGE_RGB;

#ifdef HAVE_IMAGE_DATA
  img->data = output_buffer;
  fprintf(stderr, "DEBUG: Image created: %dx%d, xppi=%d, yppi=%d, bitsPerComponent=%d\n",
          img->xsize, img->ysize, img->xppi, img->yppi,
#ifdef HAVE_BITS_PER_COMPONENT
          img->bitsPerComponent
#else
          -1
#endif
         );
#else
  fprintf(stderr, "Error: cf_image_t does not have a 'data' field.\n");
  free(output_buffer);
  free(img);
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
  fprintf(stderr, "DEBUG: File size: %zu bytes\n", filesize);

  data = malloc(filesize);
  if (!data) {
    fprintf(stderr, "JPEG‑XL: Memory allocation failed.\n");
    return -1;
  }
  bytesRead = fread(data, 1, filesize, fp);
  if (bytesRead != filesize) {
    fprintf(stderr, "JPEG‑XL: File read error (read %zu of %zu bytes).\n", bytesRead, filesize);
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
    fprintf(stderr, "JPEG‑XL: Failed to set input buffer (status=%d).\n", status);
    JxlDecoderDestroy(decoder);
    free(data);
    return -1;
  }
  fprintf(stderr, "DEBUG: Input buffer set successfully.\n");

  // Process input until full image is decoded.
  while ((status = JxlDecoderProcessInput(decoder)) != JXL_DEC_FULL_IMAGE) {
    if (status == JXL_DEC_ERROR) {
      fprintf(stderr, "JPEG‑XL: Decoding error (status=%d).\n", status);
      JxlDecoderDestroy(decoder);
      free(data);
      return -1;
    }
    /* Optionally log intermediate statuses here */
  }
  fprintf(stderr, "DEBUG: Decoder reached JXL_DEC_FULL_IMAGE.\n");

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
