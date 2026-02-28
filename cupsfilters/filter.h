//
// Filter functions header file for libcupsfilters.
//
// Copyright © 2020-2022 by Till Kamppeter.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_FILTERS_FILTER_H_
#  define _CUPS_FILTERS_FILTER_H_

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Include necessary headers...
//

#  include "log.h"

#  include <stdio.h>
#  include <stdlib.h>
#  include <time.h>
#  include <math.h>

#  if defined(WIN32) || defined(__EMX__)
#    include <io.h>
#  else
#    include <unistd.h>
#    include <fcntl.h>
#  endif // WIN32 || __EMX__

#  include <cups/cups.h>
#  include <cups/raster.h>


//
// Renamed CUPS type in API
//

#  if CUPS_VERSION_MAJOR < 3
#    define cups_page_header_t cups_page_header2_t
#  endif


//
// Types and structures...
//

typedef int (*cf_filter_iscanceledfunc_t)(void *data);

typedef struct cf_filter_data_s
{
  char *printer;             // Print queue name or NULL
  int job_id;                // Job ID or 0
  char *job_user;            // Job user or NULL
  char *job_title;           // Job title or NULL
  int copies;                // Number of copies
                             // (1 if filter(s) should not treat it)
  char *content_type;        // Input MIME type (CUPS env variable
                             // CONTENT_TYPE) or NULL
  char *final_content_type;  // Output MIME type (CUPS env variable
			     // FINAL_CONTENT_TYPE) or NULL
  ipp_t *job_attrs;          // IPP attributes passed along with the job
  ipp_t *printer_attrs;      // Printer capabilities in IPP format
			     // (what is answered to get-printer-attributes
  cups_page_header_t *header;
                             // CUPS/PWG Raster header (optional)
  int           num_options;
  cups_option_t *options;    // Job options as key/value pairs
  int back_pipe[2];          // File descriptors of backchannel pipe
  int side_pipe[2];          // File descriptors of sidechannel pipe
  cups_array_t *extension;   // Extension data
  cf_logfunc_t logfunc;      // Logging function, NULL for no logging
  void *logdata;             // User data for logging function, can be NULL
  cf_filter_iscanceledfunc_t iscanceledfunc;
                             // Function returning 1 when job is
			     // canceled, NULL for not supporting stop
			     // on cancel
  void *iscanceleddata;      // User data for is-canceled function, can be
			     // NULL
} cf_filter_data_t;

typedef struct cf_filter_data_ext_s
{
  char *name;
  void *ext;
} cf_filter_data_ext_t;

typedef int (*cf_filter_function_t)(int inputfd, int outputfd,
				    int inputseekable, cf_filter_data_t *data,
				    void *parameters);

typedef enum cf_filter_out_format_e   // Possible output formats for filter
				      // functions
{
  CF_FILTER_OUT_FORMAT_PDF,	      // PDF
  CF_FILTER_OUT_FORMAT_PDF_IMAGE,     // Raster-only PDF
  CF_FILTER_OUT_FORMAT_PCLM,	      // PCLM
  CF_FILTER_OUT_FORMAT_CUPS_RASTER,   // CUPS Raster
  CF_FILTER_OUT_FORMAT_PWG_RASTER,    // PWG Raster
  CF_FILTER_OUT_FORMAT_APPLE_RASTER,  // Apple Raster
  CF_FILTER_OUT_FORMAT_PXL            // PCL-XL
} cf_filter_out_format_t;

typedef struct cf_filter_filter_in_chain_s // filter entry for CUPS array to
					   // be supplied to cfFilterChain()
					   // filter function
{
  cf_filter_function_t function; // Filter function to be called
  void *parameters;              // Parameters for this filter function call
  char *name;                    // Name/comment, only for logging
} cf_filter_filter_in_chain_t;

typedef struct cf_filter_external_s // Parameters for the
				    // cfFilterExternal() filter
				    // function
{
  const char *filter;        // Path/Name of the CUPS filter to be called by
			     // this filter function, required
  int exec_mode;             // 0 if we call a CUPS filter, -1 if we call
                             // a System V interface script, 1 if we call a CUPS
			     // backend, 2 if we call a CUPS backend in
			     // device discovery mode
  int num_options;           // Extra options for the 5th command line
  cups_option_t *options;    // argument, options of filter_data have
                             // priority, 0/NULL if none
  char **envp;               // Additional environment variables, the already
                             // defined ones stay valid but can be overwritten
                             // by these ones, NULL if none
} cf_filter_external_t;

typedef struct cf_filter_texttopdf_parameter_s // parameters container of
					       // environemnt variables needed
					       // by texttopdf filter
					       // function
{
  char *data_dir;
  char *char_set;
  char *content_type;
  char *classification;
} cf_filter_texttopdf_parameter_t;

typedef struct cf_filter_universal_parameter_s // Contains input and output
					       // type to be supplied to the
					       // universal function, and also
					       // parameters for
					       // cfFilterTextToPDF()
{
  char *actual_output_type;
  cf_filter_texttopdf_parameter_t texttopdf_params;
  const char *bannertopdf_template_dir;
} cf_filter_universal_parameter_t;


//
// Prototypes...
//
//'cfCUPSLogFunc()' - Logging callback functions used by filters.
//
// data: User-defined logging context.
// level: Log level of the message.
// message: printf-style format string.
// ...: Optional arguments for the format string.
//
// This function is used by filters to report status, debug
// information, warnings, and errors. 
//

extern void cfCUPSLogFunc(void *data,
			  cf_loglevel_t level,
			  const char *message,
			  ...);

//
// 'cfCUPSIsCanceledFunc()' - Check whether the current job has been canceled.
//
// data: User-defined job context.
// Returns 1 if canceled, 0 otherwise.
//

extern int cfCUPSIsCanceledFunc(void *data);


// 'cfFilterDataAddExt()' - Add an extension to the filter data.
//
// data: Filter data structure.
// name: Name of the extension.
// ext: Pointer to the extension data.
// 

extern void *cfFilterDataAddExt(cf_filter_data_t *data, const char *name,
				void *ext);

//
// '*cfFilterDataGetExt()' - Retrieve an extension from the filter data.
//

extern void *cfFilterDataGetExt(cf_filter_data_t *data, const char *name);

//
// '*cfFilterDataRemoveExt()'- Remove an extension from the filter data.
//

extern void *cfFilterDataRemoveExt(cf_filter_data_t *data, const char *name);

//
// '*cfFilterGetEnvVar()' - Get the value of an environment variable from a given
//                       environment array.
// name: Name of the variable.
// env: Environment array.
//
// Returns a pointer to value string, or NULL if not found.
//


extern char *cfFilterGetEnvVar(char *name, char **env);

//
// 'cfFilterAddEnvVar()' - Add or update an environment variable.
//
// name: Name of the variable.
// value: Value of the variable.
// env: Environment array.
//
// Returns 0 on success, -1 on failure. 
//

extern int cfFilterAddEnvVar(char *name, char *value, char ***env);


//
// 'cfFilterTee()' - Filter function that copies input to output and optionally to a file.
//
// Parameters: parameters points to a const char* specifying the
// filename/path to copy the data to. 
//

extern int cfFilterTee(int inputfd,
		       int outputfd,
		       int inputseekable,
		       cf_filter_data_t *data,
		       void *parameters);

//
// 'cfFilterPOpen()' - Start a filter function in a separate process.
//
// filter_func: Filter function to execute.
// parameters: Filter-specific parameters passed to the filter.
// filter_pid: Receives the process ID of the started filter.
//

extern int cfFilterPOpen(cf_filter_function_t filter_func, 
			 int inputfd,
			 int outputfd,
			 int inputseekable,
			 cf_filter_data_t *data,
			 void *parameters,
			 int *filter_pid);


// 
// 'cfFilterPClose()' - Wait for a filter process started by cfFilterPOpen().
//
// fd: Input pipe file descriptor.
// filter_pid: Process ID of the filter.
// data: Job and printer.
// Return: 0 on success, -1 on failure.
//

extern int cfFilterPClose(int fd,
			  int filter_pid,
			  cf_filter_data_t *data);


//
// 'cfFilterChain()' - Execute a chain of filter functions.
//
// Parameters: Pointer to an array of cf_filter_filter_in_chain_t defining the
//             list of filters to execute. Each filter receives the previous filter's
//             output as input.
//             

extern int cfFilterChain(int inputfd,
			 int outputfd,
			 int inputseekable,
			 cf_filter_data_t *data,
			 void *parameters);

//
// 'cfFilterExternal()' - Executes an external filter or backend.
//
// This function runs a CUPS/System V filter or backend based on the input and output types
// specified in the parameters.
//
// See "man filter" and "man backend" for more information on CUPS filters and backends.
//
// System V interface script:
// https://www.ibm.com/docs/en/aix/7.2?topic=configuration-printer-interface-scripts
//


extern int cfFilterExternal(int inputfd,
			    int outputfd,
			    int inputseekable,
			    cf_filter_data_t *data,
			    void *parameters);


//
// 'cfFilterOpenBackAndSidePipes()' - Open the back and side pipes for communication with filters.
//	


extern int cfFilterOpenBackAndSidePipes(cf_filter_data_t *data);

//
// 'cfFilterCloseBackAndSidePipes()' - Close the back and side pipes for communication with filters.
//


extern void cfFilterCloseBackAndSidePipes(cf_filter_data_t *data);

//
// 'cfFilterGhostscript()' - Run Ghostscript to generate printer output.
// 
// Converts input data using Ghostscript to the specified output format.
// The output format must be set via data->final_content_type or alternatively as parameter of type 
// cf_filter_out_format_t. 
//
// Output formats: PDF, raster-only PDF, PCLm, PostScript, CUPS Raster,
// PWG Raster, Apple Raster, PCL-XL
//
// Note: With the Apple Raster selection and a Ghostscript version
// without "appleraster" output device (9.55.x and older) the output
// is actually CUPS Raster but information about available color
// spaces and depths is taken from the urf-supported printer IPP
// attribute. This mode is for further processing with
// rastertopwg. With Ghostscript supporting Apple Raster output
// (9.56.0 and newer), we actually produce Apple Raster and no further
// filter is required.
//


extern int cfFilterGhostscript(int inputfd,
			       int outputfd,
			       int inputseekable,
			       cf_filter_data_t *data,
			       void *parameters);

//
// 'cfFilterBannerToPDF()' - Generate a PDF banner page.
//
// Creates a banner or test page in PDF format.
// The parameter is a const char* specifying the template directory.
// CUPS uses /usr/share/cups/data/ by default.
// If a PDF file with added banner instructions is provided as input,
// the template directory is not required.
//


extern int cfFilterBannerToPDF(int inputfd,
			       int outputfd,
			       int inputseekable,
			       cf_filter_data_t *data,
			       void *parameters);

//
// 'cfFilterImageToPDF()' - Convert an image to PDF format.		
//


extern int cfFilterImageToPDF(int inputfd,
			      int outputfd,
			      int inputseekable,
			      cf_filter_data_t *data,
			      void *parameters);

//
// 'cfFilterImageToRaster()' - Convert an image to raster format.
//
// Requires specification of output format via data->final_content_type
//
// Output formats: CUPS Raster, PWG Raster, Apple Raster, PCLM
//
// Note: On the Apple Raster, PWG Raster, and PCLm selection the
// output is actually CUPS Raster but information about available
// color spaces and depths is taken from the urf-supported or
// pwg-raster-document-type-supported printer IPP attributes or from a
// supplied CUPS Raster sample header. This mode is for further
// processing with rastertopwg and/or pwgtopclm. This can change in the
// future when we add Apple Raster and PWG Raster output support to
// this filter function.
//


extern int cfFilterImageToRaster(int inputfd,
				 int outputfd,
				 int inputseekable,
				 cf_filter_data_t *data,
				 void *parameters);

//
// 'cfFilterMuPDFToPWG()' - Convert PDF or XPS input to PWG Raster format using MuPDF.
//	
// Requires specification of output format via data->final_content_type.
//
// Output formats: CUPS Raster, PWG Raster, Apple Raster, PCLm
//
// Note: With CUPS Raster, Apple Raster, or PCLm selections the output
// is actually PWG Raster but information about available color spaces
// and depths is taken from the urf-supported printer IPP attribute, the
// pclm- attributes, or from a supplied CUPS Raster sample header
// (PCLM is always sGray/sRGB 8-bit). These modes are for further processing
// with pwgtoraster or pwgtopclm. This can change in the future when
// MuPDF adds further output formats.
//


extern int cfFilterMuPDFToPWG(int inputfd,
			      int outputfd,
			      int inputseekable,
			      cf_filter_data_t *data,
			      void *parameters);

//
// 'cfFilterPCLmToRaster()' - Convert PCLm input to raster format.
//
// Requires specification of output format via data->final_content_type
//
// Output formats: CUPS Raster, Apple Raster, PWG Raster
//

extern int cfFilterPCLmToRaster(int inputfd,
				int outputfd,
				int inputseekable,
				cf_filter_data_t *data,
				void *parameters);

//
// 'cfFilterPDFToPDF()' - Process PDF input and produce PDF output.
//
// (Optional) Specification of output format via
// data->final_content_type is used for determining whether this
// filter function does page logging for CUPS (output of "PAGE: XX YY" 
// log messages) or not and also to determine whether the printer or
// driver generates copies or whether we have to send the pages
// repeatedly.
// Alternatively, the options "pdf-filter-page-logging",
// "hardware-copies", and "hardware-collate" can be used to manually do these selections.
//	


extern int cfFilterPDFToPDF(int inputfd,
			    int outputfd,
			    int inputseekable,
			    cf_filter_data_t *data,
			    void *parameters);

//
// 'cfFilterPDFToRaster()' - Convert PDF input to raster format.
//
// Requires specification of output format via data->final_content_type
//
// Output formats: CUPS Raster, PWG Raster, Apple Raster, PCLm
//
// Note: With PCLm selection the output is actually PWG Raster but color space and
// depth will be 8-bit sRGB or SGray, the only color spaces supported by PCLm. 
// This mode is for further processing with pwgtopclm.
//


extern int cfFilterPDFToRaster(int inputfd,
			       int outputfd,
			       int inputseekable,
			       cf_filter_data_t *data,
			       void* parameters);

//
// 'cfFilterPWGToRaster()' - Convert PWG Raster input to raster format.
//
// Requires specification of output format via data->final_content_type
//
// Output formats: CUPS Raster, Apple Raster, PWG Raster
//
//

extern int cfFilterPWGToRaster(int inputfd,
			       int outputfd,
			       int inputseekable,
			       cf_filter_data_t *data,
			       void *parameters);

//
// 'cfFilterPWGToPDF()' - Convert PWG Raster input to PDF format.
//
// Requires specification of output format via data->final_content_type
// or alternatively as parameter of type cf_filter_out_format_t.
//
// Output formats: PDF, PCLm
//	


extern int cfFilterPWGToPDF(int inputfd,
			       int outputfd,
			       int inputseekable,
			       cf_filter_data_t *data,
			       void *parameters);

// 'cfFilterRasterToPWG()' - Convert raster input to PWG Raster format.
//
// Requires specification of output format via data->final_content_type.
//
// Output formats: Apple Raster or PWG Raster, if PCLM is specified PWG
// Raster is produced to feed into the cfFilterPWGToPDF() filter function
//

extern int cfFilterRasterToPWG(int inputfd,
			       int outputfd,
			       int inputseekable,
			       cf_filter_data_t *data,
			       void *parameters);

//
// 'cfFilterTextToPDF()' - Convert text input to PDF format.
//
// Parameters: cf_filter_texttopdf_parameter_t*
//
// Data directory (fonts, charsets), charset, content type (for prettyprint),
// classification (for overprint/watermark)
//

extern int cfFilterTextToPDF(int inputfd,
			     int outputfd,
			     int inputseekable,
			     cf_filter_data_t *data,
			     void *parameters);

//
// 'cfFilterTextToText()' - Process plain text input and produce plain text output.
//

extern int cfFilterTextToText(int inputfd,
			      int outputfd,
			      int inputseekable,
			      cf_filter_data_t *data,
			      void *parameters);

//
// 'cfFilterUniversal()' - Universal filter function for various conversions.
//
// Requires specification of input format via data->content_type and 
// job's final output format via data->final_content_type.
//
// Parameters: cf_filter_universal_parameter_t*
//
// Contains: actual_output_type: Format which the filter should
//           actually produce if different from job's final output
//           format, otherwise NULL to produce the job's final output
//           format
//	     texttopdf_params: parameters for texttopdf
//

extern int cfFilterUniversal(int inputfd,
			     int outputfd,
			     int inputseekable,
			     cf_filter_data_t *data,
			     void *parameters);


#  ifdef __cplusplus
}
#  endif // __cplusplus

#endif // !_CUPS_FILTERS_FILTER_H_
