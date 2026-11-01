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
  int		num_values;	// Number of device ID values
  cups_option_t	*values;	// Device ID values
  const char	*value;		// Device ID value
  int	i;		// Looping var
  static const struct
  {
    const char	*input;	// Make and model string
    const char	*model;	// Expected model name
  } models[] =
  {
    { "Konica Minolta bizhub C258", "bizhub C258" },
    { "Fuji Xerox DocuPrint C3200", "DocuPrint C3200" },
    { "Kyocera Mita FS-1020D", "FS-1020D" },
    { "Lexmark International MS310", "MS310" },
    { "Epson Stylus Photo", "Stylus Photo" },
    { "Brother HL-2270 DW printer", "HL-2270 DW printer" }
  };

  //
  // The last field of a device ID does not need a trailing ';'...
  //

  num_values = cfIEEE1284GetValues("MFG:HP;MDL:DeskJet", &values);
  value = cupsGetOption("MDL", num_values, values);
  if (num_values != 2 || !value || strcmp(value, "DeskJet"))
  {
    printf("FAIL: MFG:HP;MDL:DeskJet expected 2 values with MDL \"DeskJet\", "
	   "got %d with MDL \"%s\"\n", num_values, value ? value : "NULL");
    return (1);
  }
  cupsFreeOptions(num_values, values);

  cfIEEE1284GetMakeModel("MFG:HP;MDL:DeskJet", buffer, sizeof(buffer));
  if (strcmp(buffer, "HP DeskJet"))
  {
    printf("FAIL: MFG:HP;MDL:DeskJet expected make and model \"HP DeskJet\", "
	   "got \"%s\"\n", buffer);
    return (1);
  }

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

  res = cfIEEE1284NormalizeMakeModel("MFG:HP;MDL:;", NULL,
				     CF_IEEE1284_NORMALIZE_HUMAN, NULL,
				     buffer, sizeof(buffer), &model, NULL,
				     NULL);
  if (res != NULL)
  {
    printf("FAIL: MFG:HP;MDL:; expected NULL, got \"%s\"\n", res);
    return (1);
  }

  res = cfIEEE1284NormalizeMakeModel("MFG:;MDL:DeskJet;", NULL,
				     CF_IEEE1284_NORMALIZE_HUMAN, NULL,
				     buffer, sizeof(buffer), &model, NULL,
				     NULL);
  if (res != NULL)
  {
    printf("FAIL: MFG:;MDL:DeskJet; expected NULL, got \"%s\"\n", res);
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

  //
  // Known makes with a space in their name, and ordinary names which only
  // happen to have a space where those names would have one.
  //

  for (i = 0; i < (int)(sizeof(models) / sizeof(models[0])); i ++)
  {
    model = NULL;
    res = cfIEEE1284NormalizeMakeModel(models[i].input, NULL,
				       CF_IEEE1284_NORMALIZE_HUMAN, NULL,
				       buffer, sizeof(buffer), &model, NULL,
				       NULL);
    if (!res || !model || strcmp(model, models[i].model))
    {
      printf("FAIL: \"%s\" expected model \"%s\", got \"%s\"\n",
	     models[i].input, models[i].model, model ? model : "NULL");
      return (1);
    }
  }

  puts("PASS: cfIEEE1284NormalizeMakeModel issue #214 tests passed.");
  return (0);
}
