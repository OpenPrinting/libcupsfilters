//
// Regression test for the cfFilterTextToText() output-page size overflow.
//
// "PageWidth" and "PageHeight" are accepted straight from the job options
// with only a lower bound of zero, so a job that requests an enormous page
// (something a hostile or simply buggy print client can put in job options
// without any special privilege) ends up with num_columns/num_lines values
// whose product overflows the signed int used to size the output page
// buffer. calloc() then gets handed a small wrapped size while the
// formatting loop keeps writing according to the real, unwrapped column
// count, so it walks straight past the end of the allocation.
//
// This test pulls in the real cupsfilters/texttotext.c so it drives the
// actual cfFilterTextToText() entry point, the same one a print job goes
// through in production. It feeds it a page-width/page-height combination
// that overflows the "page_size" computation and a one-line input file long
// enough to reach past a plausible wrapped allocation. Built with
// AddressSanitizer, the unfixed code writes past the undersized page buffer
// and ASan aborts; with the overflow guarded, cfFilterTextToText() simply
// rejects the bogus dimensions and falls back to sane defaults.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// Include the unit under test so its logic runs instrumented by ASan
// regardless of how the shared library itself was built.
#include "cupsfilters/texttotext.c"

int
main(void)
{
  char input_path[] = "/tmp/texttotext-overflow-input-XXXXXX";
  int input_fd = mkstemp(input_path);
  if (input_fd < 0)
  {
    perror("mkstemp");
    return 2;
  }

  // A single line of ordinary characters, long enough that were the page
  // buffer really only a handful of bytes (as it is once "page_size"
  // wraps), writing it out would run straight past the allocation.
  char line[4096];
  memset(line, 'A', sizeof(line) - 1);
  line[sizeof(line) - 1] = '\n';
  if (write(input_fd, line, sizeof(line)) != (ssize_t)sizeof(line))
  {
    perror("write");
    close(input_fd);
    unlink(input_path);
    return 2;
  }
  lseek(input_fd, 0, SEEK_SET);

  int output_fd = open("/dev/null", O_WRONLY);
  if (output_fd < 0)
  {
    perror("open /dev/null");
    close(input_fd);
    unlink(input_path);
    return 2;
  }

  int num_options = 0;
  cups_option_t *options = NULL;
  // A hostile-page-width job: the page is reported as roughly a billion
  // "columns" wide and a single line tall -- values well past anything a
  // real printer supports, but nothing in cfFilterTextToText() rejects
  // them before they reach the page_size computation.
  num_options = cupsAddOption("PageWidth", "1073741824", num_options,
                              &options);
  num_options = cupsAddOption("PageHeight", "1", num_options, &options);
  num_options = cupsAddOption("PageLeft", "0", num_options, &options);
  num_options = cupsAddOption("PageRight", "0", num_options, &options);

  cf_filter_data_t data;
  memset(&data, 0, sizeof(data));
  data.num_options = num_options;
  data.options = options;
  data.copies = 1;

  int ret = cfFilterTextToText(input_fd, output_fd, 0, &data, NULL);

  cupsFreeOptions(num_options, options);
  close(input_fd);
  close(output_fd);
  unlink(input_path);

  fprintf(stderr, "cfFilterTextToText returned %d without triggering an "
                  "out-of-bounds write\n", ret);
  return 0;
}
