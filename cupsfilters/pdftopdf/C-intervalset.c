#include "C-intervalset-private.h"
#include <stdio.h>
#include "cupsfilters/debug-internal.h"
#include <limits.h>
#include <stdlib.h>

const key_t npos = INT_MAX;

void 
_cfPDFToPDFIntervalSet_init(_cfPDFToPDFIntervalSet *set)
{
  set->data = NULL;
  set->size = 0;
  set->capacity = 0;
}

void 
_cfPDFToPDFIntervalSet_clear(_cfPDFToPDFIntervalSet *set)
{
  free(set->data);
  set->data = NULL;
  set->size = 0;
  set->capacity = 0;

}

void 
_cfPDFToPDFIntervalSet_add(_cfPDFToPDFIntervalSet *set,
			   key_t start, 
			   key_t end)
{
  if (start < end) 
  {
    if (set->size == set->capacity) 
    {
      set->capacity = set->capacity ? set->capacity * 2 : 1;
      set->data = (value_t *)realloc(set->data, set->capacity * sizeof(value_t));
    }

    set->data[set->size].first = start;
    set->data[set->size].end = end;
    set->size++;
  }
}

//helper for qsort
static int compare_intervals(const void *a, const void *b) {
    const value_t *interval_a = (const value_t *)a;
    const value_t *interval_b = (const value_t *)b;
    return (interval_a->first - interval_b->first);
}

void 
_cfPDFToPDFIntervalSet_finish(_cfPDFToPDFIntervalSet *set)
{
  if (set->size == 0)
    return;

  // Sort the intervals
  qsort(set->data, set->size, sizeof(_cfPDFToPDFIntervalSet), compare_intervals);

  size_t pos = 0;
  for (size_t i = 1; i < set->size; ++i) 
  {
    if (set->data[pos].end >= set->data[i].first) 
    {
      if (set->data[pos].end < set->data[i].end) 
        set->data[pos].end = set->data[i].end;
    } 
   
    else 
    {
      ++pos;
      if (pos != i) 
        set->data[pos] = set->data[i];
    }
  }
  set->size = pos + 1;
}

bool 
_cfPDFToPDFIntervalSet_contains(const _cfPDFToPDFIntervalSet *set, 
				key_t val)
{
  for (size_t i = 0; i < set->size; ++i) 
  {
    if (val >= set->data[i].first && val < set->data[i].end) 
      return true;
  }
  return false;
}

key_t 
_cfPDFToPDFIntervalSet_next(const _cfPDFToPDFIntervalSet *set, 
			    key_t val) 
{
  val++;
  value_t key = {val, npos};
  value_t *it = (value_t *)bsearch(&key, set->data, set->size, 
		  		   sizeof(value_t), compare_intervals);

  if (it == NULL) 
  {
    for (size_t i = 0; i < set->size; ++i) 
    {
      if (set->data[i].first > val) 
        return set->data[i].first;
        
      return npos;
    }

    if (it == set->data) 
    {
      if (it->first > val) 
        return it->first;
        
      return npos;
    }

    --it;
    if (val < it->end)
      return val;
    

    ++it;
    if (it == set->data + set->size) 
        return npos;
    
    return it->first;
  }
}

void
_cfPDFToPDFIntervalSet_dump(const _cfPDFToPDFIntervalSet *set,
                            pdftopdf_doc_t *doc)
{
  int len = set->size;
  if (len == 0)
  {
    if (doc->logfunc)
      doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                   "cfFilterPDFToPDF: (empty)");
    return;
  }

  len--;

  for (int iA = 0; iA < len; iA++)
  {
    if (doc->logfunc)
      doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                   "cfFilterPDFToPDF: [%d,%d)",
                   set->data[iA].first, set->data[iA].end);
  }

  if (set->data[len].end == npos)
  {
    if (doc->logfunc)
      doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                   "cfFilterPDFToPDF: [%d,inf)",
                   set->data[len].first);
  }
  else
  {
    if (doc->logfunc)
      doc->logfunc(doc->logdata, CF_LOGLEVEL_DEBUG,
                   "cfFilterPDFToPDF: [%d,%d)",
                   set->data[len].first, set->data[len].end);
  }
}
