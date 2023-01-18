//
// PDF file output routines for libcupsfilters.
//
// Copyright 2008 by Tobias Hoffmann.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_FILTERS_PDFUTILS_PRIVATE_H_
#  define _CUPS_FILTERS_PDFUTILS_PRIVATE_H_

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Include necessary headers...
//

#include <time.h>
#include <cupsfilters/fontembed-private.h>


//
// Types and structures...
//

struct _cf_keyval_t
{
  char *key, *value;
};

typedef struct
{
  long filepos;

  int pagessize, pagesalloc;
  int *pages;

  int xrefsize, xrefalloc;
  long *xref;

  int kvsize, kvalloc;
  struct _cf_keyval_t *kv;
} _cf_pdf_out_t;


//
// Prototypes...
//

// allocates a new _cf_pdf_out_t structure
// returns NULL on error

_cf_pdf_out_t *_cfPDFOutNew();
void _cfPDFOutFree(_cf_pdf_out_t *pdf);

// start outputting a pdf
// returns false on error

int _cfPDFOutBeginPDF(_cf_pdf_out_t *pdf);
void _cfPDFOutFinishPDF(_cf_pdf_out_t *pdf);

// General output routine for our pdf.
// Keeps track of characters actually written out

void _cfPDFOutPrintF(_cf_pdf_out_t *pdf, const char *fmt, ...)
  __attribute__((format(printf, 2, 3)));

// write out an escaped pdf string: e.g.  (Text \(Test\)\n)
// > len == -1: use strlen(str) 

void _cfPDFOutputString(_cf_pdf_out_t *pdf, const char *str, int len);
void _cfPDFOutputHexString(_cf_pdf_out_t *pdf, const char *str, int len);

// Format the broken up timestamp according to
// pdf requirements for /CreationDate
// NOTE: uses statically allocated buffer 

const char *_cfPDFOutToPDFDate(struct tm *curtm);

// begin a new object at current point of the 
// output stream and add it to the xref table.
// returns the obj number.

int _cfPDFOutAddXRef(_cf_pdf_out_t *pdf);

// adds page dictionary >obj to the global Pages tree
// returns false on error

int _cfPDFOutAddPage(_cf_pdf_out_t *pdf, int obj);

// add a >key,>val pair to the document's Info dictionary
// returns false on error

int _cfPDFOutAddKeyValue(_cf_pdf_out_t *pdf, const char *key, const char *val);

// Writes the font >emb including descriptor to the pdf 
// and returns the object number.
// On error 0 is returned.

int _cfPDFOutWriteFont(_cf_pdf_out_t *pdf,
		      struct _cf_fontembed_emb_params_s *emb);

#  ifdef __cplusplus
}
#  endif // __cplusplus

#endif // !_CUPS_FILTERS_PDFUTILS_PRIVATE_H_
