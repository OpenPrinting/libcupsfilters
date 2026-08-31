//
// IEEE-1284 normalization regression test for libcupsfilters (Issue #214).
//
// Copyright © 2026 by OpenPrinting.
//
// Licensed under Apache License v2.0. See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <cupsfilters/ieee1284.h>
#include <stdio.h>
#include <string.h>

int
main(void)
{
  char	buffer[1024];	// Make/model buffer
  char	*model = NULL;	// Pointer to model name
  char	*res;		// Result pointer

  //
  // Issue #214 regression test: empty MFG and MDL fields in IEEE-1284 device ID
  //

  res = cfIEEE1284NormalizeMakeModel("MFG:;MDL:;", NULL,
				     CF_IEEE1284_NORMALIZE_HUMAN, NULL,
				     buffer, sizeof(buffer), &model, NULL,
				     NULL);
  if (res != NULL)
  {
    printf("FAIL: MFG:;MDL:; expected NULL, got \"%s\"\n", res);
    return (1);
  }

  res = cfIEEE1284NormalizeMakeModel("MFG:;MDL:;CMD:PostScript;", NULL,
				     CF_IEEE1284_NORMALIZE_HUMAN, NULL,
				     buffer, sizeof(buffer), &model, NULL,
				     NULL);
  if (res != NULL)
  {
    printf("FAIL: MFG:;MDL:;CMD:PostScript; expected NULL, got \"%s\"\n", res);
    return (1);
  }

  //
  // Verify valid inputs behavior preservation
  //

  res = cfIEEE1284NormalizeMakeModel("MDL:;", NULL,
				     CF_IEEE1284_NORMALIZE_HUMAN, NULL,
				     buffer, sizeof(buffer), &model, NULL,
				     NULL);
  if (!res || strcmp(res, "MDL:;"))
  {
    printf("FAIL: MDL:; expected \"MDL:;\", got \"%s\"\n", res ? res : "NULL");
    return (1);
  }

  res = cfIEEE1284NormalizeMakeModel("MFG:HP;MDL:DeskJet;", NULL,
				     CF_IEEE1284_NORMALIZE_HUMAN, NULL,
				     buffer, sizeof(buffer), &model, NULL,
				     NULL);
  if (!res || strcmp(res, "HP DeskJet"))
  {
    printf("FAIL: MFG:HP;MDL:DeskJet; expected \"HP DeskJet\", got \"%s\"\n",
	   res ? res : "NULL");
    return (1);
  }

  puts("PASS: cfIEEE1284NormalizeMakeModel issue #214 tests passed.");
  return (0);
}
