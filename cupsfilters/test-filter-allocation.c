//
// Regression tests for filter setup allocation failures.
//
// Copyright 2026 by Ronit Mehta.
//
// Licensed under Apache License v2.0. See the file "LICENSE" for more
// information.
//

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#include "config.h"
#include <cupsfilters/filter.h>
#include <cupsfilters/libcups2-private.h>
#include <cups/file.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int allocations, fail_at, reject_add, forks;

static void *
checked_malloc(size_t size)
{
  if (++allocations == fail_at)
  {
    errno = ENOMEM;
    return (NULL);
  }
  return (malloc(size));
}

static int
checked_array_add(cups_array_t *array, void *element)
{
  return (reject_add ? 0 : cupsArrayAdd(array, element));
}

static pid_t
checked_fork(void)
{
  forks++;
  return (fork());
}

/* Inject failures into the production allocation and process paths. */
#define malloc checked_malloc
#define cupsArrayAdd checked_array_add
#define fork checked_fork
#include "filter.c"
#include "universal.c"
#undef malloc
#undef cupsArrayAdd
#undef fork

static void
delete_chain(cups_array_t *chain)
{
  cf_filter_filter_in_chain_t *filter;

  while ((filter = cupsArrayGetFirst(chain)) != NULL)
  {
    cupsArrayRemove(chain, filter);
    free(filter->parameters);
    free(filter);
  }
  cupsArrayDelete(chain);
}

static int
test_entry(void)
{
  cups_array_t *chain = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);
  cf_filter_out_format_t format = CF_FILTER_OUT_FORMAT_PCLM;
  cf_filter_filter_in_chain_t *filter;

  if (!chain)
    return (1);
  for (fail_at = 1; fail_at <= 2; fail_at++)
  {
    allocations = 0;
    filter = add_filter(chain, cfFilterPWGToPDF, &format, sizeof(format), "pclm");
    if (filter || cupsArrayGetCount(chain))
    {
      delete_chain(chain);
      return (1);
    }
  }
  fail_at = 0;
  reject_add = 1;
  filter = add_filter(chain, cfFilterPWGToPDF, &format, sizeof(format), "pclm");
  reject_add = 0;
  if (filter || cupsArrayGetCount(chain))
  {
    delete_chain(chain);
    return (1);
  }
  filter = add_filter(chain, cfFilterPWGToPDF, &format, sizeof(format), "pclm");
  if (!filter || filter->parameters == &format ||
      *(cf_filter_out_format_t *)filter->parameters != format)
  {
    delete_chain(chain);
    return (1);
  }
  delete_chain(chain);
  return (0);
}

static int
test_universal(void)
{
  cf_filter_data_t data = { 0 };

  data.content_type = "image/png";
  data.final_content_type = "application/PCLm";
  for (fail_at = 1; fail_at <= 4; fail_at++)
  {
    int inputfd = open("/dev/null", O_RDONLY);
    int outputfd = open("/dev/null", O_WRONLY);

    if (inputfd < 0 || outputfd < 0)
    {
      if (inputfd >= 0)
        close(inputfd);
      if (outputfd >= 0)
        close(outputfd);
      return (1);
    }
    allocations = forks = 0;
    if (!cfFilterUniversal(inputfd, outputfd, 0, &data, NULL) || forks)
      return (1);
    if (fcntl(inputfd, F_GETFD) >= 0 || errno != EBADF ||
        fcntl(outputfd, F_GETFD) >= 0 || errno != EBADF)
      return (1);
  }
  fail_at = 0;
  return (0);
}

static int
noop_filter(int inputfd, int outputfd, int seekable,
            cf_filter_data_t *data, void *parameters)
{
  (void)inputfd;
  (void)outputfd;
  (void)seekable;
  (void)data;
  (void)parameters;
  return (0);
}

static int
test_pids(int failure, int fail_add)
{
  cups_array_t *chain = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);
  cf_filter_filter_in_chain_t filters[2] = {
    { noop_filter, NULL, "first" }, { noop_filter, NULL, "second" }
  };
  cf_filter_data_t data = { 0 };
  int inputfd, outputfd, status, result;

  if (!chain)
    return (1);
  if (!cupsArrayAdd(chain, &filters[0]) || !cupsArrayAdd(chain, &filters[1]))
  {
    cupsArrayDelete(chain);
    return (1);
  }
  inputfd = open("/dev/null", O_RDONLY);
  outputfd = open("/dev/null", O_WRONLY);
  if (inputfd < 0 || outputfd < 0)
  {
    if (inputfd >= 0)
      close(inputfd);
    if (outputfd >= 0)
      close(outputfd);
    cupsArrayDelete(chain);
    return (1);
  }
  allocations = forks = 0;
  fail_at = failure;
  reject_add = fail_add;
  result = cfFilterChain(inputfd, outputfd, 0, &data, chain);
  fail_at = reject_add = 0;
  cupsArrayDelete(chain);
  if (!result || forks != (failure == 1 ? 0 : 1))
    return (1);
  if (fcntl(inputfd, F_GETFD) >= 0 || errno != EBADF)
    return (1);
  if (fcntl(outputfd, F_GETFD) >= 0 || errno != EBADF)
    return (1);
  if (waitpid(-1, &status, WNOHANG) != -1 || errno != ECHILD)
    return (1);
  return (0);
}

int
main(void)
{
  if (test_entry() || test_universal() || test_pids(1, 0) ||
      test_pids(2, 0) || test_pids(0, 1))
  {
    fputs("Filter allocation failure test failed\n", stderr);
    return (1);
  }
  return (0);
}
