//
// Copyright 2024 Uddhav Phatak <uddhavabhijeet@gmail.com>
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "C-intervalset-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include "cupsfilters/debug-internal.h"

const int _cfPDFToPDFIntervalSet_npos = INT_MAX;

void 
_cfPDFToPDFIntervalSet_init(_cfPDFToPDFIntervalSet *set) // {{{
{
  set->data = NULL;
  set->size = 0;
  set->capacity = 0;
}
// }}}

void 
_cfPDFToPDFIntervalSet_clear(_cfPDFToPDFIntervalSet *set) // {{{
{
  free(set->data);
  set->data = NULL;
  set->size = 0;
  set->capacity = 0;
}
// }}}

void 
_cfPDFToPDFIntervalSet_add(_cfPDFToPDFIntervalSet *set, int start, int end) // {{{
{
  if (start >= end) 
    return;

  if (set->size == set->capacity) 
  {
    set->capacity = set->capacity == 0 ? 4 : set->capacity * 2;
    set->data = realloc(set->data, set->capacity * sizeof(interval_t));
  }

  set->data[set->size].start = start;
  set->data[set->size].end = end;
  set->size++;
}
// }}}

void 
_cfPDFToPDFIntervalSet_add_single(_cfPDFToPDFIntervalSet *set, 
				  int start) // {{{
{
   key_t end = _cfPDFToPDFIntervalSet_npos;

  if (start >= end)
    return;

  if (set->size == set->capacity) 
  {
    set->capacity = set->capacity == 0 ? 4 : set->capacity * 2;
    set->data = realloc(set->data, set->capacity * sizeof(interval_t));
    if (set->data == NULL)
      return;
  }

  set->data[set->size].start = start;
  set->data[set->size].end = end;
  set->size++;
}
// }}}

static int 
compare_intervals(const void *a, const void *b) // {{{
{
  interval_t *ia = (interval_t *)a;
  interval_t *ib = (interval_t *)b;
  if (ia->start != ib->start)
    return ia->start - ib->start;
  return ia->end - ib->end;
}
// }}}

void 
_cfPDFToPDFIntervalSet_finish(_cfPDFToPDFIntervalSet *set) // {{{
{
  if (set->size == 0) 
    return;

  qsort(set->data, set->size, sizeof(interval_t), compare_intervals);

  size_t new_size = 0;
  for (size_t i = 1; i < set->size; i++) 
  {
    if (set->data[new_size].end >= set->data[i].start) 
    {
      if (set->data[new_size].end < set->data[i].end) 
      {
        set->data[new_size].end = set->data[i].end;
      }
    } 
    else 
    {
      new_size++;
      set->data[new_size] = set->data[i];
    }
  }
  set->size = new_size + 1;
}
// }}}}

size_t 
_cfPDFToPDFIntervalSet_size(const _cfPDFToPDFIntervalSet *set) // {{{
{
  return set->size;
}
// }}}

bool 
_cfPDFToPDFIntervalSet_contains(const _cfPDFToPDFIntervalSet *set, 
				int val) // {{{
{
  for (size_t i = 0; i < set->size; i++) 
  {
    if (val >= set->data[i].start && val < set->data[i].end) 
    {
      return true;
    }
  }
  return false;
}
// }}}

int 
_cfPDFToPDFIntervalSet_next(const _cfPDFToPDFIntervalSet *set, 
			    int val) // {{{
{
  val++;
  for (size_t i = 0; i < set->size; i++) 
  {
    if (val < set->data[i].end) 
    {
      if (val >= set->data[i].start) 
      {
        return val;
      } 
      else 
      {
        return set->data[i].start;
      }
    }
  }
  return _cfPDFToPDFIntervalSet_npos;
}
// }}}

void 
_cfPDFToPDFIntervalSet_dump(const _cfPDFToPDFIntervalSet *set, 
			    pdftopdf_doc_t *doc) // {{{
{
  if (set->size == 0) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: (empty)");
    return;
  }

  for (size_t i = 0; i < set->size; i++) 
  {
    if (doc->logfunc) doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                                   "cfFilterPDFToPDF: [%d,%d)",
                                   set->data[i].start, set->data[i].end);
  }
}
// }}}
