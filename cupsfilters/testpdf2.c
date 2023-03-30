//
// PDF file output test program 2 (fontembed) for libcupsfilters.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
#include <cupsfilters/libcups2-private.h>
#include "pdfutils-private.h"
#include "config.h"
#include "debug-internal.h"
#include "cupsfilters/fontembed-private.h"

#include <stdio.h>

static inline void
write_string(_cf_pdf_out_t *pdf,
	     _cf_fontembed_emb_params_t *emb,
	     const char *str) // {{{
{
  DEBUG_assert(pdf);
  DEBUG_assert(emb);
  int iA;

  if (emb->plan & _CF_FONTEMBED_EMB_A_MULTIBYTE)
  {
    putc('<', stdout);
    for (iA = 0; str[iA]; iA ++)
    {
      const unsigned short gid = _cfFontEmbedEmbGet(emb,
						    (unsigned char)str[iA]);
      fprintf(stdout, "%04x", gid);
    }
    putc('>', stdout);
    pdf->filepos += 4 * iA + 2;
  }
  else
  {
    for (iA = 0; str[iA]; iA ++)
    {
      _cfFontEmbedEmbGet(emb, (unsigned char)str[iA]);
      // TODO: pdf: otf_from_pdf_default_encoding
    }
    _cfPDFOutputString(pdf, str, -1);
  }
}
// }}}


int
main(int  argc,
     char *argv[])
{
  _cf_pdf_out_t *pdf;

  pdf = _cfPDFOutNew();
  DEBUG_assert(pdf);

  _cfPDFOutBeginPDF(pdf);

  // font, pt.1 
  const char *fn = TESTFONT;
  _cf_fontembed_otf_file_t *otf = NULL;

  if (argc == 2)
    fn = argv[1];

  otf = _cfFontEmbedOTFLoad(fn);
  if (!otf)
  {
    printf("Font %s was not loaded, exiting.\n", fn);
    return (1);
  }
  DEBUG_assert(otf);
  _cf_fontembed_fontfile_t *ff = _cfFontEmbedFontFileOpenSFNT(otf);
  _cf_fontembed_emb_params_t *emb =
    _cfFontEmbedEmbNew(ff,
		       _CF_FONTEMBED_EMB_DEST_PDF16,
		       _CF_FONTEMBED_EMB_C_FORCE_MULTIBYTE |
		       _CF_FONTEMBED_EMB_C_TAKE_FONTFILE);

  // test
  const int PageWidth = 595, PageLength = 842;
  const int cobj = _cfPDFOutAddXRef(pdf);
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "<</Length %d 0 R\n"
		 ">>\n"
		 "stream\n",
		 cobj, cobj + 1);
  long streamlen = -pdf->filepos;
  _cfPDFOutPrintF(pdf, "BT /a 10 Tf ");
  write_string(pdf, emb, "Test");
  _cfPDFOutPrintF(pdf, " Tj ET");

  streamlen += pdf->filepos;
  _cfPDFOutPrintF(pdf,
		 "\nendstream\n"
		 "endobj\n");
  const int clobj = _cfPDFOutAddXRef(pdf);
  DEBUG_assert(clobj == cobj + 1);
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "%ld\n"
		 "endobj\n",
		 clobj, streamlen);

  // font
  int font_obj = _cfPDFOutWriteFont(pdf, emb);
  DEBUG_assert(font_obj);

  int obj = _cfPDFOutAddXRef(pdf);
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "<</Type/Page\n"
		 "  /Parent 1 0 R\n"
		 "  /MediaBox [0 0 %d %d]\n"
		 "  /Contents %d 0 R\n"
		 "  /Resources << /Font << /a %d 0 R >> >>\n"
		 ">>\n"
		 "endobj\n",
		 obj, PageWidth, PageLength, cobj, font_obj);
                                                  // TODO: into pdf->
  _cfPDFOutAddPage(pdf, obj);
  _cfPDFOutFinishPDF(pdf);

  _cfPDFOutFree(pdf);

  _cfFontEmbedEmbClose(emb);

  return (0);
}
