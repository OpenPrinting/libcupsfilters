//
// Regression test for the pdftoraster copy_height off-by-one / out-of-bounds
// fix.  It pulls in the real cupsfilters/pdftoraster.c so it exercises the
// actual copy_image_rows() helper (the code write_page_image() calls in
// production), then drives it with a colordata buffer sized to EXACTLY
// copy_height rows while the page (cupsHeight) is one row taller -- i.e. the
// "rendered image shorter than page" case where the off-by-one lived.  Built
// with AddressSanitizer, the buggy `h <= copy_height` reads one row past the
// buffer and ASan aborts; the fixed `h < copy_height` stays in bounds.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// Include the unit under test so the static helper and its types are visible.
#include "cupsfilters/pdftoraster.c"

// A convert-line callback that READS the source row (this is the access that
// goes out of bounds when the copy loop over-runs) and returns a valid dst.
static unsigned char *
test_convert_line(unsigned char *src, unsigned char *dst,
                  unsigned int row, unsigned int plane,
                  unsigned int pixels, unsigned int size,
                  pdftoraster_doc_t *doc, convert_cspace_func convertCSpace)
{
  (void)row; (void)plane; (void)doc; (void)convertCSpace;
  unsigned int n = pixels < size ? pixels : size;
  for (unsigned int i = 0; i < n; i++)
    dst[i] = src[i];            // OOB read of src when copy_height is over-run
  return dst;
}

int
main(void)
{
  const unsigned int copy_height = 4;
  const unsigned int rowsize     = 8;
  const unsigned int copy_width  = 8;

  // colordata holds EXACTLY copy_height rows; reading row copy_height is OOB.
  unsigned char *colordata = (unsigned char *)malloc((size_t)copy_height * rowsize);
  memset(colordata, 0x55, (size_t)copy_height * rowsize);
  unsigned char *lineBuf = (unsigned char *)malloc(rowsize);

  pdftoraster_doc_t doc;
  memset(&doc, 0, sizeof(doc));
  doc.header.cupsWidth        = copy_width;
  doc.header.cupsHeight       = copy_height + 1;   // page one row taller
  doc.header.cupsBitsPerColor = 8;
  doc.header.cupsBitsPerPixel  = 8;
  doc.header.cupsBytesPerLine = rowsize;
  doc.header.cupsColorOrder   = CUPS_ORDER_CHUNKED;
  doc.header.cupsColorSpace   = CUPS_CSPACE_K;
  doc.header.cupsNumColors    = 1;
  doc.header.HWResolution[0]  = doc.header.HWResolution[1] = 72;
  doc.header.PageSize[0]      = copy_width;
  doc.header.PageSize[1]      = copy_height + 1;
  doc.bytesPerLine = rowsize;
  doc.nplanes      = 1;
  doc.nbands       = 1;
  doc.allocLineBuf = true;
  doc.swap_image_y = false;

  pdf_conversion_function_t convert;
  memset(&convert, 0, sizeof(convert));
  convert.convertLineEven = test_convert_line;
  convert.convertLineOdd  = test_convert_line;
  convert.convertCSpace   = NULL;

  int fd = open("/dev/null", O_WRONLY);
  cups_raster_t *raster = cupsRasterOpen(fd, CUPS_RASTER_WRITE);
  if (!raster)
  {
    fprintf(stderr, "cupsRasterOpen failed\n");
    free(lineBuf);
    free(colordata);
    close(fd);
    return 2;
  }
  cupsRasterWriteHeader(raster, &doc.header);

  copy_image_rows(raster, &doc, &convert, 0, colordata, rowsize,
                  copy_height, copy_width, lineBuf, 255);

  cupsRasterClose(raster);
  close(fd);
  free(lineBuf);
  free(colordata);
  fprintf(stderr, "OK: copy_image_rows completed without overrun\n");
  return 0;
}
