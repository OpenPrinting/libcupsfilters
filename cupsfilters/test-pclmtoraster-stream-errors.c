//
// PCLm image stream failure regression tests (Issue #173).
//
// Copyright © 2026 by OpenPrinting.
//
// Licensed under Apache License v2.0. See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <cupsfilters/filter.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


typedef struct
{
  const char *name;
  const char *filter;
  int height;
  int partial;
  int no_images;
  int fail;
} test_case_t;


static void
write_image(FILE *fp, int number, const char *filter, int height)
{
  static const unsigned char pixels[] = { 0, 255, 0 };

  fprintf(fp, "%d 0 obj\n<< /Type /XObject /Subtype /Image /Width 1 "
              "/Height %d /ColorSpace /DeviceRGB /BitsPerComponent 8 ",
          number, height);
  if (filter)
    fprintf(fp, "/Filter /%s ", filter);
  fprintf(fp, "/Length 3 >>\nstream\n");
  fwrite(pixels, 1, sizeof(pixels), fp);
  fprintf(fp, "\nendstream\nendobj\n");
}


static int
write_pdf(const char *filename, const test_case_t *test)
{
  FILE *fp = fopen(filename, "wb");
  long offsets[7], xref;
  int i, count = test->partial ? 7 : 6;
  int failed;
  const char *content = test->no_images ? "" : "q 72 0 0 72 0 0 cm /Image1 Do Q\n";

  if (!fp)
    return (1);
  fprintf(fp, "%%PDF-1.4\n");
  offsets[1] = ftell(fp);
  fprintf(fp, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
  offsets[2] = ftell(fp);
  fprintf(fp, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
  offsets[3] = ftell(fp);
  fprintf(fp, "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 72 72] "
              "/Resources << /XObject << ");
  if (!test->no_images)
    fprintf(fp, "/Image1 4 0 R ");
  if (test->partial)
    fprintf(fp, "/Image2 6 0 R ");
  fprintf(fp, ">> >> /Contents 5 0 R >>\nendobj\n");
  offsets[4] = ftell(fp);
  write_image(fp, 4, test->partial ? NULL : test->filter, test->height);
  offsets[5] = ftell(fp);
  fprintf(fp, "5 0 obj\n<< /Length %zu >>\nstream\n%sendstream\nendobj\n",
          strlen(content), content);
  if (test->partial)
  {
    offsets[6] = ftell(fp);
    write_image(fp, 6, test->filter, 1);
  }
  xref = ftell(fp);
  fprintf(fp, "xref\n0 %d\n0000000000 65535 f \n", count);
  for (i = 1; i < count; i ++)
    fprintf(fp, "%010ld 00000 n \n", offsets[i]);
  fprintf(fp, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
          count, xref);
  failed = ferror(fp);
  if (fclose(fp))
    failed = 1;
  return (failed);
}


static int
run_case(const char *directory, const test_case_t *test)
{
  char input[1024], output[1024];
  cf_filter_data_t data;
  cf_filter_out_format_t format = CF_FILTER_OUT_FORMAT_PWG_RASTER;
  struct stat st;
  int inputfd, outputfd, status, failed;

  snprintf(input, sizeof(input), "%s/%s.pdf", directory, test->name);
  snprintf(output, sizeof(output), "%s/%s.pwg", directory, test->name);
  if (write_pdf(input, test))
    return (1);
  inputfd = open(input, O_RDONLY);
  outputfd = open(output, O_CREAT | O_TRUNC | O_WRONLY, 0600);
  if (inputfd < 0 || outputfd < 0)
  {
    if (inputfd >= 0)
      close(inputfd);
    if (outputfd >= 0)
      close(outputfd);
    unlink(input);
    unlink(output);
    return (1);
  }
  memset(&data, 0, sizeof(data));
  data.copies = 1;
  data.content_type = "application/PCLm";
  data.final_content_type = "image/pwg-raster";
  status = cfFilterPCLmToRaster(inputfd, outputfd, 1, &data, &format);
  close(outputfd);
  failed = test->fail ? status == 0 : status != 0;
  if (!test->fail && (stat(output, &st) || st.st_size <= 4))
    failed = 1;
  printf("%s: status=%d, expected %s: %s\n", test->name, status,
         test->fail ? "failure" : "success", failed ? "FAIL" : "PASS");
  unlink(input);
  unlink(output);
  return (failed);
}


int
main(void)
{
  char directory[] = "pclm-stream-errors.XXXXXX";
  size_t i;
  int failed = 0;
  static const test_case_t tests[] =
  {
    { "valid", NULL, 1, 0, 0, 0 },
    { "unsupported-filter", "Unsupported", 1, 0, 0, 1 },
    { "decode-error", "FlateDecode", 1, 0, 0, 1 },
    { "short-stream", NULL, 2, 0, 0, 1 },
    { "partial-read", "Unsupported", 1, 1, 0, 1 },
    { "no-images", NULL, 1, 0, 1, 1 }
  };

  if (!mkdtemp(directory))
    return (1);
  for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i ++)
    failed |= run_case(directory, tests + i);
  if (rmdir(directory))
    failed = 1;
  return (failed);
}
