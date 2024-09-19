//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_FILTERS_PDFTOPDF_INTERVALSET_H_
#define _CUPS_FILTERS_PDFTOPDF_INTERVALSET_H_

#include "C-pdftopdf-private.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct
{
  int start;
  int end;
} interval_t;

typedef struct
{
  interval_t *data;
  size_t size;
  size_t capacity;
} _cfPDFToPDFIntervalSet;

extern const int _cfPDFToPDFIntervalSet_npos;

void _cfPDFToPDFIntervalSet_init(_cfPDFToPDFIntervalSet *set);
void _cfPDFToPDFIntervalSet_clear(_cfPDFToPDFIntervalSet *set);
void _cfPDFToPDFIntervalSet_add(_cfPDFToPDFIntervalSet *set, int start, int end);
void _cfPDFToPDFIntervalSet_add_single(_cfPDFToPDFIntervalSet *set, int start);
void _cfPDFToPDFIntervalSet_finish(_cfPDFToPDFIntervalSet *set);

size_t _cfPDFToPDFIntervalSet_size(const _cfPDFToPDFIntervalSet *set);

bool _cfPDFToPDFIntervalSet_contains(const _cfPDFToPDFIntervalSet *set, int val);
int _cfPDFToPDFIntervalSet_next(const _cfPDFToPDFIntervalSet *set, int val);

void _cfPDFToPDFIntervalSet_dump(const _cfPDFToPDFIntervalSet *set, pdftopdf_doc_t *doc);

#endif // !_CUPS_FILTERS_PDFTOPDF_INTERVALSET_H_

