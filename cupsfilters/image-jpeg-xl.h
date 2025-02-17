#ifndef IMAGE_JPEG_XL_H
#define IMAGE_JPEG_XL_H

#ifdef HAVE_LIBJXL
#include <stdio.h>
#include "image-private.h"  

int _cfImageReadJPEGXL(cf_image_t *img, FILE *fp,
                       cf_icspace_t primary, cf_icspace_t secondary,
                       int saturation, int hue, const cf_ib_t *lut);
#endif
#endif 
