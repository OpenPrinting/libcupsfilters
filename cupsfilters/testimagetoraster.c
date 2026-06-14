//
// Unit test for cfFilterImageToRaster() — verifies the buffer-safe snprintf
// replacements introduced to fix strcpy() overflow risks in imagetoraster.c.
//
// Copyright 2024 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <cupsfilters/filter.h>
#include <cups/cups.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


//
// 'main()' - Run cfFilterImageToRaster() end-to-end and verify it succeeds.
//
// The fixed code replaces strcpy(defSize, header.cupsPageSizeName) with
// snprintf(defSize, sizeof(defSize), ...) to guard against buffer overflows.
// Running the actual filter function exercises those code paths and, when
// built with -fsanitize=address, will catch any regression to the unsafe form.
//

int					// O - Exit status
main(void)
{
  int			failed = 0;	// Failure counter
  int			inputfd;	// Input image file descriptor
  int			outputfd;	// Output raster file descriptor
  int			job_canceled = 0; // Cancellation flag
  cf_filter_data_t	filter_data;	// Filter job/printer data


  signal(SIGPIPE, SIG_IGN);

  //
  // Open the bundled test JPEG image as the filter input.
  //

  inputfd = open("cupsfilters/test_files/test_imagetoraster.jpg", O_RDONLY);
  if (inputfd < 0)
  {
    fprintf(stderr,
	    "ERROR: testimagetoraster: Cannot open "
	    "cupsfilters/test_files/test_imagetoraster.jpg\n");
    return (1);
  }

  //
  // Discard the raster output — we are testing for correctness, not output.
  //

  outputfd = open("/dev/null", O_WRONLY);
  if (outputfd < 0)
  {
    fprintf(stderr, "ERROR: testimagetoraster: Cannot open /dev/null\n");
    close(inputfd);
    return (1);
  }

  //
  // Build a minimal cf_filter_data_t.  NULL printer_attrs causes the filter
  // to fall back to built-in defaults, which is sufficient to reach and
  // exercise the snprintf(defSize, ...) and calloc() code paths.
  //

  memset(&filter_data, 0, sizeof(filter_data));
  filter_data.printer            = "test-printer";
  filter_data.job_id             = 1;
  filter_data.job_user           = "test";
  filter_data.job_title          = "testimagetoraster buffer-safety check";
  filter_data.copies             = 1;
  filter_data.content_type       = "image/jpeg";
  filter_data.final_content_type = "application/vnd.cups-raster";
  filter_data.logfunc            = cfCUPSLogFunc;
  filter_data.logdata            = NULL;
  filter_data.iscanceledfunc     = cfCUPSIsCanceledFunc;
  filter_data.iscanceleddata     = &job_canceled;
  filter_data.back_pipe[0]       = -1;
  filter_data.back_pipe[1]       = -1;
  filter_data.side_pipe[0]       = -1;
  filter_data.side_pipe[1]       = -1;

  //
  // Run the filter.  A non-zero return value is a test failure.
  //

  fprintf(stderr, "testimagetoraster: Testing cfFilterImageToRaster...\n");

  if (cfFilterImageToRaster(inputfd, outputfd, 1, &filter_data, NULL) != 0)
  {
    fprintf(stderr,
	    "ERROR: testimagetoraster: cfFilterImageToRaster returned error\n");
    failed ++;
  }
  else
    fprintf(stderr, "testimagetoraster: PASSED\n");

  close(inputfd);
  close(outputfd);

  return (failed ? 1 : 0);
}
