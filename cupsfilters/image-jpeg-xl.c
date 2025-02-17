#ifdef HAVE_LIBJXL
#include "image-jpeg-xl.h"
#include <jxl/decode.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * is_jpegxl() - Check if the header bytes indicate a JPEG‑XL file.
 *
 * This example compares the first 8 bytes with an expected signature.
 */
int
is_jpegxl(const unsigned char *header, size_t len)
{
  if (len < 12)
    return 0;
  if (!memcmp(header, "\x00\x00\x00\x0C\x4A\x58\x4C\x20", 8))
    return 1;
  return 0;
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
  cf_image_t *temp = img;  /* For brevity */
  
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

  // Process input until full image is decoded.
  while ((status = JxlDecoderProcessInput(decoder)) != JXL_DEC_FULL_IMAGE) {
    if (status == JXL_DEC_ERROR) {
      fprintf(stderr, "JPEG‑XL: Decoding error.\n");
      JxlDecoderDestroy(decoder);
      free(data);
      return -1;
    }
  }

  /* Convert the decoded output into a cf_image_t.
     Here we call a helper function (e.g., cfImageCreateFromJxlDecoder())
     that extracts image properties (width, height, bit depth, etc.) and sets
     the output pixel buffer.
  */
  temp = cfImageCreateFromJxlDecoder(decoder);
  if (!temp) {
    fprintf(stderr, "JPEG‑XL: Failed to create image from decoder data.\n");
    JxlDecoderDestroy(decoder);
    free(data);
    return -1;
  }
  
  /* Copy properties from the temporary image into the provided img structure */
  memcpy(img, temp, sizeof(cf_image_t));

  free(temp); /* If cfImageCreateFromJxlDecoder() allocated the cf_image_t separately */
  JxlDecoderDestroy(decoder);
  free(data);

  return 0;
}
#endif
