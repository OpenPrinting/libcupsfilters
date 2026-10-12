//
// Harness for test-ghostscript-ocr.sh: run cfFilterGhostscriptOCR() and
// cfFilterGhostscript() on a file.
//
// Usage: test-ghostscript-ocr <mode> <input> <output> [tessdata_dir|-]
//                             [fallback] [seekable]
//
// Modes: ocr (parameter struct), options (job options only),
// content-type (OCR via final_content_type), image (raster-only PDF),
// available (cfFilterGhostscriptOCRAvailable(), prints the directory)
//
// Copyright © 2026 by OpenPrinting
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <cupsfilters/filter.h>
#include <cups/cups.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


//
// 'log_to_stderr()' - Log function printing everything to stderr
//

static void
log_to_stderr(void *data,            // I - Unused
	      cf_loglevel_t level,   // I - Log level
	      const char *message,   // I - Format string
	      ...)                   // I - Arguments
{
  va_list ap;
  static const char *levels[] = { "UNSPEC", "DEBUG", "INFO", "WARN", "ERROR",
				  "FATAL", "CONTROL" };

  (void)data;
  fprintf(stderr, "[%s] ",
	  (level >= 0 && level < (int)(sizeof(levels) / sizeof(levels[0])) ?
	   levels[level] : "?"));
  va_start(ap, message);
  vfprintf(stderr, message, ap);
  va_end(ap);
  fputc('\n', stderr);
}


//
// 'open_input()' - Open the input file directly or through a pipe
//

static int                           // O - File descriptor or -1
open_input(const char *path,         // I - Input file
	   int seekable)             // I - 1: open file, 0: feed via pipe
{
  int fd,
      pipefds[2];
  pid_t pid;
  char buf[8192];
  ssize_t n;

  if ((fd = open(path, O_RDONLY)) < 0)
  {
    perror("open input");
    return (-1);
  }

  if (seekable)
    return (fd);

  if (pipe(pipefds) < 0)
  {
    perror("pipe");
    close(fd);
    return (-1);
  }

  if ((pid = fork()) < 0)
  {
    perror("fork");
    close(fd);
    return (-1);
  }

  if (pid == 0)
  {
    // Child: copy the file into the pipe
    close(pipefds[0]);
    while ((n = read(fd, buf, sizeof(buf))) > 0)
      if (write(pipefds[1], buf, (size_t)n) != n)
	_exit(1);
    close(pipefds[1]);
    _exit(0);
  }

  close(pipefds[1]);
  close(fd);
  return (pipefds[0]);
}


//
// 'main()' - Run the requested mode
//

int                                  // O - Exit status
main(int argc,                       // I - Number of arguments
     char *argv[])                   // I - Arguments
{
  const char *mode,
	     *input,
	     *output,
	     *tessdata_dir = NULL;
  int fallback = 1,
      seekable = 1,
      infd,
      outfd,
      ret;
  cf_filter_data_t data;
  cf_filter_ghostscript_ocr_params_t params =
    CF_FILTER_GHOSTSCRIPT_OCR_PARAMS_INITIALIZER;
  cf_filter_out_format_t outformat;
  char dir[1024];

  if (argc < 4)
  {
    fprintf(stderr,
	    "Usage: %s <ocr|options|content-type|image|available> <input> "
	    "<output> [tessdata_dir|-] [fallback] [seekable]\n", argv[0]);
    return (2);
  }

  mode = argv[1];
  input = argv[2];
  output = argv[3];
  if (argc > 4 && strcmp(argv[4], "-"))
    tessdata_dir = argv[4];
  if (argc > 5)
    fallback = atoi(argv[5]);
  if (argc > 6)
    seekable = atoi(argv[6]);

  memset(&data, 0, sizeof(data));
  data.copies = 1;
  data.content_type = (char *)"application/pdf";
  data.final_content_type = (char *)"application/pdf";
  data.logfunc = log_to_stderr;

  if (!strcmp(mode, "options"))
  {
    data.num_options = cupsAddOption("ocr-language", "eng", data.num_options,
				     &data.options);
    data.num_options = cupsAddOption("ocr-fallback", fallback ? "true" :
				     "false", data.num_options,
				     &data.options);
    data.num_options = cupsAddOption("ocr-resolution", "300",
				     data.num_options, &data.options);
    if (tessdata_dir)
      data.num_options = cupsAddOption("ocr-tessdata-dir", tessdata_dir,
				       data.num_options, &data.options);
  }
  else
  {
    params.language = "eng";
    params.tessdata_dir = tessdata_dir;
    params.fallback = fallback;
    params.resolution = 300;
  }

  if (!strcmp(mode, "available"))
  {
    ret = cfFilterGhostscriptOCRAvailable(&data, &params, dir, sizeof(dir));
    if (ret)
      printf("%s\n", dir);
    return (ret ? 0 : 1);
  }

  if ((infd = open_input(input, seekable)) < 0)
    return (1);

  if ((outfd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
  {
    perror("open output");
    return (1);
  }

  if (!strcmp(mode, "ocr"))
    ret = cfFilterGhostscriptOCR(infd, outfd, seekable, &data, &params);
  else if (!strcmp(mode, "options"))
    ret = cfFilterGhostscriptOCR(infd, outfd, seekable, &data, NULL);
  else if (!strcmp(mode, "content-type"))
  {
    data.final_content_type = (char *)"application/vnd.cups-pdf-ocr";
    if (tessdata_dir)
      data.num_options = cupsAddOption("ocr-tessdata-dir", tessdata_dir,
				       data.num_options, &data.options);
    ret = cfFilterGhostscript(infd, outfd, seekable, &data, NULL);
  }
  else if (!strcmp(mode, "image"))
  {
    outformat = CF_FILTER_OUT_FORMAT_PDF_IMAGE;
    ret = cfFilterGhostscript(infd, outfd, seekable, &data, &outformat);
  }
  else
  {
    fprintf(stderr, "Unknown mode \"%s\"\n", mode);
    return (2);
  }

  while (wait(NULL) > 0);

  if (ret)
    fprintf(stderr, "Filter function returned %d\n", ret);

  return (ret ? 1 : 0);
}
