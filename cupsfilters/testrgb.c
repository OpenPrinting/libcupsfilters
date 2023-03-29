//
// Test for the new RGB color separation code for libcupsfilters.
//
// Copyright 2007-2011 by Apple Inc.
// Copyright 1993-2006 by Easy Software Products, All Rights Reserved.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Contents:
//
//   main()       - Do color rgb tests.
//   test_gray()  - Test grayscale rgbs...
//   test_rgb()   - Test color rgbs...
//

//
// Include necessary headers.
//
#include <cupsfilters/libcups2.h>
#include <config.h>
#include <string.h>
#include <ctype.h>
#include "driver.h"
#include <sys/stat.h>

#ifdef USE_LCMS1
#  include <lcms.h>
#endif // USE_LCMS1


void	test_gray(cf_sample_t *samples, int num_samples,
	          int cube_size, int num_comps, const char *basename);
void	test_rgb(cf_sample_t *samples, int num_samples,
		 int cube_size, int num_comps,
		 const char *basename);


//
// 'main()' - Do color rgb tests.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  static cf_sample_t	CMYK[] =	// Basic 4-color sep
			{
			  //{ r,   g,   b   }, { C,   M,   Y,   K   }
			  { { 0,   0,   0   }, { 0,   0,   0,   255 } },
			  { { 255, 0,   0   }, { 0,   255, 240, 0   } },
			  { { 0,   255, 0   }, { 200, 0,   200, 0   } },
			  { { 255, 255, 0   }, { 0,   0,   240, 0   } },
			  { { 0,   0,   255 }, { 200, 200, 0,   0   } },
			  { { 255, 0,   255 }, { 0,   200, 0,   0   } },
			  { { 0,   255, 255 }, { 200, 0,   0,   0   } },
			  { { 255, 255, 255 }, { 0,   0,   0,   0   } }
			};


  //
  // Make the test directory...
  //

  mkdir("test", 0755);

  //
  // Run tests for CMYK and CMYK separations...
  //

  test_rgb(CMYK, 8, 2, 4, "test/rgb-cmyk");

  test_gray(CMYK, 8, 2, 4, "test/gray-cmyk");

  //
  // Return with no errors...
  //

  return (0);
}


//
// 'test_gray()' - Test grayscale rgbs...
//

void
test_gray(cf_sample_t   *samples,	// I - Sample values
          int           num_samples,	// I - Number of samples
	  int           cube_size,	// I - Cube size
          int           num_comps,	// I - Number of components
	  const char    *basename)	// I - Base filename of output
{
  int			i;		// Looping var
  char			filename[255];	// Output filename
  char			line[255];	// Line from PPM file
  int			width, height;	// Width and height of test image
  int			x, y;		// Current coordinate in image
  int			r, g, b;	// Current RGB color
  unsigned char		input[7000];	// Line to rgbarate
  unsigned char		output[48000],	// Output rgb data
			*outptr;	// Pointer in output
  FILE			*in;		// Input PPM file
  FILE			*out[CF_MAX_CHAN];
					// Output PGM files
  FILE			*comp;		// Composite output
  cf_rgb_t		*rgb;		// Color separation


  //
  // Open the test image...
  //

  in = fopen("image.pgm", "rb");
  while (fgets(line, sizeof(line), in) != NULL)
    if (isdigit(line[0]))
      break;

  sscanf(line, "%d%d", &width, &height);

  if (fgets(line, sizeof(line), in)); // Ignore return value of fgets()

  //
  // Create the color rgb...
  //

  rgb = cfRGBNew(num_samples, samples, cube_size, num_comps);

  //
  // Open the color rgb files...
  //

  for (i = 0; i < num_comps; i ++)
  {
    sprintf(filename, "%s%d.pgm", basename, i);
    out[i] = fopen(filename, "wb");

    fprintf(out[i], "P5\n%d %d 255\n", width, height);
  }

  sprintf(filename, "%s.ppm", basename);
  comp = fopen(filename, "wb");

  fprintf(comp, "P6\n%d %d 255\n", width, height);

  //
  // Read the image and do the rgbs...
  //

  for (y = 0; y < height; y ++)
  {
    if (fread(input, width, 1, in)); // Ignore return value of fread()

    cfRGBDoGray(rgb, input, output, width);

    for (x = 0, outptr = output; x < width; x ++, outptr += num_comps)
    {
      for (i = 0; i < num_comps; i ++)
        putc(255 - outptr[i], out[i]);

      r = 255;
      g = 255;
      b = 255;

      r -= outptr[0];
      g -= outptr[1];
      b -= outptr[2];

      r -= outptr[3];
      g -= outptr[3];
      b -= outptr[3];

      if (num_comps > 4)
      {
        r -= outptr[4] / 2;
	g -= outptr[5] / 2;
      }

      if (num_comps > 6)
      {
        r -= outptr[6] / 2;
	g -= outptr[6] / 2;
	b -= outptr[6] / 2;
      }

      if (r < 0)
        putc(0, comp);
      else
        putc(r, comp);

      if (g < 0)
        putc(0, comp);
      else
        putc(g, comp);

      if (b < 0)
        putc(0, comp);
      else
        putc(b, comp);
    }
  }

  for (i = 0; i < num_comps; i ++)
    fclose(out[i]);

  fclose(comp);
  fclose(in);

  cfRGBDelete(rgb);
}


//
// 'test_rgb()' - Test color rgbs...
//

void
test_rgb(cf_sample_t   *samples,	// I - Sample values
         int           num_samples,	// I - Number of samples
	 int           cube_size,	// I - Cube size
         int           num_comps,	// I - Number of components
	 const char    *basename)	// I - Base filename of output
{
  int			i;		// Looping var
  char			filename[255];	// Output filename
  char			line[255];	// Line from PPM file
  int			width, height;	// Width and height of test image
  int			x, y;		// Current coordinate in image
  int			r, g, b;	// Current RGB color
  unsigned char		input[7000];	// Line to rgbarate
  unsigned char		output[48000],	// Output rgb data
			*outptr;	// Pointer in output
  FILE			*in;		// Input PPM file
  FILE			*out[CF_MAX_CHAN];
					// Output PGM files
  FILE			*comp;		// Composite output
  cf_rgb_t		*rgb;		// Color separation


  //
  // Open the test image...
  //

  in = fopen("image.ppm", "rb");
  while (fgets(line, sizeof(line), in) != NULL)
    if (isdigit(line[0]))
      break;

  sscanf(line, "%d%d", &width, &height);

  if (fgets(line, sizeof(line), in)); // Ignore return value of fgets()

  //
  // Create the color rgb...
  //

  rgb = cfRGBNew(num_samples, samples, cube_size, num_comps);

  //
  // Open the color rgb files...
  //

  for (i = 0; i < num_comps; i ++)
  {
    sprintf(filename, "%s%d.pgm", basename, i);
    out[i] = fopen(filename, "wb");

    fprintf(out[i], "P5\n%d %d 255\n", width, height);
  }

  sprintf(filename, "%s.ppm", basename);
  comp = fopen(filename, "wb");

  fprintf(comp, "P6\n%d %d 255\n", width, height);

  //
  // Read the image and do the rgbs...
  //

  for (y = 0; y < height; y ++)
  {
    if (fread(input, width, 3, in)); // Ignore return value of fread()

    cfRGBDoRGB(rgb, input, output, width);

    for (x = 0, outptr = output; x < width; x ++, outptr += num_comps)
    {
      for (i = 0; i < num_comps; i ++)
        putc(255 - outptr[i], out[i]);

      r = 255;
      g = 255;
      b = 255;

      r -= outptr[0];
      g -= outptr[1];
      b -= outptr[2];

      r -= outptr[3];
      g -= outptr[3];
      b -= outptr[3];

      if (num_comps > 4)
      {
        r -= outptr[4] / 2;
	g -= outptr[5] / 2;
      }

      if (num_comps > 6)
      {
        r -= outptr[6] / 2;
	g -= outptr[6] / 2;
	b -= outptr[6] / 2;
      }

      if (r < 0)
        putc(0, comp);
      else
        putc(r, comp);

      if (g < 0)
        putc(0, comp);
      else
        putc(g, comp);

      if (b < 0)
        putc(0, comp);
      else
        putc(b, comp);
    }
  }

  for (i = 0; i < num_comps; i ++)
    fclose(out[i]);

  fclose(comp);
  fclose(in);

  cfRGBDelete(rgb);
}
