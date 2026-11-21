//
// Media size and margin regression tests for cfGenerateSizes() (Issue #89).
//
// Copyright © 2026 by OpenPrinting.
//
// Licensed under Apache License v2.0. See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <cupsfilters/ipp.h>
#include <cupsfilters/libcups2-private.h>
#include <string.h>


// cfGenerateSizes() returns the legacy media layout also used by libppd.
typedef struct
{
  char media[128];
  int width, length, bottom, left, right, top;
} media_size_t;


static ipp_t *
make_size(int width, int length)
{
  ipp_t *size = ippNew();

  ippAddInteger(size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "x-dimension", width);
  ippAddInteger(size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "y-dimension", length);
  return (size);
}


static ipp_t *
make_media(int width, int length, int margin)
{
  ipp_t *media = ippNew();
  ipp_t *size = make_size(width, length);

  ippAddCollection(media, IPP_TAG_ZERO, "media-size", size);
  ippDelete(size);
  ippAddInteger(media, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-left-margin", margin);
  ippAddInteger(media, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-bottom-margin", margin);
  ippAddInteger(media, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-right-margin", margin);
  ippAddInteger(media, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-margin", margin);
  return (media);
}


static ipp_t *
make_printer(const char *collection_attr, int size_supported,
             int media_supported, int a4_borderless)
{
  ipp_t *printer = ippNew();
  ipp_t *media[3], *sizes[3];
  int i;
  static const int margins[] = { 0, 500 };
  static const char * const names[] =
  {
    "iso_a4_210x297mm", "na_letter_8.5x11in", "iso_a5_148x210mm"
  };
  static const char * const margin_attrs[] =
  {
    "media-left-margin-supported", "media-bottom-margin-supported",
    "media-right-margin-supported", "media-top-margin-supported"
  };

  for (i = 0; i < 4; i ++)
    ippAddIntegers(printer, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                   margin_attrs[i], 2, margins);

  if (collection_attr)
  {
    media[0] = make_media(21000, 29700, 500);
    media[1] = make_media(21590, 27940, 0);
    media[2] = a4_borderless ? make_media(21000, 29700, 0) : NULL;
    ippAddCollections(printer, IPP_TAG_PRINTER, collection_attr,
                      a4_borderless ? 3 : 2, (const ipp_t **)media);
    for (i = 0; i < (a4_borderless ? 3 : 2); i ++)
      ippDelete(media[i]);
  }

  if (size_supported)
  {
    sizes[0] = make_size(21000, 29700);
    sizes[1] = make_size(21590, 27940);
    sizes[2] = make_size(14800, 21000);
    ippAddCollections(printer, IPP_TAG_PRINTER, "media-size-supported", 3,
                      (const ipp_t **)sizes);
    for (i = 0; i < 3; i ++)
      ippDelete(sizes[i]);
  }

  if (media_supported)
    ippAddStrings(printer, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                  "media-supported", 3, NULL, names);

  return (printer);
}


static int
check_sizes(const char *collection_attr, int size_supported,
            int media_supported, int a4_borderless)
{
  ipp_t *printer = make_printer(collection_attr, size_supported,
                                media_supported, a4_borderless);
  cups_array_t *sizes = NULL;
  media_size_t value, *size = &value;
  void *entry;
  int failed = 0, seen_a4 = 0, seen_borderless = 0;
  int seen_letter = 0, seen_a5 = 0;

  cfGenerateSizes(printer, CF_GEN_SIZES_DEFAULT, &sizes, NULL, NULL, NULL,
                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                   NULL, NULL, NULL, NULL, NULL, NULL);

  if (cupsArrayGetCount(sizes) != (a4_borderless ? 4 : 3))
  {
    fprintf(stderr, "%s: expected %d sizes, got %d\n", collection_attr,
            a4_borderless ? 4 : 3, (int)cupsArrayGetCount(sizes));
    failed = 1;
  }

  for (entry = cupsArrayGetFirst(sizes); entry; entry = cupsArrayGetNext(sizes))
  {
    memcpy(size, entry, sizeof(*size));
    size_t name_len = strcspn(size->media, " ");

    if (name_len == 2 && !strncmp(size->media, "A4", 2))
    {
      seen_a4 ++;
      if (size->left != 500 || size->bottom != 500 ||
          size->right != 500 || size->top != 500)
        failed = 1;
      if (media_supported && !strstr(size->media, " iso_a4_210x297mm"))
        failed = 1;
    }
    else if (name_len == 13 && !strncmp(size->media, "A4.Borderless", 13))
    {
      seen_borderless ++;
      if (!a4_borderless || size->left || size->bottom ||
          size->right || size->top)
        failed = 1;
      if (media_supported && !strstr(size->media, " iso_a4_210x297mm"))
        failed = 1;
    }
    else if (name_len == 17 && !strncmp(size->media, "Letter.Borderless", 17))
    {
      seen_letter ++;
      if (size->left || size->bottom || size->right || size->top)
        failed = 1;
    }
    else if (name_len == 13 && !strncmp(size->media, "A5.Borderless", 13))
    {
      seen_a5 ++;
      if (size->left || size->bottom || size->right || size->top)
        failed = 1;
    }
    else
      failed = 1;
  }

  if (seen_a4 != 1 || seen_borderless != a4_borderless ||
      seen_letter != 1 || seen_a5 != 1)
    failed = 1;

  printf("%s, size-supported=%d, media-supported=%d, A4-borderless=%d: %s\n",
         collection_attr, size_supported, media_supported, a4_borderless,
         failed ? "FAIL" : "PASS");
  cupsArrayDelete(sizes);
  ippDelete(printer);
  return (failed);
}


static int
check_search(const char *collection_attr, int a4_borderless)
{
  ipp_t *printer = make_printer(collection_attr, 1, 1, a4_borderless);
  char name[IPP_MAX_NAME] = "A4.Borderless";
  int width = 21000, length = 29700;
  int left = 0, bottom = 0, right = 0, top = 0;
  int failed;

  cfGenerateSizes(printer, CF_GEN_SIZES_SEARCH_BORDERLESS_ONLY, NULL, NULL,
                   &width, &length, &left, &bottom, &right, &top,
                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, name, NULL);
  failed = a4_borderless ? (width != 21000 || length != 29700 ||
                             left || bottom || right || top) :
                            (width != 0 || length != 0);
  printf("%s, borderless search, A4-borderless=%d: %s\n", collection_attr,
         a4_borderless, failed ? "FAIL" : "PASS");
  ippDelete(printer);
  return (failed);
}


static int
check_fallback(void)
{
  ipp_t *printer = make_printer(NULL, 1, 1, 0);
  cups_array_t *sizes = NULL;
  media_size_t value, *size = &value;
  void *entry;
  int failed = 0;

  cfGenerateSizes(printer, CF_GEN_SIZES_DEFAULT, &sizes, NULL, NULL, NULL,
                   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                   NULL, NULL, NULL, NULL, NULL, NULL);
  if (cupsArrayGetCount(sizes) != 3)
    failed = 1;
  for (entry = cupsArrayGetFirst(sizes); entry; entry = cupsArrayGetNext(sizes))
  {
    memcpy(size, entry, sizeof(*size));
    if (!strstr(size->media, ".Borderless") || size->left || size->bottom ||
        size->right || size->top)
      failed = 1;
  }

  printf("No media collections, printer-wide margins: %s\n",
         failed ? "FAIL" : "PASS");
  cupsArrayDelete(sizes);
  ippDelete(printer);
  return (failed);
}


int
main(void)
{
  int failed = 0, i, fallback, borderless;
  static const char * const collections[] =
  {
    "media-col-database", "media-col-ready"
  };

  for (i = 0; i < 2; i ++)
  {
    for (borderless = 0; borderless < 2; borderless ++)
    {
      for (fallback = 1; fallback <= 3; fallback ++)
        failed |= check_sizes(collections[i], fallback & 1,
                               fallback & 2, borderless);
      failed |= check_search(collections[i], borderless);
    }
  }

  failed |= check_fallback();
  return (failed);
}
