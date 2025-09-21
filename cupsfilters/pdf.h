//
// Copyright 2012 Canonical Ltd.
// Copyright 2018 Sahil Arora <sahilarora.535@gmail.com>
// Copyright 2024-2025 Uddhav Phatak <uddhavphatak@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_FILTERS_PDF_H_
#define _CUPS_FILTERS_PDF_H_

#include <stdio.h>

typedef struct pdfio_file_t cf_pdf_t;

typedef struct _cf_opt cf_opt_t;

//
// Type to bunch PDF form field name and its value.
//

struct _cf_opt
{
  const char* key;
  const char* val;
  cf_opt_t *next;
};

cf_pdf_t *cfPDFLoadTemplate(const char *filename);
void cfPDFFree(cf_pdf_t *pdf);

void cfPDFWrite(cf_pdf_t *doc, 
		FILE *file);

int cfPDFPrependStream(cf_pdf_t *doc, 
		       unsigned page, 
		       const char *buf, 
		       size_t len);

int cfPDFAddType1Font(cf_pdf_t *doc, 
		      unsigned page, 
		      const char *name);

int cfPDFResizePage(cf_pdf_t *doc, 
		    unsigned page, 
		    float width, 
		    float length, 
		    float *scale);

int cfPDFDuplicatePage(cf_pdf_t *doc, 
		       unsigned page, 
		       unsigned count);

int cfPDFFillForm(cf_pdf_t *doc, 
		  cf_opt_t *opt);

int cfPDFPages(const char *filename);

int cfPDFPagesFP(char *file);

#endif // !_CUPS_FILTERS_PDF_H_

