#include "pdftopdf-private.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef int key_t;
typedef uint16_t data_t;

typedef struct {
    key_t first;
    key_t end;
} value_t;

typedef struct {
    value_t *data;
    size_t size;
    size_t capacity;
} _cfPDFToPDFIntervalSet;



extern const key_t npos;

void _cfPDFToPDFIntervalSet_init(_cfPDFToPDFIntervalSet *set);
void _cfPDFToPDFIntervalSet_clear(_cfPDFToPDFIntervalSet *set);
void _cfPDFToPDFIntervalSet_add(_cfPDFToPDFIntervalSet *set, key_t start, key_t end);
void _cfPDFToPDFIntervalSet_finish(_cfPDFToPDFIntervalSet *set);

size_t _cfPDFToPDFIntervalSet_size(const _cfPDFToPDFIntervalSet *vec) { return vec->size; }

bool _cfPDFToPDFIntervalSet_contains(const _cfPDFToPDFIntervalSet *set, key_t val);
key_t interval_set_next(const _cfPDFToPDFIntervalSet *set, key_t val);
void _cfPDFToPDFIntervalSet_dump(const _cfPDFToPDFIntervalSet *set, pdftopdf_doc_t *doc);
