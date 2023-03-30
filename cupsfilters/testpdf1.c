//
// PDF file output test program 1 for libcupsfilters.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
#include <cupsfilters/libcups2-private.h>
#include "pdfutils-private.h"
#include "debug-internal.h"
#include <string.h>

int
main()
{
  _cf_pdf_out_t *pdf;

  pdf = _cfPDFOutNew();
  DEBUG_assert(pdf);

  _cfPDFOutBeginPDF(pdf);

  // bad font
  int font_obj = _cfPDFOutAddXRef(pdf);
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "<</Type/Font\n"
		 "  /Subtype /Type1\n" // /TrueType,/Type3
		 "  /BaseFont /%s\n"
		 ">>\n"
		 "endobj\n",
		 font_obj, "Courier");
  // test
  const int PageWidth = 595, PageLength = 842;
  int cobj = _cfPDFOutAddXRef(pdf);
  const char buf[] = "BT /a 10 Tf (abc) Tj ET";
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "<</Length %ld\n"
		 ">>\n"
		 "stream\n"
		 "%s\n"
		 "endstream\n"
		 "endobj\n",
		 cobj, strlen(buf), buf);

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

  return (0);
}
