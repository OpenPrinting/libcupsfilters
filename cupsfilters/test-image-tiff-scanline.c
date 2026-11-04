//
// Regression test for the scanline handling of _cfImageReadTIFF() in
// libcupsfilters (issues #116 and #172).
//
// Copyright © 2026 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Each case writes a small TIFF file with libtiff, opens it with
// cfImageOpen() and compares every decoded pixel with the expected one:
//
// - 8-bit gray with 1, 2 (alpha) and 4 samples per pixel; with 4 samples
//   libtiff's scanline is wider than the pixel buffer it used to be read
//   into (#116),
// - 2- and 4-bit RGB and RGBA, whose samples are packed across byte
//   boundaries (#172), and 8-bit RGB and RGBA,
// - some of them right to left (ORIENTATION_TOPRIGHT) as well.
//
// test-image-tiff-scanline.sh builds it with AddressSanitizer when it can,
// so libtiff writing a scanline past the pixel buffer aborts the test; a
// misdecoded row fails the pixel comparison.
//

#include <cupsfilters/image.h>
#include <tiffio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_WIDTH	17		// Odd, so packed rows end mid-byte
#define TEST_HEIGHT	3


//
// 'sample_value()' - Return the value of a sample in the test image.
//
// Color samples follow a pattern; the alpha sample (if any) is opaque.
//

static unsigned				// O - Sample value
sample_value(int x,			// I - Column
             int y,			// I - Row
             int s,			// I - Sample in the pixel
             int alpha_sample,		// I - Index of the alpha sample or -1
             int bits)			// I - Bits per sample
{
  unsigned	maxval = (1u << bits) - 1;	// Largest sample value


  if (s == alpha_sample)
    return (maxval);
  else
    return ((unsigned)(x * 3 + y * 5 + s * 7) % (maxval + 1));
}


//
// 'write_tiff()' - Write a test image with packed samples.
//

static int				// O - 0 on success, -1 on error
write_tiff(const char *filename,	// I - File to write
           uint16_t   photometric,	// I - Photometric interpretation
           int        samples,		// I - Samples per pixel
           int        bits,		// I - Bits per sample
           int        colors,		// I - Color samples per pixel
           int        alpha_sample,	// I - Index of the alpha sample or -1
           uint16_t   orientation)	// I - Orientation
{
  TIFF		*tif;			// TIFF file
  unsigned char	row[TEST_WIDTH * 4];	// Packed scanline
  uint16_t	extra[3];		// Extra sample types
  int		x, y, s;		// Looping vars
  unsigned	pos;			// Bit position of a sample


  if ((tif = TIFFOpen(filename, "w")) == NULL)
    return (-1);

  TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, TEST_WIDTH);
  TIFFSetField(tif, TIFFTAG_IMAGELENGTH, TEST_HEIGHT);
  TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits);
  TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples);
  TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, photometric);
  TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
  TIFFSetField(tif, TIFFTAG_ORIENTATION, orientation);
  TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TEST_HEIGHT);

  if (samples > colors)
  {
    for (s = colors; s < samples; s ++)
      extra[s - colors] = s == alpha_sample ? EXTRASAMPLE_UNASSALPHA :
					      EXTRASAMPLE_UNSPECIFIED;

    TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, samples - colors, extra);
  }

  for (y = 0; y < TEST_HEIGHT; y ++)
  {
    memset(row, 0, sizeof(row));

    for (x = 0; x < TEST_WIDTH; x ++)
      for (s = 0; s < samples; s ++)
      {
        pos = (unsigned)((x * samples + s) * bits);
        row[pos / 8] |= sample_value(x, y, s, alpha_sample, bits) <<
			(8 - bits - pos % 8);
      }

    if (TIFFWriteScanline(tif, row, y, 0) < 0)
    {
      TIFFClose(tif);
      return (-1);
    }
  }

  TIFFClose(tif);

  return (0);
}


//
// 'check_image()' - Write a test image, read it back and compare the pixels.
//

static int				// O - Number of failures
check_image(const char *name,		// I - Case name
            const char *filename,	// I - Scratch file
            uint16_t   photometric,	// I - Photometric interpretation
            int        samples,		// I - Samples per pixel
            int        bits,		// I - Bits per sample
            int        alpha_sample,	// I - Index of the alpha sample or -1
            uint16_t   orientation)	// I - Orientation
{
  cf_image_t	*img;			// Image
  cf_ib_t	pixels[TEST_WIDTH * 3];	// Decoded row
  int		colors,			// Color samples per pixel
		x, y, c,		// Looping vars
		tx;			// Column in the TIFF file
  unsigned	maxval = (1u << bits) - 1,	// Largest sample value
		expected;		// Expected pixel value


  colors = photometric == PHOTOMETRIC_RGB ? 3 : 1;

  if (write_tiff(filename, photometric, samples, bits, colors,
                 alpha_sample, orientation))
  {
    printf("FAIL: %s: unable to write the test image\n", name);
    return (1);
  }

  if ((img = cfImageOpen(filename, CF_IMAGE_RGB, CF_IMAGE_WHITE, 100, 0,
                         NULL)) == NULL)
  {
    printf("FAIL: %s: cfImageOpen() returned NULL\n", name);
    return (1);
  }

  if (cfImageGetWidth(img) != TEST_WIDTH ||
      cfImageGetHeight(img) != TEST_HEIGHT ||
      cfImageGetDepth(img) != colors)
  {
    printf("FAIL: %s: image is %ux%u, depth %d\n", name,
           cfImageGetWidth(img), cfImageGetHeight(img), cfImageGetDepth(img));
    cfImageClose(img);
    return (1);
  }

  for (y = 0; y < TEST_HEIGHT; y ++)
  {
    cfImageGetRow(img, 0, y, TEST_WIDTH, pixels);

    for (x = 0; x < TEST_WIDTH; x ++)
      for (c = 0; c < colors; c ++)
      {
        tx       = orientation == ORIENTATION_TOPRIGHT ? TEST_WIDTH - 1 - x : x;
        expected = 255 * sample_value(tx, y, c, alpha_sample, bits) / maxval;

        if (pixels[x * colors + c] != expected)
        {
          printf("FAIL: %s: pixel (%d,%d) sample %d is %u, expected %u\n",
                 name, x, y, c, pixels[x * colors + c], expected);
          cfImageClose(img);
          return (1);
        }
      }
  }

  cfImageClose(img);

  printf("PASS: %s\n", name);

  return (0);
}


//
// 'main()' - Run the regression test.
//

int					// O - Exit status
main(void)
{
  char		filename[1024];		// Scratch file
  const char	*tmpdir;		// Temporary directory
  int		fd,			// Scratch file descriptor
		failures = 0;		// Number of failures


  if ((tmpdir = getenv("TMPDIR")) == NULL)
    tmpdir = "/tmp";

  snprintf(filename, sizeof(filename), "%s/test-image-tiff-XXXXXX", tmpdir);
  if ((fd = mkstemp(filename)) < 0)
  {
    perror(filename);
    return (1);
  }
  close(fd);

  TIFFSetWarningHandler(NULL);

  failures += check_image("gray 8-bit", filename, PHOTOMETRIC_MINISBLACK,
                          1, 8, -1, ORIENTATION_TOPLEFT);
  failures += check_image("gray 8-bit with alpha", filename,
                          PHOTOMETRIC_MINISBLACK, 2, 8, 1,
                          ORIENTATION_TOPLEFT);
  failures += check_image("gray 8-bit with 4 samples", filename,
                          PHOTOMETRIC_MINISBLACK, 4, 8, -1,
                          ORIENTATION_TOPLEFT);
  failures += check_image("gray 8-bit with 4 samples, right to left",
                          filename, PHOTOMETRIC_MINISBLACK, 4, 8, -1,
                          ORIENTATION_TOPRIGHT);
  failures += check_image("RGB 2-bit", filename, PHOTOMETRIC_RGB, 3, 2, -1,
                          ORIENTATION_TOPLEFT);
  failures += check_image("RGB 2-bit, right to left", filename,
                          PHOTOMETRIC_RGB, 3, 2, -1, ORIENTATION_TOPRIGHT);
  failures += check_image("RGBA 2-bit", filename, PHOTOMETRIC_RGB, 4, 2, 3,
                          ORIENTATION_TOPLEFT);
  failures += check_image("RGB 4-bit", filename, PHOTOMETRIC_RGB, 3, 4, -1,
                          ORIENTATION_TOPLEFT);
  failures += check_image("RGBA 4-bit", filename, PHOTOMETRIC_RGB, 4, 4, 3,
                          ORIENTATION_TOPLEFT);
  failures += check_image("RGB 8-bit", filename, PHOTOMETRIC_RGB, 3, 8, -1,
                          ORIENTATION_TOPLEFT);
  failures += check_image("RGB 8-bit, right to left", filename,
                          PHOTOMETRIC_RGB, 3, 8, -1, ORIENTATION_TOPRIGHT);
  failures += check_image("RGBA 8-bit", filename, PHOTOMETRIC_RGB, 4, 8, 3,
                          ORIENTATION_TOPLEFT);

  unlink(filename);

  return (failures ? 1 : 0);
}
