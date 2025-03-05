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
//   _cfIsJPEGXL()            - Check if the file header indicates JPEG‑XL format.
//   _cfImageReadJPEGXL()     - Read a JPEG‑XL image file using libjxl and fill a cf_image_t structure.
//   _cfImageOpenJPEGXL()     - Open a JPEG‑XL image file and read it into memory.
//


#ifndef IMAGE_JPEG_XL_H
#define IMAGE_JPEG_XL_H

#ifdef HAVE_LIBJXL
#include <stdio.h>
#include "image-private.h"  

int _cfIsJPEGXL(const unsigned char *header, size_t len);

int _cfImageReadJPEGXL(cf_image_t *img, FILE *fp,
                       cf_icspace_t primary, cf_icspace_t secondary,
                       int saturation, int hue, const cf_ib_t *lut);

cf_image_t *_cfImageOpenJPEGXL(FILE *fp, cf_icspace_t primary,
                               cf_icspace_t secondary, int saturation,
                               int hue, const cf_ib_t *lut);

#endif // HAVE_LIBJXL
#endif // IMAGE_JPEG_XL_H
