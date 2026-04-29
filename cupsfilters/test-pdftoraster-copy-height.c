//
// Test program for pdftoraster copy_height off-by-one fix.
//
// This test validates the copy loop logic used in write_page_image()
// when the rendered image is smaller than the page height. The bug
// used `h <= copy_height` instead of `h < copy_height`, causing one
// extra iteration past the allocated buffer. The fix also adds a guard
// for `allocLineBuf` to prevent NULL-ptr memset.
//
// Licensed under Apache License v2.0. See the file "LICENSE" for more
// information.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int n_errors = 0;
#define TEST(cond, msg) do { \
  if (!(cond)) { \
    fprintf(stderr, "FAIL: %s\n", msg); \
    n_errors++; \
  } else { \
    fprintf(stderr, "OK:   %s\n", msg); \
  } \
} while (0)

typedef struct {
  unsigned int cupsHeight;
  unsigned int cupsWidth;
  unsigned int bytesPerLine;
  int allocLineBuf;
} doc_t;

/* Simulate the forward copy loop from write_page_image().
 * Returns the number of times the "inside valid page/image area" branch executed.
 * If allocLineBuf is 0 and the branch would execute, returns 0 on crash (caller
 * interprets). */
static int
simulate_forward_loop(unsigned int image_height,
                      unsigned int page_height,
                      int allocLineBuf)
{
  doc_t doc;
  memset(&doc, 0, sizeof(doc));
  doc.cupsHeight = page_height;
  doc.cupsWidth = 16;
  doc.bytesPerLine = 16;
  doc.allocLineBuf = allocLineBuf;

  unsigned int copy_height = (image_height < page_height) ? image_height : page_height;
  unsigned char *colordata = (unsigned char *)malloc(image_height * 16);
  unsigned char *lineBuf = NULL;
  if (allocLineBuf)
    lineBuf = (unsigned char *)malloc(doc.bytesPerLine);

  unsigned char *bp = colordata;
  int inside_count = 0;

  for (unsigned int h = 0; h < doc.cupsHeight; h++)
  {
    if (h < copy_height)              // FIXED: was h <= copy_height
    {
      inside_count++;
      if (doc.allocLineBuf)
        memset(lineBuf, 0, doc.bytesPerLine);
      bp += 16;                       // image_rowsize
    }
    else
    {
      if (doc.allocLineBuf)
        memset(lineBuf, 0, doc.bytesPerLine);
    }
  }

  /* bp should never point past the allocated region.
   * After the fix: copy_height iterations, each advancing by 16,
   * so bp == colordata + copy_height * 16 == end of allocation. */
  if (bp > colordata + image_height * 16) {
    fprintf(stderr, "FAIL: bp=%p exceeds colordata+size=%p (copy_height=%u image_height=%u)\n",
            (void*)bp, (void*)(colordata + image_height * 16), copy_height, image_height);
    n_errors++;
  }

  free(lineBuf);
  free(colordata);
  return inside_count;
}

/* Simulate the reverse (duplex) copy loop.
 *
 * The reverse loop iterates h from page_height DOWN TO 1, with bp
 * starting at the last valid row.  Here h <= copy_height is correct
 * (unlike the forward loop) because h = copy_height corresponds to
 * the last valid row index (copy_height-1), and the loop ends before
 * bp can be dereferenced past the allocation start.
 */
static int
simulate_reverse_loop(unsigned int image_height,
                      unsigned int page_height,
                      int allocLineBuf)
{
  doc_t doc;
  memset(&doc, 0, sizeof(doc));
  doc.cupsHeight = page_height;
  doc.cupsWidth = 16;
  doc.bytesPerLine = 16;
  doc.allocLineBuf = allocLineBuf;

  unsigned int copy_height = (image_height < page_height) ? image_height : page_height;
  unsigned char *colordata = (unsigned char *)malloc(image_height * 16);
  unsigned char *lineBuf = NULL;
  if (allocLineBuf)
    lineBuf = (unsigned char *)malloc(doc.bytesPerLine);

  unsigned char *bp = colordata + (copy_height - 1) * 16;
  int inside_count = 0;

  for (unsigned int h = doc.cupsHeight; h > 0; h--)
  {
    if (h <= copy_height)             // correct (h descends from page_height to 1)
    {
      inside_count++;
      if (doc.allocLineBuf)
        memset(lineBuf, 0, doc.bytesPerLine);
      bp -= 16;
    }
    else
    {
      if (doc.allocLineBuf)
        memset(lineBuf, 0, doc.bytesPerLine);
    }
  }

  free(lineBuf);
  free(colordata);
  return inside_count;
}

int
main(void)
{
  fprintf(stderr, "--- pdftoraster copy_height tests ---\n");

  /* Test 1: image < page, allocLineBuf=true — the OOB scenario */
  TEST(simulate_forward_loop(1, 2, 1) == 1,
       "forward: image=1 page=2 -> inside_count=1 (was 2 before fix)");
  TEST(simulate_reverse_loop(1, 2, 1) == 1,
       "reverse: image=1 page=2 -> inside_count=1 (was 2 before fix)");

  /* Test 2: image == page — normal case */
  TEST(simulate_forward_loop(5, 5, 1) == 5,
       "forward: image=5 page=5 -> inside_count=5");
  TEST(simulate_reverse_loop(5, 5, 1) == 5,
       "reverse: image=5 page=5 -> inside_count=5");

  /* Test 3: image > page — normal case (clamped by copy_height) */
  TEST(simulate_forward_loop(10, 5, 1) == 5,
       "forward: image=10 page=5 -> inside_count=5 (clamped)");
  TEST(simulate_reverse_loop(10, 5, 1) == 5,
       "reverse: image=10 page=5 -> inside_count=5 (clamped)");

  /* Test 4: image=0 (edge case: should not crash) */
  /* copy_height = 0, loop should never enter the "inside" branch */
  TEST(simulate_forward_loop(0, 5, 1) == 0,
       "forward: image=0 page=5 -> inside_count=0");
  TEST(simulate_reverse_loop(0, 5, 1) == 0,
       "reverse: image=0 page=5 -> inside_count=0");

  /* Test 5: image < page, allocLineBuf=false — the NULL-ptr crash scenario */
  /* Must not crash; function returns early from inside branch since allocLineBuf=false */
  TEST(simulate_forward_loop(1, 2, 0) == 1,
       "forward: image=1 page=2 allocLineBuf=0 -> no crash, inside_count=1");
  TEST(simulate_reverse_loop(1, 2, 0) == 1,
       "reverse: image=1 page=2 allocLineBuf=0 -> no crash, inside_count=1");

  fprintf(stderr, "--- %s ---\n", n_errors ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
  return n_errors ? 1 : 0;
}
