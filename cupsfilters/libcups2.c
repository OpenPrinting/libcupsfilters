//
// Wrapper function for ipp.c for libcups2.
//
// Copyright 2020-2022 by Till Kamppeter.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
//
// Include necessary headers...
//


#include <config.h>

#ifdef HAVE_LIBCUPS2

#include <cupsfilters/libcups2-private.h>
#include <cupsfilters/ipp.h>
#include <cups/http.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

static int                             
convert_to_port(char *a)
{
  int port = 0;

  for (int i = 0; i < strlen(a); i ++)
    port = port*10 + (a[i] - '0');

  return (port);
}


char *
cfResolveURI2(const char *uri,
	      int is_fax)
{
  int  ippfind_pid = 0,		// Process ID of ippfind for IPP
       post_proc_pipe[2],	// Pipe to post-processing for IPP
       wait_children,		// Number of child processes left
       wait_pid,		// Process ID from wait()
       wait_status,		// Status from child
       exit_status = 0,		// Exit status
       bytes,
       port,
       i,
       output_of_fax_uri = 0,
       is_local;
  char *ippfind_argv[100],	// Arguments for ippfind
       *ptr_to_port = NULL,
       *reg_type,
       *resolved_uri = NULL,	// Buffer for resolved URI
       *resource_field = NULL,
       *service_hostname = NULL,
       // URI components...
       scheme[32],
       userpass[256],
       hostname[1024],
       resource[1024],
       *buffer = NULL,		// Copy buffer
       *ptr;			// Pointer into string;
  cups_file_t *fp;		// Post-processing input file
  int  status;			// Status of GET request

  status = httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme),
			   userpass, sizeof(userpass),
			   hostname, sizeof(hostname), &port, resource,
			   sizeof(resource));
  if (status < HTTP_URI_STATUS_OK)
    // Invalid URI
    goto error;

  // URI is not DNS-SD-based, so do not resolve
  if ((reg_type = strstr(hostname, "._tcp")) == NULL)
    return (strdup(uri));

  resolved_uri =
    (char *)malloc(CF_GET_PRINTER_ATTRIBUTES_MAX_URI_LEN * (sizeof(char)));
  if (resolved_uri == NULL)
    goto error;
  memset(resolved_uri, 0, CF_GET_PRINTER_ATTRIBUTES_MAX_URI_LEN);

  reg_type --;
  while (reg_type >= hostname && *reg_type != '.')
    reg_type --;
  if (reg_type < hostname)
    goto error;
  *reg_type++ = '\0';

  i = 0;
  ippfind_argv[i++] = "ippfind";
  ippfind_argv[i++] = reg_type;           // list IPP(S) entries
  ippfind_argv[i++] = "-T";               // DNS-SD poll timeout
  ippfind_argv[i++] = "0";                // Minimum time required
  if (is_fax)
  {
    ippfind_argv[i++] = "--txt";
    ippfind_argv[i++] = "rfo";
  }
  ippfind_argv[i++] = "-N";
  ippfind_argv[i++] = hostname;
  ippfind_argv[i++] = "-x";
  ippfind_argv[i++] = "echo";             // Output the needed data fields
  ippfind_argv[i++] = "-en";              // separated by tab characters
  if(is_fax)
    ippfind_argv[i++] = "\n{service_hostname}\t{txt_rfo}\t{service_port}\t";
  else
    ippfind_argv[i++] = "\n{service_hostname}\t{txt_rp}\t{service_port}\t";
  ippfind_argv[i++] = ";";
  ippfind_argv[i++] = "--local";          // Rest only if local service
  ippfind_argv[i++] = "-x";
  ippfind_argv[i++] = "echo";             // Output an 'L' at the end of the
  ippfind_argv[i++] = "-en";              // line
  ippfind_argv[i++] = "L";
  ippfind_argv[i++] = ";";
  ippfind_argv[i++] = NULL;

  //
  // Create a pipe for passing the ippfind output to post-processing
  //

  if (pipe(post_proc_pipe))
    goto error;

  if ((ippfind_pid = fork()) == 0)
  {
    //
    // Child comes here...
    //

    dup2(post_proc_pipe[1], 1);
    close(post_proc_pipe[0]);
    close(post_proc_pipe[1]);

    execvp(CUPS_IPPFIND, ippfind_argv);

    exit(1);
  }
  else if (ippfind_pid < 0)
  {
    //
    // Unable to fork!
    //

    goto error;
  }

  close(post_proc_pipe[1]);

  fp = cupsFileOpenFd(post_proc_pipe[0], "r");

  buffer =
    (char*)malloc(CF_GET_PRINTER_ATTRIBUTES_MAX_OUTPUT_LEN * sizeof(char));
  if (buffer == NULL)
    goto error;
  memset(buffer, 0, CF_GET_PRINTER_ATTRIBUTES_MAX_OUTPUT_LEN);

  while ((bytes =
	  cupsFileGetLine(fp, buffer,
			  CF_GET_PRINTER_ATTRIBUTES_MAX_OUTPUT_LEN)) > 0)
  {
    // Mark all the fields of the output of ippfind
    ptr = buffer;

    // ignore new lines
    if (bytes < 3)
      goto read_error;

    // First, build the DNS-SD-service-name-based URI ...
    while (ptr && !isalnum(*ptr & 255)) ptr ++;

    service_hostname = ptr; 
    ptr = memchr(ptr, '\t',
		 CF_GET_PRINTER_ATTRIBUTES_MAX_OUTPUT_LEN - (ptr - buffer));
    if (!ptr) goto read_error;
    *ptr = '\0';
    ptr ++;

    resource_field = ptr;
    ptr = memchr(ptr, '\t',
		 CF_GET_PRINTER_ATTRIBUTES_MAX_OUTPUT_LEN - (ptr - buffer));
    if (!ptr) goto read_error;
    *ptr = '\0';
    ptr ++;

    ptr_to_port = ptr;
    ptr = memchr(ptr, '\t',
		 CF_GET_PRINTER_ATTRIBUTES_MAX_OUTPUT_LEN - (ptr - buffer));
    if (!ptr) goto read_error;
    *ptr = '\0';
    ptr ++;

    // Do we have a local service so that we have to set the host name to
    // "localhost"?
    is_local = (*ptr == 'L');

    ptr = strchr(reg_type, '.');
    if (!ptr) goto read_error;
    *ptr = '\0';

    port = convert_to_port(ptr_to_port);

    httpAssembleURIf(HTTP_URI_CODING_ALL, resolved_uri,
		     2047, reg_type + 1, NULL,
		     (is_local ? "localhost" : service_hostname), port, "/%s",
		     resource_field);

    if (is_fax)
      output_of_fax_uri = 1; // fax-uri requested from fax-capable device

  read_error:
    memset(buffer, 0, CF_GET_PRINTER_ATTRIBUTES_MAX_OUTPUT_LEN);
  }

  cupsFileClose(fp);

  if (buffer != NULL)
    free(buffer);

  //
  // Wait for the child processes to exit...
  //

  wait_children = 1;

  while (wait_children > 0)
  {
    //
    // Wait until we get a valid process ID or the job is canceled...
    //

    while ((wait_pid = wait(&wait_status)) < 0 && errno == EINTR);

    if (wait_pid < 0)
      break;

    wait_children --;

    //
    // Report child status...
    //

    if (wait_status)
    {
      if (WIFEXITED(wait_status))
      {
	exit_status = WEXITSTATUS(wait_status);
        if (wait_pid == ippfind_pid && exit_status <= 2)
          exit_status = 0;	  
      }
      else if (WTERMSIG(wait_status) == SIGTERM)
      {
	// All OK, no error
      }
      else
      {
	exit_status = WTERMSIG(wait_status);
      }
    }
  }
  if (is_fax && !output_of_fax_uri)
    goto error;

  return (resolved_uri);

  //
  // Exit...
  //

 error:
  if (resolved_uri != NULL)
    free(resolved_uri);
  return (NULL);
}

#endif // HAVE_LIBCUPS2
