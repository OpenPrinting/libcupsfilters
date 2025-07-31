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
//   _cfIsJPEGXL()            - Check if the file header indicates JPEG‑XL format.
//   _cfImageReadJPEGXL()     - Read a JPEG‑XL image file using libjxl and fill a cf_image_t structure.
//


#ifdef HAVE_LIBJXL
#include <stdio.h>
#include "image-private.h"
#include <jxl/decode.h>

int _cfIsJPEGXL(const unsigned char *header, size_t len);

int _cfImageReadJPEGXL(cf_image_t *img, FILE *fp,
                       cf_icspace_t primary, cf_icspace_t secondary,
                       int saturation, int hue, const cf_ib_t *lut);

#endif // HAVE_LIBJXL
