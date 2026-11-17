//
// Regression test for __cfFontEmbedEmbPDFFWCIDWidths(), which builds the
// /W array of CID fonts embedded by texttopdf.
//
// A run of five or more glyphs with the same non-default width is turned
// into a range entry (-len c1 w). The array must decode back to the exact
// width of every glyph, and must be terminated within its allocation.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <cupsfilters/fontembed-private.h>
#include "embed-pdf-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct
{
  const int *widths;
  int       len;
} widths_ctx_t;


static int
get_width(void *context,
	  int gid) // {{{
{
  widths_ctx_t *ctx = (widths_ctx_t *)context;

  return (ctx->widths[gid]);
}
// }}}


//
// Decode warray the same way _cfFontEmbedEmbPDFSimpleFont() writes /W, into
// one width per glyph. Returns 0 on success.
//

static int
decode(const _cf_fontembed_emb_pdf_font_widths_t *fw,
       int len,
       int *out) // {{{
{
  // Upper bound for a well-formed array: every glyph in its own array
  // region (len c w), plus the terminator.
  int max = 3 * len + 1;
  int iA = 0, gid, n;

  for (gid = 0; gid < len; gid ++)
    out[gid] = fw->default_width;

  while (iA < max && fw->warray[iA])
  {
    if (fw->warray[iA] < 0)
    {
      // range: -len c1 w  ->  c1 .. c1 + len
      if (iA + 2 >= max)
	return (-1);
      n = -fw->warray[iA];
      gid = fw->warray[iA + 1];
      if (gid < 0 || gid + n >= len)
      {
	fprintf(stderr, "range %d..%d out of bounds\n", gid, gid + n);
	return (-1);
      }
      for (; n >= 0; n --, gid ++)
	out[gid] = fw->warray[iA + 2];
      iA += 3;
    }
    else
    {
      // array: len c w[len]
      n = fw->warray[iA ++];
      if (iA + n >= max)
	return (-1);
      gid = fw->warray[iA ++];
      if (gid < 0 || gid + n > len)
      {
	fprintf(stderr, "array %d+%d out of bounds\n", gid, n);
	return (-1);
      }
      for (; n > 0; n --, gid ++)
	out[gid] = fw->warray[iA ++];
    }
  }

  if (iA >= max)
  {
    fprintf(stderr, "warray not terminated\n");
    return (-1);
  }
  return (0);
}
// }}}


static int
check(const char *name,
      const int *widths,
      int len,
      int default_width) // {{{
{
  widths_ctx_t ctx = { widths, len };
  _cf_fontembed_emb_pdf_font_widths_t *fw;
  int *out, gid, ret = 0;

  fw = __cfFontEmbedEmbPDFFWCIDWidths(NULL, len, default_width, get_width,
				      &ctx);
  if (!fw)
  {
    fprintf(stderr, "FAIL %s: allocation failed\n", name);
    return (1);
  }

  out = calloc(len, sizeof(int));
  if (decode(fw, len, out))
    ret = 1;
  else
  {
    for (gid = 0; gid < len; gid ++)
      if (out[gid] != widths[gid])
      {
	fprintf(stderr, "glyph %d: got width %d, want %d\n", gid, out[gid],
		widths[gid]);
	ret = 1;
      }
  }

  printf("%s %s\n", ret ? "FAIL" : "PASS", name);
  free(out);
  free(fw);
  return (ret);
}
// }}}


int
main(void) // {{{
{
  int ret = 0;

  // Five equal non-default widths right at the start of an array region.
  static const int run_at_start[] =
    { 500, 600, 600, 600, 600, 600, 500, 500 };

  // Five equal widths after other non-default widths in the same region.
  static const int run_after_array[] =
    { 500, 700, 710, 600, 600, 600, 600, 600, 720, 500 };

  // A long run, then an array region, then another long run.
  static const int long_runs[] =
    { 278, 278, 278, 278, 278, 278, 278, 278, 355, 556, 889,
      556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 500 };

  // Proportional font: widths vary, with runs of equal digit widths.
  static const int proportional[] =
    { 0, 278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278,
      333, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278,
      278, 584, 584, 584, 556, 1015, 667, 667, 722, 722, 667, 611, 778,
      722, 278, 500, 667, 556, 833, 722, 778, 667, 778, 722, 667, 611 };

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

  ret |= check("run-at-start", run_at_start, N(run_at_start), 500);
  ret |= check("run-after-array", run_after_array, N(run_after_array), 500);
  ret |= check("long-runs", long_runs, N(long_runs), 500);
  ret |= check("proportional", proportional, N(proportional), 556);
  ret |= check("proportional-estimated-dw", proportional, N(proportional),
	       -1);

  return (ret);
}
// }}}
