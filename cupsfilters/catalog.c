//
// IPP attribute/option string catalog manager for libcupsfilters.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers.
//

#include <config.h>

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cups/cups.h>
#include <cups/dir.h>
#include <cups/pwg.h>
#include <cupsfilters/catalog.h>
#include <cupsfilters/libcups2-private.h>

//
// 'cfGetURI()' - Get a file from the given URI and save it to a temporary file.
//

int				                     	// O  - 1 on success, 0 on failure
cfGetURI(const char *url,	    	// I  - URL to get
	 char       *name,	        	// I  - Temporary filename
	 size_t     namesize)		      // I  - Size of temporary filename buffer
{
  http_t		*http = NULL;
  char			scheme[32],	// URL scheme
			userpass[256],	// URL username:password
			host[256],	// URL host
			resource[256];	// URL resource
  int			port;		// URL port
  http_encryption_t	encryption;	// Type of encryption to use
  http_status_t		status;		// Status of GET request
  int			fd;		// Temporary file


  if (httpSeparateURI(HTTP_URI_CODING_ALL, url, scheme, sizeof(scheme),
		      userpass, sizeof(userpass), host, sizeof(host), &port,
		      resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
    return (0);

  if (port == 443 || !strcmp(scheme, "https"))
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

  http = httpConnect(host, port, NULL, AF_UNSPEC, encryption, 1, 5000, NULL);

  if (!http)
    return (0);

  if ((fd = cupsCreateTempFd(NULL, NULL, name, (int)namesize)) < 0)
    return (0);

  status = cupsGetFd(http, resource, fd);

  close(fd);
  httpClose(http);

  if (status != HTTP_STATUS_OK)
  {
    unlink(name);
    *name = '\0';
    return (0);
  }

  return (1);
}

//
// 'cfCatalogSearchDirLocale()' - Search a directory for a CUPS message catalog file 
//                                  matching the given locale.
//

char *                                              // O - Catalog file path, or NULL if not found
cfCatalogSearchDirLocale(const char *dirname,       // I - Directory name
  const char *locale)                               // I - Locale name 
{
  char *catalog = NULL;
  char catalogpath[2048];

  if (!dirname || !locale)
    return (NULL);

  snprintf(catalogpath, sizeof(catalogpath),
            "%s/%s/cups_%s.po", dirname, locale, locale);
  if (access(catalogpath, R_OK) == 0)
    // found
    catalog = strdup(catalogpath);

  return (catalog);
}

//
// 'cfCatalogSearchDirLang()' - Search a directory for a CUPS message catalog file
//                              matching the given language.
//

char *                                                // O - Catalog file path, or NULL if not found
cfCatalogSearchDirLang(const char *dirname,           // I - Directory name
   const char *lang)                                  // I - Language name
{
  size_t lang_len;
  const char *c1, *c2;
  char *catalog = NULL;
  cups_dir_t *dir = NULL, *subdir;
  cups_dentry_t *subdirentry, *catalogentry;
  char subdirpath[1024], catalogpath[2048];

  if (!dirname || !lang)
    return (NULL);
  
  if ((dir = cupsDirOpen(dirname)) == NULL)
    return (NULL);
  
  lang_len = strlen(lang);
  while ((subdirentry = cupsDirRead(dir)) != NULL)
  {
    // Do we actually have a subdir?
    if (!S_ISDIR(subdirentry->fileinfo.st_mode))
      continue;
    // Check for language prefix match
    c1 = subdirentry->filename;
    if ((strncmp(c1, lang, lang_len) != 0) ||
        (c1[lang_len] != '_' && c1[lang_len] != '\0'))
      continue;
    snprintf(subdirpath, sizeof(subdirpath), "%s/%s", dirname, c1);
    if ((subdir = cupsDirOpen(subdirpath)) != NULL)
    {
      while ((catalogentry = cupsDirRead(subdir)) != NULL)
      {
        // Do we actually have a regular file?
        if (!S_ISREG(catalogentry->fileinfo.st_mode))
          continue;
        // Check format of catalog name
        c2 = catalogentry->filename;
        if (strlen(c2) < 10 || strncmp(c2, "cups_", 5) != 0 ||
            strncmp(c2 + 5, lang, lang_len) != 0 ||
            strcmp(c2 + strlen(c2) - 3, ".po"))
          continue;
        // Is catalog readable ?
        snprintf(catalogpath, sizeof(catalogpath), "%s/%s", subdirpath, c2);
        if (access(catalogpath, R_OK) != 0)
          continue;
        // Found
        catalog = strdup(catalogpath);
        break;
      }
      cupsDirClose(subdir);
      subdir = NULL;
      if (catalog != NULL)
        break;
    }
  }

  cupsDirClose(dir);
  return (catalog);
}

//
// 'cfCatalogSearchDir()' - Search a directory for a CUPS message catalog file matching
//                          the preferred locale.

char *                                        // O - Catalog file path, or NULL if not found  
cfCatalogSearchDir(const char *dirname,       // I - Directory name
  const char *preferredlocale)                // I - Preferred locale name
{
  const char *c1, *c2;
  char *catalog = NULL;
  cups_dir_t *dir = NULL, *subdir;
  cups_dentry_t *subdirentry, *catalogentry;
  char subdirpath[1024], catalogpath[2048], lang[8];
  int i;

  if (dirname == NULL)
    return (NULL);

  if (preferredlocale)
  {
    // Check first for an exact match
    catalog = cfCatalogSearchDirLocale(dirname, preferredlocale);
    if (catalog != NULL)
      return (catalog);

    // Check for language match, with any region
    // Cover both cases, whether locale has region suffix or not
    if ((i = strcspn(preferredlocale, "_")) >= sizeof(lang))
      i = sizeof(lang) - 1;
    strncpy(lang, preferredlocale, i);
    lang[i] = '\0';
    catalog = cfCatalogSearchDirLang(dirname, lang);
    if (catalog != NULL)
      return (catalog);
  
    // If still not found, default to en_US, en_GB, en_... respectively
    // Finally, default to any catalog file found
  }

  catalog = cfCatalogSearchDirLocale(dirname, "en_US");
  if (catalog)
    return (catalog);
  catalog = cfCatalogSearchDirLocale(dirname, "en_GB");
  if (catalog)
    return (catalog);
  catalog = cfCatalogSearchDirLang(dirname, "en");
  if (catalog)
    return (catalog);
  

  if ((dir = cupsDirOpen(dirname)) == NULL)
    return (NULL);

  while ((subdirentry = cupsDirRead(dir)) != NULL)
  {
    // Do we actually have a subdir?
    if (!S_ISDIR(subdirentry->fileinfo.st_mode))
      continue;
    // Check format of subdir name
    c1 = subdirentry->filename;
    if (c1[0] < 'a' || c1[0] > 'z' || c1[1] < 'a' || c1[1] > 'z')
      continue;
    if (c1[2] >= 'a' && c1[2] <= 'z')
      i = 3;
    else
      i = 2;
    if (c1[i] == '_')
    {
      i ++;
      if (c1[i] < 'A' || c1[i] > 'Z' || c1[i + 1] < 'A' || c1[i + 1] > 'Z')
	continue;
      i += 2;
      if (c1[i] >= 'A' && c1[i] <= 'Z')
	i ++;
    }
    if (c1[i] != '\0' && c1[i] != '@')
      continue;
    strncpy(lang, c1, i);
    lang[i] = '\0';
    snprintf(subdirpath, sizeof(subdirpath), "%s/%s", dirname, c1);
    if ((subdir = cupsDirOpen(subdirpath)) != NULL)
    {
      while ((catalogentry = cupsDirRead(subdir)) != NULL)
      {
	// Do we actually have a regular file?
	if (!S_ISREG(catalogentry->fileinfo.st_mode))
	  continue;
	// Check format of catalog name
	c2 = catalogentry->filename;
	if (strlen(c2) < 10 || strncmp(c2, "cups_", 5) != 0 ||
	    strncmp(c2 + 5, lang, i) != 0 ||
	    strcmp(c2 + strlen(c2) - 3, ".po"))
	  continue;
	// Is catalog readable ?
	snprintf(catalogpath, sizeof(catalogpath), "%s/%s", subdirpath, c2);
	if (access(catalogpath, R_OK) != 0)
	  continue;
	// Found
	catalog = strdup(catalogpath);
	break;
      }
      cupsDirClose(subdir);
      subdir = NULL;
      if (catalog != NULL)
	break;
    }
  }

  cupsDirClose(dir);
  return (catalog);
}

//
// 'cfCatalogFind()' - Find a CUPS message catalog file containing
//                     human-readable standard option and choice names
//                     for IPP printers.
//

char *                                     // O - Catalog file path, or NULL if not found   
cfCatalogFind(const char *preferreddir,    // I - Preferred directory 
   const char *preferredlocale)            // I - Preferred locale name
{
  const char *c;
  char buf[1024];
  char *catalog = NULL;

  // Directory supplied by calling program, from config file,
  // environment variable, ...
  if ((catalog = cfCatalogSearchDir(preferreddir, preferredlocale)) != NULL)
    goto found;

  // Directory supplied by environment variable CUPS_LOCALEDIR
  if ((catalog = cfCatalogSearchDir(getenv("CUPS_LOCALEDIR"), 
                                    preferredlocale)) != NULL)
    goto found;

  // Determine CUPS datadir (usually /usr/share/cups)
  if ((c = getenv("CUPS_DATADIR")) == NULL)
    c = CUPS_DATADIR;

  // Search /usr/share/cups/locale/ (location which
  // Debian/Ubuntu package of CUPS is using)
  snprintf(buf, sizeof(buf), "%s/locale", c);
  if ((catalog = cfCatalogSearchDir(buf, preferredlocale)) != NULL)
    goto found;

  // Search /usr/(local/)share/locale/ (standard location
  // which CUPS is using on Linux)
  snprintf(buf, sizeof(buf), "%s/../locale", c);
  if ((catalog = cfCatalogSearchDir(buf, preferredlocale)) != NULL)
    goto found;

  // Search /usr/(local/)lib/locale/ (standard location
  // which CUPS is using on many non-Linux systems)
  snprintf(buf, sizeof(buf), "%s/../../lib/locale", c);
  if ((catalog = cfCatalogSearchDir(buf, preferredlocale)) != NULL)
    goto found;

 found:
  return (catalog);
}


static int
compare_choices(void *a,
		void *b,
		void *user_data)
{
  return (strcasecmp(((catalog_choice_strings_t *)a)->name,
		     ((catalog_choice_strings_t *)b)->name));
}


static int
compare_options(void *a,
		void *b,
		void *user_data)
{
  return (strcasecmp(((catalog_opt_strings_t *)a)->name,
		     ((catalog_opt_strings_t *)b)->name));
}

//
// 'cfCatalogFreeChoiceStrings()' - Free a choice strings entry containing the
//                                   localized human-readable choice name.
//

void                                            
cfCatalogFreeChoiceStrings(void* entry,          // I - Choice strings entry
			   void* user_data)                       // I - User data (unused)
{
  catalog_choice_strings_t *entry_rec = (catalog_choice_strings_t *)entry;

  if (entry_rec)
  {
    if (entry_rec->name) free(entry_rec->name);
    if (entry_rec->human_readable) free(entry_rec->human_readable);
    free(entry_rec);
  }
}

//
// 'cfCatalogFreeOptionStrings()' - Free memory allocated for an option strings
//                                  entry in the catalog.
//

void
cfCatalogFreeOptionStrings(void* entry,    // I - Option strings entry
			   void* user_data)                  // I - User data (unused)  
{
  catalog_opt_strings_t *entry_rec = (catalog_opt_strings_t *)entry;

  if (entry_rec)
  {
    if (entry_rec->name) free(entry_rec->name);
    if (entry_rec->human_readable) free(entry_rec->human_readable);
    if (entry_rec->choices) cupsArrayDelete(entry_rec->choices);
    free(entry_rec);
  }
}

//
// 'cfCatalogOptionArrayNew()' - Create a new array to store catalog option string entries.
//

cups_array_t *              // O - New array
cfCatalogOptionArrayNew()
{
  return (cupsArrayNew(compare_options, NULL, NULL, 0,
			NULL, cfCatalogFreeOptionStrings));
}

//
// 'cfCatalogFindOption()' - Find a catalog option strings entry by name.
//

catalog_opt_strings_t *                      // O - Option strings entry, or NULL if not found
cfCatalogFindOption(cups_array_t *options,   // I - Catalog choice array
		    char *name)                          // I - Option name
{
  catalog_opt_strings_t opt;

  if (!name || !options)
    return (NULL);

  opt.name = name;
  return (cupsArrayFind(options, &opt));
}

//
// 'cfCatalogFindChoice()' - Find a catalog choice strings entry by name.
//

catalog_choice_strings_t *                 // O - Choice strings entry, or NULL if not found
cfCatalogFindChoice(cups_array_t *choices, // I - Catalog choice array
		    char *name)                        // I - Choice name
{
  catalog_choice_strings_t choice;

  if (!name || !choices)
    return (NULL);

  choice.name = name;
  return (cupsArrayFind(choices, &choice));
}

//
// 'cfCatalogAddOption()' - Add a catalog option strings entry to the options array.
//

catalog_opt_strings_t *             // O - Option strings entry
cfCatalogAddOption(char *name,      // I - Option name
		   char *human_readable,        // I - Human-readable option text
		   cups_array_t *options)       // I - Catalog option array
{
  catalog_opt_strings_t *opt = NULL;

  if (!name || !options)
    return (NULL);

  if ((opt = cfCatalogFindOption(options, name)) == NULL)
  {
    opt = calloc(1, sizeof(catalog_opt_strings_t));
    if (!opt)
      return (NULL);
    opt->human_readable = NULL;
    opt->choices = cupsArrayNew(compare_choices, NULL, NULL, 0,
				 NULL, cfCatalogFreeChoiceStrings);
    if (!opt->choices)
    {
      free(opt);
      return (NULL);
    }
    opt->name = strdup(name);
    if (!cupsArrayAdd(options, opt))
    {
      cfCatalogFreeOptionStrings(opt, NULL);
      return (NULL);
    }
  }

  if (human_readable)
    opt->human_readable = strdup(human_readable);

  return (opt);
}

//
// 'cfCatalogAddChoice()' - Add a catalog choice strings entry to the option's choices array.
//

catalog_choice_strings_t *          // O - Catalog choice entry
cfCatalogAddChoice(char *name,      // I - Choice name
		    char *human_readable,       // I - Human-readable choice text
		    char *opt_name,             // I - Parent option name
		    cups_array_t *options)      // I - Catalog option array
{
  catalog_choice_strings_t *choice = NULL;
  catalog_opt_strings_t *opt;

  if (!name || !human_readable || !opt_name || !options)
    return (NULL);

  opt = cfCatalogAddOption(opt_name, NULL, options);
  if (!opt)
    return (NULL);

  if ((choice = cfCatalogFindChoice(opt->choices, name)) == NULL)
  {
    choice = calloc(1, sizeof(catalog_choice_strings_t));
    if (!choice)
      return (NULL);
    choice->human_readable = NULL;
    choice->name = strdup(name);
    if (!cupsArrayAdd(opt->choices, choice))
    {
      cfCatalogFreeChoiceStrings(choice, NULL);
      return (NULL);
    }
  }

  if (human_readable)
    choice->human_readable = strdup(human_readable);

  return (choice);

}
//
// 'cfCatalogLookUpOption()' - Look up the human-readable text for a catalog option.
//

char *                                            // O - Human-readable option text or NULL
cfCatalogLookUpOption(char *name,                 // I - Option name
		      cups_array_t *options,                  // I - Catalog option array
		      cups_array_t *printer_options)          // I - Printer-specific option array
{
  catalog_opt_strings_t *opt = NULL;

  if (!name || !options)
    return (NULL);

  if (printer_options &&
      (opt = cfCatalogFindOption(printer_options, name)) != NULL)
    return (opt->human_readable);
  if ((opt = cfCatalogFindOption(options, name)) != NULL)
    return (opt->human_readable);
  else
    return (NULL);
}

//
// 'cfCatalogLookUpChoice()' - Look up the human-readable text for a catalog choice.
//

char *                               // O - Human-readable choice text or NULL 
cfCatalogLookUpChoice(char *name,    // I - Choice name
		      char *opt_name,             // I - Parent option name
		      cups_array_t *options,      // I - Catalog option array
		      cups_array_t *printer_options)    // I - Printer-specific option array
{
  catalog_opt_strings_t *opt = NULL;
  catalog_choice_strings_t *choice = NULL;

  if (!name || !opt_name || !options)
    return (NULL);

  if (printer_options &&
      (opt = cfCatalogFindOption(printer_options, opt_name)) != NULL &&
      (choice = cfCatalogFindChoice(opt->choices, name)) != NULL)
    return (choice->human_readable);
  else if ((opt = cfCatalogFindOption(options, opt_name)) != NULL &&
	   (choice = cfCatalogFindChoice(opt->choices, name)) != NULL)
    return (choice->human_readable);
  else
    return (NULL);
}

//
// 'cfCatalogLoad()' - Load a catalog file.
//

void
cfCatalogLoad(const char *location,             // I - Catalog file location (path or URL)
              const char *preferredlocale,        // I - Preferred locale name
	      cups_array_t *options)                    // I - Catalog option array
{
  char tmpfile[1024];
  const char *filename = NULL;
  struct stat statbuf;
  cups_file_t *fp;
  char line[65536];
  char *ptr, *start, *start2, *end, *end2, *sep;
  char *opt_name = NULL, *choice_name = NULL,
       *human_readable = NULL;
  int part = -1; // -1: before first "msgid" or invalid
		 //     line
		 //  0: "msgid"
		 //  1: "msgstr"
		 //  2: "..." = "..."
		 // 10: EOF, save last entry
  int digit;
  int found_in_catalog = 0;

  memset(tmpfile, 0, 1024);

  if (location == NULL || (strncasecmp(location, "http:", 5) &&
			   strncasecmp(location, "https:", 6)))
  {
    if (location == NULL ||
	(stat(location, &statbuf) == 0 &&
	 S_ISDIR(statbuf.st_mode))) // directory?
    {
      filename = cfCatalogFind(location, preferredlocale);
      if (filename)
        found_in_catalog = 1;
    }
    else
      filename = location;
  }
  else
  {
    if (cfGetURI(location, tmpfile, sizeof(tmpfile)))
      filename = tmpfile;
  }
  if (!filename)
    return;

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    if (filename == tmpfile)
      unlink(filename);
    return;
  }

  while (cupsFileGets(fp, line, sizeof(line)) || (part = 10))
  {
    // Find a pair of quotes delimiting a string in each line
    // and optional "msgid" or "msgstr" keywords, or a
    // "..." = "..." pair. Skip comments ('#') and empty lines.
    if (part < 10)
    {
      ptr = line;
      while (isspace(*ptr)) ptr ++;
      if (*ptr == '#' || *ptr == '\0') continue;
      if ((start = strchr(ptr, '\"')) == NULL) continue;
      if ((end = strrchr(ptr, '\"')) == start) continue;
      if (*(end - 1) == '\\') continue;
      start2 = NULL;
      end2 = NULL;
      if (start > ptr)
      {
	if (*(start - 1) == '\\') continue;
	if (strncasecmp(ptr, "msgid", 5) == 0) part = 0;
	if (strncasecmp(ptr, "msgstr", 6) == 0) part = 1;
      }
      else
      {
	start2 = ptr;
	while ((start2 = strchr(start2 + 1, '\"')) < end &&
	       *(start2 - 1) == '\\');
	if (start2 < end)
	{
	  // Line with "..." = "..." of text/strings format
	  end2 = end;
	  end = start2;
	  start2 ++;
	  while (isspace(*start2)) start2 ++;
	  if (*start2 != '=') continue;
	  start2 ++;
	  while (isspace(*start2)) start2 ++;
	  if (*start2 != '\"') continue;
	  start2 ++;
	  *end2 = '\0';
	  part = 2;
	}
	else
	  // Continuation line in message catalog file
	  start2 = NULL;
      }
      start ++;
      *end = '\0';
    }
    // Read out the strings between the quotes and save entries
    if (part == 0 || part == 2 || part == 10)
    {
      // Save previous attribute
      if (human_readable)
      {
	if (opt_name)
	{
	  if (choice_name)
	  {
	    cfCatalogAddChoice(choice_name, human_readable,
			       opt_name, options);
	    free(choice_name);
	  }
	  else
	    cfCatalogAddOption(opt_name, human_readable, options);
	  free(opt_name);
	}
	free(human_readable);
	opt_name = NULL;
	choice_name = NULL;
	human_readable = NULL;
      }
      // Stop the loop after saving the last entry
      if (part == 10)
	break;
      // IPP attribute has to be defined with a single msgid line,
      // no continuation lines
      if (opt_name)
      {
	free (opt_name);
	opt_name = NULL;
	if (choice_name)
	{
	  free (choice_name);
	  choice_name = NULL;
	}
	part = -1;
	continue;
      }
      // No continuation line in text/strings format
      if (part == 2 && (start2 == NULL || end2 == NULL))
      {
	part = -1;
	continue;
      }
      // Check line if it is a valid IPP attribute:
      // No spaces, only lowercase letters, digits, '-', '_',
      // "option" or "option.choice"
      for (ptr = start, sep = NULL; ptr < end; ptr ++)
	if (*ptr == '.') // Separator between option and choice
	{
	  if (!sep) // Only the first '.' counts
	  {
	    sep = ptr + 1;
	    *ptr = '\0';
	  }
	}
	else if (!((*ptr >= 'a' && *ptr <= 'z') ||
		   (*ptr >= '0' && *ptr <= '9') ||
		   *ptr == '-' || *ptr == '_'))
	  break;
      if (ptr < end) // Illegal character found
      {
	part = -1;
	continue;
      }
      if (strlen(start) > 0) // Option name found
	opt_name = strdup(start);
      else // Empty option name
      {
	part = -1;
	continue;
      }
      if (sep && strlen(sep) > 0) // Choice name found
	choice_name = strdup(sep);
      else // Empty choice name
	choice_name = NULL;
      if (part == 2) // Human-readable string in the same line
      {
	start = start2;
	end = end2;
      }
    }
    if (part == 1 || part == 2)
    {
      // msgid was not for an IPP attribute, ignore this msgstr
      if (!opt_name) continue;
      // Empty string
      if (start == end) continue;
      // Unquote string
      ptr = start;
      end = start;
      while (*ptr)
      {
	if (*ptr == '\\')
	{
	  ptr ++;
	  if (isdigit(*ptr))
	  {
	    digit = 0;
	    *end = 0;
	    while (isdigit(*ptr) && digit < 3)
	    {
	      *end = *end * 8 + *ptr - '0';
	      digit ++;
	      ptr ++;
	    }
	    end ++;
	  }
	  else
	  {
	    if (*ptr == 'n')
	      *end ++ = '\n';
	    else if (*ptr == 'r')
	      *end ++ = '\r';
	    else if (*ptr == 't')
	      *end ++ = '\t';
	    else
	      *end ++ = *ptr;
	    ptr ++;
	  }
	}
	else
	  *end ++ = *ptr ++;
      }
      *end = '\0';
      // Did the unquoting make the string empty?
      if (strlen(start) == 0) continue;
      // Add the string to our human-readable string
      if (human_readable) // Continuation line
      {
      size_t human_readable_size = sizeof(char) *
                (strlen(human_readable) +
                  strlen(start) + 2);
      char *temp_hr = realloc(human_readable, human_readable_size);
      if (!temp_hr) 
      {
        // If realloc fails, free the original memory to prevent a leak
        free(human_readable);
        human_readable = NULL;
        continue; // Skip the rest of this line's processing
      }
      human_readable = temp_hr;
      ptr = human_readable + strlen(human_readable);
      *ptr = ' ';
      strncpy(ptr + 1, start,
        human_readable_size - (ptr - human_readable) - 1);
      }
      else // First line
	human_readable = strdup(start);
    }
  }
  cupsFileClose(fp);
  if (choice_name != NULL)
    free(choice_name);
  if (opt_name != NULL)
    free(opt_name);
  if (filename == tmpfile)
    unlink(filename);
  else
    if (found_in_catalog)
      free((char *)filename);
}
