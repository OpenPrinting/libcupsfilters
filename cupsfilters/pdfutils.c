//
// PDF file output routines for libcupsfilters.
//
// Copyright 2008 by Tobias Hoffmann.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
#include <cupsfilters/libcups2.h>
#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include "pdfutils-private.h"
#include "fontembed-private.h"
#include "debug-internal.h"


//
// '_cfPDFOutPrintF()' - General output routine for our PDF
//
// Keeps track of characters actually written out
//

void
_cfPDFOutPrintF(_cf_pdf_out_t *pdf,
	       const char *fmt,...) // {{{
{
  int len;
  va_list ap;


  DEBUG_assert(pdf);

  va_start(ap, fmt);
  len = vprintf(fmt, ap);
  va_end(ap);
  pdf->filepos += len;
}
// }}}


//
// '_cfPDFOutputString()' - Write out an escaped PDF string: e.g.
//                         "(Text \(Test\)\n)"
//

void
_cfPDFOutputString(_cf_pdf_out_t *pdf,
		  const char *str,
		  int len) // {{{ -> len == -1: strlen()
{
  DEBUG_assert(pdf);
  DEBUG_assert(str);

  if (len == -1)
    len = strlen(str);
  putc('(', stdout);
  // escape special chars: \0 \\ \( \)  -- don't bother about balanced parens
  int iA = 0;
  for (; len > 0; iA ++, len --)
  {
    if ((str[iA] < 32) || (str[iA] > 126))
    {
      fwrite(str, 1, iA, stdout);
      fprintf(stdout, "\\%03o", (unsigned char)str[iA]);
      pdf->filepos += iA + 4;
      str += iA + 1;
      iA = -1;
    }
    else if ((str[iA] == '(') || (str[iA] == ')') || (str[iA] == '\\'))
    {
      fwrite(str, 1, iA, stdout);
      fprintf(stdout, "\\%c" ,str[iA]);
      pdf->filepos += iA + 2;
      str += iA + 1;
      iA = -1;
    }
  }
  pdf->filepos += iA + 2;
  fwrite(str, 1, iA, stdout);
  putc(')', stdout);
}
// }}}


//
// '_cfPDFOutputHexString()' - Write ot a string in hex, 2 digits per byte
//

void
_cfPDFOutputHexString(_cf_pdf_out_t *pdf,
		     const char *str,
		     int len) // {{{ -> len == -1: strlen()
{
  DEBUG_assert(pdf);
  DEBUG_assert(str);

  if (len == -1)
    len = strlen(str);
  pdf->filepos += 2 * len + 2;
  putc('<', stdout);
  for (; len > 0; str++, len--)
    fprintf(stdout, "%02x", (unsigned char)*str);
  putc('>', stdout);
}
// }}}


//
// '_cfPDFOutNew()' - Allocates a new _cf_pdf_out_t structure
//

_cf_pdf_out_t * // O - NULL on error
_cfPDFOutNew()  // {{{
{
  _cf_pdf_out_t *ret = malloc(sizeof(_cf_pdf_out_t));

  if (ret)
    memset(ret, 0, sizeof(_cf_pdf_out_t));

  return (ret);
}
// }}}


//
// '_cfPDFOutToPDFDate()' - Format the broken up timestamp according to
//                         PDF requirements for /CreationDate
//
// NOTE: uses statically allocated buffer
//

const char *
_cfPDFOutToPDFDate(struct tm *curtm) // {{{
{
  static char curdate[250];


  if (!curtm)
  {
    time_t curtime;
    curtime = time(NULL);
    curtm   = localtime(&curtime);
  }
  strftime(curdate, sizeof(curdate), "D:%Y%m%d%H%M%S%z", curtm);
  curdate[23] = 0;
  curdate[22] = '\'';
  curdate[21] = curdate[18];
  curdate[20] = curdate[17];
  curdate[19] = '\'';
  return (curdate);
}
// }}}


//
// '_cfPDFOutAddXRef()' - Begin a new object at current point of the 
//                       output stream and add it to the xref table.
//

int                                // O - Object number
_cfPDFOutAddXRef(_cf_pdf_out_t *pdf) // {{{
{
  DEBUG_assert(pdf);
  DEBUG_assert(pdf->xrefsize <= pdf->xrefalloc);

  if (pdf->xrefsize == pdf->xrefalloc)
  {
    long *tmp;
    pdf->xrefalloc += 50;
    tmp = realloc(pdf->xref, sizeof(long) * pdf->xrefalloc);
    if (!tmp)
    {
      pdf->xrefalloc=-1;
      return -1;
    }
    pdf->xref = tmp;
  }
  pdf->xref[pdf->xrefsize++] = pdf->filepos;
  return (pdf->xrefsize); // xrefsize + 1
}
// }}}


//
// '_cfPDFOutAddPage()' - Adds page dictionary Object to the global pages tree
//

int                               // O - Return 0 on error
_cfPDFOutAddPage(_cf_pdf_out_t *pdf,
		int obj) // {{{
{
  DEBUG_assert(pdf);
  DEBUG_assert(obj > 0);
  DEBUG_assert(pdf->pagessize <= pdf->pagesalloc);

  if (pdf->pagessize == pdf->pagesalloc)
  {
    int *tmp;
    pdf->pagesalloc += 10;
    tmp = realloc(pdf->pages, sizeof(int) * pdf->pagesalloc);
    if (!tmp)
    {
      pdf->pagesalloc = -1;
      return (0);
    }
    pdf->pages = tmp;
  }
  pdf->pages[pdf->pagessize++] = obj;
  return (1);
}
// }}}


//
// '_cfPDFOutAddKeyValue()' - Add a key/value pair to the document's info
//                           dictionary
//

int                               // O - Return 0 on error
_cfPDFOutAddKeyValue(_cf_pdf_out_t *pdf,
		    const char *key,
		    const char *val) // {{{
{
  DEBUG_assert(pdf);
  DEBUG_assert(pdf->kvsize <= pdf->kvalloc);

  if (pdf->kvsize == pdf->kvalloc)
  {
    struct _cf_keyval_t *tmp;
    pdf->kvalloc += 10;
    tmp = realloc(pdf->kv, sizeof(struct _cf_keyval_t) * pdf->kvalloc);
    if (!tmp)
    {
      pdf->kvalloc = -1;
      return (0);
    }
    pdf->kv = tmp;
  }
  pdf->kv[pdf->kvsize].key = strdup(key);
  pdf->kv[pdf->kvsize].value = strdup(val);
  if ((!pdf->kv[pdf->kvsize].key) || (!pdf->kv[pdf->kvsize].value))
    return (0);
  pdf->kvsize ++;
  return (1);
}
// }}}


//
// '_cfPDFOutBeginPDF()' - Start outputting a PDF
//

int                                 // O - Return 0 on error
_cfPDFOutBeginPDF(_cf_pdf_out_t *pdf) // ,...output_device?...) // {{{
{
  int pages_obj;


  DEBUG_assert(pdf);
  DEBUG_assert(pdf->kvsize == 0); // otherwise: finish_pdf has not been called

  pdf->xrefsize = pdf->pagessize = 0;
  pdf->filepos = 0;
  pages_obj = _cfPDFOutAddXRef(pdf); // fixed later
  if (pages_obj != 1)
    return (0);
  _cfPDFOutPrintF(pdf, "%%PDF-1.3\n");
  return (1);
}
// }}}


//
// '_cfPDFOutFinishPDF()' - Finish outputting the PDF
//

void
_cfPDFOutFinishPDF(_cf_pdf_out_t *pdf) // {{{
{
  int iA;
  int root_obj,
      info_obj = 0,
      xref_start;


  DEBUG_assert(pdf && (pdf->filepos != -1));

  // pages 
  const int pages_obj = 1;
  pdf->xref[0] = pdf->filepos; // now fix it
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "<</Type/Pages\n"
		 "  /Count %d\n"
		 "  /Kids [",
		 pages_obj, pdf->pagessize);
  for (iA = 0; iA < pdf->pagessize; iA ++)
    _cfPDFOutPrintF(pdf, "%d 0 R ", pdf->pages[iA]);
  _cfPDFOutPrintF(pdf,
		 "]\n"
		 ">>\n"
		 "endobj\n");

  // rootdict
  root_obj = _cfPDFOutAddXRef(pdf);
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "<</Type/Catalog\n"
		 "  /Pages %d 0 R\n"
		 ">>\n"
		 "endobj\n",
		 root_obj, pages_obj);

  // info 
  if (pdf->kvsize)
  {
    info_obj = _cfPDFOutAddXRef(pdf);
    _cfPDFOutPrintF(pdf,
		   "%d 0 obj\n"
		   "<<\n",
		   info_obj);
    for (iA = 0; iA < pdf->kvsize; iA++)
    {
      _cfPDFOutPrintF(pdf, "  /%s ", pdf->kv[iA].key);
      _cfPDFOutputString(pdf, pdf->kv[iA].value, -1);
      _cfPDFOutPrintF(pdf, "\n");
    }
    _cfPDFOutPrintF(pdf,
		   ">>\n"
		   "endobj\n");
  }
  // TODO: some return-value checking (??)
 
  // write xref
  xref_start = pdf->filepos;
  _cfPDFOutPrintF(pdf,
		 "xref\n"
		 "%d %d\n"
		 "%010d 65535 f \n",
		 0, pdf->xrefsize + 1, 0);
  for (iA = 0; iA < pdf->xrefsize; iA ++)
    _cfPDFOutPrintF(pdf, "%010ld 00000 n \n",
		   pdf->xref[iA]);
  _cfPDFOutPrintF(pdf,
		 "trailer\n"
		 "<<\n"
		 "  /Size %d\n"
		 "  /Root %d 0 R\n",
		 pdf->xrefsize + 1,
		 root_obj);
  if (info_obj)
    _cfPDFOutPrintF(pdf, "  /Info %d 0 R\n", info_obj);
  _cfPDFOutPrintF(pdf,
		 ">>\n"
		 "startxref\n"
		 "%d\n"
		 "%%%%EOF\n",
		 xref_start);

  // set to done
  pdf->filepos = -1;
  for (iA = 0; iA < pdf->kvsize; iA ++)
  {
    free(pdf->kv[iA].key);
    free(pdf->kv[iA].value);
  }
  pdf->kvsize = 0;
}
// }}}


//
// '_cfPDFOutFree()' - Free memory of a _cf_pdf_out_t structure
//

void
_cfPDFOutFree(_cf_pdf_out_t *pdf) // {{{
{
  if (pdf)
  {
    DEBUG_assert(pdf->kvsize == 0); // otherwise: finish_pdf has not been called

    free(pdf->kv);
    free(pdf->pages);
    free(pdf->xref);
    free(pdf);
  }
}
// }}}


static void
pdf_out_outfn(const char *buf,
	      int len,
	      void *context) // {{{
{
  _cf_pdf_out_t *pdf = (_cf_pdf_out_t *)context;

  if (fwrite(buf, 1, len, stdout) != len)
  {
    perror("Short write");
    DEBUG_assert(0);
    return;
  }
  pdf->filepos += len;
}
// }}}


//
// '_cfPDFOutWriteFont()' - Writes the font emb including descriptor to the PDF 
//                         and returns the object number.
//

int
_cfPDFOutWriteFont(_cf_pdf_out_t *pdf,
		  _cf_fontembed_emb_params_t *emb) // {{{ 
{
  DEBUG_assert(pdf);
  DEBUG_assert(emb);

  _cf_fontembed_emb_pdf_font_descr_t *fdes = _cfFontEmbedEmbPDFFontDescr(emb);
  if (!fdes)
  {
    if (emb->outtype == _CF_FONTEMBED_EMB_FMT_STDFONT)
    { // std-14 font
      const int f_obj = _cfPDFOutAddXRef(pdf);
      char *res = _cfFontEmbedEmbPDFSimpleStdFont(emb);
      if (!res)
        return (0);

      _cfPDFOutPrintF(pdf,
		     "%d 0 obj\n"
		     "%s"
		     "endobj\n",
		     f_obj,
		     res);
      free(res);
      return (f_obj);
    }
    return (0);
  }

  const int ff_obj = _cfPDFOutAddXRef(pdf);
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "<</Length %d 0 R\n",
		 ff_obj,
		 ff_obj + 1);
  if (_cfFontEmbedEmbPDFGetFontFileSubType(emb))
    _cfPDFOutPrintF(pdf, "  /Subtype /%s\n",
		   _cfFontEmbedEmbPDFGetFontFileSubType(emb));
  if (emb->outtype == _CF_FONTEMBED_EMB_FMT_TTF)
    _cfPDFOutPrintF(pdf, "  /Length1 %d 0 R\n",
		   ff_obj + 2);
  else if (emb->outtype == _CF_FONTEMBED_EMB_FMT_T1) // TODO
    _cfPDFOutPrintF(pdf,
		   "  /Length1 ?\n"
		   "  /Length2 ?\n"
		   "  /Length3 ?\n");
  _cfPDFOutPrintF(pdf,
		 ">>\n"
		 "stream\n");
  long streamsize = -pdf->filepos;
  const int outlen = _cfFontEmbedEmbEmbed(emb, pdf_out_outfn, pdf);
  streamsize += pdf->filepos;
  _cfPDFOutPrintF(pdf,"\nendstream\n"
                    "endobj\n");

  const int l0_obj = _cfPDFOutAddXRef(pdf);
  DEBUG_assert(l0_obj == ff_obj + 1);
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "%ld\n"
		 "endobj\n",
		 l0_obj, streamsize);

  if (emb->outtype == _CF_FONTEMBED_EMB_FMT_TTF)
  {
    const int l1_obj = _cfPDFOutAddXRef(pdf);
    DEBUG_assert(l1_obj == ff_obj + 2);
    _cfPDFOutPrintF(pdf,
		   "%d 0 obj\n"
		   "%d\n"
		   "endobj\n",
		   l1_obj, outlen);
  }

  const int fd_obj = _cfPDFOutAddXRef(pdf);
  char *res = _cfFontEmbedEmbPDFSimpleFontDescr(emb, fdes, ff_obj);
  if (!res)
  {
    free(fdes);
    return (0);
  }
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "%s"
		 "endobj\n",
		 fd_obj, res);
  free(res);

  _cf_fontembed_emb_pdf_font_widths_t *fwid = _cfFontEmbedEmbPDFFontWidths(emb);
  if (!fwid)
  {
    free(fdes);
    return (0);
  }
  const int f_obj = _cfPDFOutAddXRef(pdf);
  res = _cfFontEmbedEmbPDFSimpleFont(emb, fdes, fwid, fd_obj);
  if (!res)
  {
    free(fwid);
    free(fdes);
    return (0);
  }
  _cfPDFOutPrintF(pdf,
		 "%d 0 obj\n"
		 "%s"
		 "endobj\n",
		 f_obj,res);
  free(res);
  free(fwid);

  if (emb->plan & _CF_FONTEMBED_EMB_A_MULTIBYTE)
  {
    res = _cfFontEmbedEmbPDFSimpleCIDFont(emb, fdes->fontname, f_obj);
    if (!res)
    {
      free(fdes);
      return (0);
    }
    const int cf_obj = _cfPDFOutAddXRef(pdf);
    _cfPDFOutPrintF(pdf,
		   "%d 0 obj\n"
		   "%s"
		   "endobj\n",
		   cf_obj, res);
    free(res);
    free(fdes);
    return (cf_obj);
  }

  free(fdes);
  return (f_obj);
}
// }}}
