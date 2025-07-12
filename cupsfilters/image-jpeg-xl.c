//
// JPEG‑XL image routines for libcupsfilters.
//
// Copyright 2025 by Titiksha Bansal.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Contents:
//
//   _cfIsJPEGXL()                          - Check if the file header indicates JPEG‑XL format.
//   _cfImageReadJPEGXL()                   - Read a JPEG‑XL image file using libjxl and fill a 
//                                            cf_image_t structure.
//


//
// Include necessary headers
//


#include "config.h"

#ifdef HAVE_LIBJXL
#include "image-jpeg-xl.h"
#include <jxl/decode.h>
#include <jxl/types.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>  // For PRIu64 


//
// _cfIsJPEGXL() - Check if the header bytes indicate a JPEG‑XL file.
// Checks both the container signature ("0000000C4A584C20") and the stream signature ("FF0A").
//

int
_cfIsJPEGXL(const unsigned char *header, 
            size_t len)
{
  if (len < 12)
    return 0;
  
  JxlSignature sig = JxlSignatureCheck((const uint8_t *)header, len);
  
  return (sig == JXL_SIG_CODESTREAM || sig == JXL_SIG_CONTAINER);
  return 0;
}


//
// _cfImageReadJPEGXL() - Read a JPEG‑XL image using libjxl.
// Reads the entire file from the given FILE pointer, decodes it using libjxl,
// and fills the provided cf_image_t structure. Returns 0 on success, nonzero on failure.
//

int
_cfImageReadJPEGXL( cf_image_t      *img,
                FILE            *fp,
                cf_icspace_t    primary,
                cf_icspace_t    secondary,
                int             saturation,
                int             hue,
                const cf_ib_t   *lut)
{
  JxlDecoder *dec = NULL;
  JxlBasicInfo info;
  JxlDecoderStatus status;
  uint8_t *jxl_data = NULL;
  long jxl_size;
  int ok = 0;
  size_t bytes_read;

  //
  // Read entire file into memory
  //

  fseek(fp, 0, SEEK_END);
  jxl_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  jxl_data = (uint8_t*)malloc(jxl_size);
  if (!jxl_data)
  {
    fclose(fp);
    return 1;
  }
  bytes_read = fread(jxl_data, 1, jxl_size, fp);
  if (bytes_read != (size_t)jxl_size)
  {
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  dec = JxlDecoderCreate(NULL);
  if (!dec)
  {
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  status = JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE);
  if (status != JXL_DEC_SUCCESS)
  {
    JxlDecoderDestroy(dec);
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  JxlDecoderSetInput(dec, jxl_data, jxl_size);

  //
  // Process to get basic info
  //
  
  status = JxlDecoderProcessInput(dec);
  if (status != JXL_DEC_BASIC_INFO)
  {
    JxlDecoderDestroy(dec);
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  if (JxlDecoderGetBasicInfo(dec, &info) != JXL_DEC_SUCCESS)
  {
    JxlDecoderDestroy(dec);
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  img->xsize = info.xsize;
  img->ysize = info.ysize;

  //
  // Validate dimensions
  //
  
  if (img->xsize == 0 || img->xsize > CF_IMAGE_MAX_WIDTH ||
      img->ysize == 0 || img->ysize > CF_IMAGE_MAX_HEIGHT)
  {
    DEBUG_printf(("DEBUG: JXL image has invalid dimensions %ux%u!\n",
                  (unsigned)img->xsize, (unsigned)img->ysize));
    JxlDecoderDestroy(dec);
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  //
  // Read EXIF data (if available)
  //
  
#ifdef HAVE_EXIF
  fseek(fp, 0, SEEK_SET);
  int temp = _cfImageReadEXIF(img, fp);    // Handle resolution from EXIF if needed
#endif

  //
  // Determine colorspace based on number of color channels
  //
  
  if (info.num_color_channels == 3)
  {
    img->colorspace = (primary == CF_IMAGE_RGB_CMYK) ? CF_IMAGE_RGB : primary;
  } 
  else
  {
    img->colorspace = secondary;
  }

  //
  // Set up pixel format for decoding
  //
  
  JxlPixelFormat format;
  format.num_channels = (info.num_color_channels == 1) ? 1 : 3;
  if (info.alpha_bits > 0)
  {
    format.num_channels++;
  }
  format.data_type = JXL_TYPE_UINT8;
  format.endianness = JXL_NATIVE_ENDIAN;
  format.align = 0;

  size_t buffer_size;
  if (JxlDecoderImageOutBufferSize(dec, &format, &buffer_size) != JXL_DEC_SUCCESS)
  {
    JxlDecoderDestroy(dec);
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  uint8_t *pixels = (uint8_t*)malloc(buffer_size);
  if (!pixels)
  {
    JxlDecoderDestroy(dec);
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  if (JxlDecoderSetImageOutBuffer(dec, &format, pixels, buffer_size) != JXL_DEC_SUCCESS)
  {
    free(pixels);
    JxlDecoderDestroy(dec);
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  //
  // Process to get the full image
  //
  
  status = JxlDecoderProcessInput(dec);
  if (status != JXL_DEC_FULL_IMAGE)
  {
    free(pixels);
    JxlDecoderDestroy(dec);
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  //
  // Handle alpha blending with white background
  //
  
  if (info.alpha_bits > 0)
  {
    int channels = format.num_channels;
    size_t pixel_count = img->xsize * img->ysize;
    for (size_t i = 0; i < pixel_count; i++)
    {
      uint8_t *pixel = pixels + i * channels;
      uint8_t alpha = pixel[channels - 1];
      if (alpha != 255)
      {
        float ratio = alpha / 255.0f;
        for (int c = 0; c < channels - 1; c++)
        {
          pixel[c] = (uint8_t)(pixel[c] * ratio + 255 * (1.0f - ratio) + 0.5f);
        }
        pixel[channels - 1] = 255;
      }
    }
  }

  //
  // Process each row for colorspace conversion and LUT
  //
  
  int bpp = cfImageGetDepth(img);
  cf_ib_t *out = (cf_ib_t*)calloc(img->xsize * bpp, sizeof(cf_ib_t));
  if (!out)
  {
    free(pixels);
    JxlDecoderDestroy(dec);
    free(jxl_data);
    fclose(fp);
    return 1;
  }

  for (int y = 0; y < img->ysize; y++)
  {
    uint8_t *row = pixels + y * img->xsize * format.num_channels;

    if (info.num_color_channels == 3)
    {                  // RGB(A) Image
      if (saturation != 100 || hue != 0)
      {
        cfImageRGBAdjust(row, img->xsize, saturation, hue);
      }

      switch (img->colorspace)
      {
        case CF_IMAGE_WHITE:
          cfImageRGBToWhite(row, out, img->xsize);
          break;
        case CF_IMAGE_RGB:
        case CF_IMAGE_RGB_CMYK:
          cfImageRGBToRGB(row, out, img->xsize);
          break;
        case CF_IMAGE_BLACK:
          cfImageRGBToBlack(row, out, img->xsize);
          break;
        case CF_IMAGE_CMY:
          cfImageRGBToCMY(row, out, img->xsize);
          break;
        case CF_IMAGE_CMYK:
          cfImageRGBToCMYK(row, out, img->xsize);
          break;
      }
    } 
    else
    {                                              // Grayscale Image
      switch (img->colorspace)
      {
        case CF_IMAGE_WHITE:
          memcpy(out, row, img->xsize);
          break;
        case CF_IMAGE_RGB:
        case CF_IMAGE_RGB_CMYK:
          cfImageWhiteToRGB(row, out, img->xsize);
          break;
        case CF_IMAGE_BLACK:
          cfImageWhiteToBlack(row, out, img->xsize);
          break;
        case CF_IMAGE_CMY:
          cfImageWhiteToCMY(row, out, img->xsize);
          break;
        case CF_IMAGE_CMYK:
          cfImageWhiteToCMYK(row, out, img->xsize);
          break;
      }
    }

    if (lut)
    {
      cfImageLut(out, img->xsize * bpp, lut);
    }

    _cfImagePutRow(img, 0, y, img->xsize, out);
  }

  //
  // Cleanup
  //
  
  free(out);
  free(pixels);
  JxlDecoderDestroy(dec);
  free(jxl_data);
  fclose(fp);

  return 0;
}
#endif // HAVE_LIBJXL
