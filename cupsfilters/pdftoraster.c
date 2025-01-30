//
// PDF to Raster filter function for libcupsfilters.
//
// Copyright (c) 2025 by Uddhav Phatak <uddhavabhijeet@gmail.com> 
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <cupsfilters/filter.h>
#include <cupsfilters/raster.h>
#include <cupsfilters/image.h>
#include <cupsfilters/colormanager.h>
#include <pdfio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <lcms2.h>

#define TEMP_PREFIX "/tmp/pdftoraster-XXXXXX"

typedef struct {
  char *input_filename;
  char *temp_base;
  cf_logfunc_t log;
  void *logdata;
  cmsHPROFILE dst_profile;
} pdftoraster_doc_t;

static void 
cleanup_temp_files(pdftoraster_doc_t *doc) 
{
  if (!doc || !doc->temp_base) return;
    
  char cmd[2048];
  snprintf(cmd, sizeof(cmd), "rm -rf %s* 2>/dev/null", doc->temp_base);
  int rc = system(cmd);
  (void)rc;
}

static int 
get_pdf_page_count(const char *filename, 
		   cf_logfunc_t log, 
		   void *ld) 
{
  pdfio_file_t *pdf = pdfioFileOpen(filename, NULL, NULL, NULL, NULL);
    
  int pages = pdfioFileGetNumPages(pdf);
  pdfioFileClose(pdf);
  return pages;
}

static int 
render_page_to_ppm(pdftoraster_doc_t *doc, 
		   int pagenum, 
		   int resolution) 
{
  char temp_ppm[1024];
  snprintf(temp_ppm, sizeof(temp_ppm), "%s-%06d.ppm", doc->temp_base, pagenum);

  const char *args[] = {
    "pdftoppm",
    "-r", NULL,
    "-f", NULL,
    "-l", NULL,
    "-cropbox",
    "-singlefile",
    doc->input_filename,
    temp_ppm,
    NULL
  };

  char res_str[16], page_str[16];
  snprintf(res_str, sizeof(res_str), "%d", resolution);
  snprintf(page_str, sizeof(page_str), "%d", pagenum);
  
  args[2] = res_str;
  args[4] = page_str;
  args[5] = page_str;

  pid_t pid = fork();
  if (pid == 0) {
      execvp(args[0], (char *const *)args);
      exit(EXIT_FAILURE);
  }

  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int 
convert_ppm_to_raster(pdftoraster_doc_t *doc, 
		      const char *ppm_path, 
		      cups_raster_t *raster, 
		      cups_page_header2_t *header) 
{
  cf_image_t *img = NULL;
  unsigned char *line = NULL;
  cmsHTRANSFORM transform = NULL;
  cmsHPROFILE src_profile = NULL;
  int ret = -1;

  img = cfImageOpen(ppm_path,
                    CUPS_CSPACE_RGB,  // Primary color space
                    CUPS_CSPACE_W,    // Secondary color space
                    100,              // Saturation
                    0,                // Hue
                    NULL);            // LUT

  if (!img) 
  {
    doc->log(doc->logdata, CF_LOGLEVEL_ERROR, "Failed to open PPM image");
    goto out;
  }

  src_profile = cmsCreate_sRGBProfile();
  if (!doc->dst_profile) {
    doc->dst_profile = cmsCreate_sRGBProfile(); 
  }

  transform = cmsCreateTransform(src_profile, TYPE_RGB_8,
                                 doc->dst_profile, TYPE_CMYK_8,
                                 INTENT_PERCEPTUAL, 0);

  line = malloc(header->cupsBytesPerLine);
  if (!line) {
      doc->log(doc->logdata, CF_LOGLEVEL_ERROR, "Memory allocation failed");
      goto out;
  }

  for (unsigned y = 0; y < header->cupsHeight; y++) {
    unsigned char pixels[header->cupsWidth * 3];
      
    if (!cfImageGetRow(img, 0, y, header->cupsWidth, pixels)) {
      doc->log(doc->logdata, CF_LOGLEVEL_ERROR, "Failed to get image row");
      goto out;
    }

    cmsDoTransform(transform, pixels, line, header->cupsWidth);

    if (!cupsRasterWritePixels(raster, line, header->cupsBytesPerLine)) {
      doc->log(doc->logdata, CF_LOGLEVEL_ERROR, "Raster write failed");
      goto out;
    }
  }

  ret = 0;

out:
    free(line);
    if (transform) cmsDeleteTransform(transform);
    if (src_profile) cmsCloseProfile(src_profile);
    if (img) cfImageClose(img);
    return ret;
}

int 
cfFilterPDFToRaster(int inputfd, 
		    int outputfd, 
		    int inputseekable,
                    cf_filter_data_t *data, 
		    void *parameters) 
{
  pdftoraster_doc_t doc = {0};
  cups_raster_t *raster = NULL;
  cups_page_header2_t header;
  int ret = 1;
  char temp_input[1024];
  int fd = -1;

  doc.log = data->logfunc;
  doc.logdata = data->logdata;

  strncpy(temp_input, TEMP_PREFIX, sizeof(temp_input));
  if ((fd = mkstemp(temp_input)) == -1) {
    doc.log(doc.logdata, CF_LOGLEVEL_ERROR,
            "Temp file creation failed: %s", strerror(errno));
    goto cleanup;
  }

  char buf[8192];
  ssize_t bytes;
  while ((bytes = read(inputfd, buf, sizeof(buf))) > 0) {
    if (write(fd, buf, bytes) != bytes) {
      doc.log(doc.logdata, CF_LOGLEVEL_ERROR,
              "Write error: %s", strerror(errno));
      goto cleanup;
    }
  }
  close(fd);
  fd = -1;
  doc.input_filename = temp_input;

  doc.temp_base = strdup(TEMP_PREFIX);
  if (!mkdtemp(doc.temp_base)) {
    doc.log(doc.logdata, CF_LOGLEVEL_ERROR,
            "Temp directory creation failed: %s", strerror(errno));
    goto cleanup;
  }

  int npages = get_pdf_page_count(doc.input_filename, doc.log, doc.logdata);
  if (npages < 1) {
    doc.log(doc.logdata, CF_LOGLEVEL_ERROR, "Invalid page count");
    goto cleanup;
  }

  cf_filter_out_format_t outformat = CF_FILTER_OUT_FORMAT_CUPS_RASTER;
  if (data->final_content_type) {
    if (strcasestr(data->final_content_type, "pwg")) {
      outformat = CF_FILTER_OUT_FORMAT_PWG_RASTER;
    } 
    else if (strcasestr(data->final_content_type, "urf")) {
      outformat = CF_FILTER_OUT_FORMAT_APPLE_RASTER;
    }
  }

  memset(&header, 0, sizeof(header));
  if (!cfRasterPrepareHeader(&header, data, outformat,
                             (outformat == CF_FILTER_OUT_FORMAT_PWG_RASTER ||
                              outformat == CF_FILTER_OUT_FORMAT_APPLE_RASTER) ?
                              outformat : CF_FILTER_OUT_FORMAT_CUPS_RASTER,
                              0, NULL)) {
    doc.log(doc.logdata, CF_LOGLEVEL_ERROR, "Header preparation failed");
    goto cleanup;
  }

  raster = cupsRasterOpen(outputfd, 
      			  outformat == CF_FILTER_OUT_FORMAT_PWG_RASTER ? CUPS_RASTER_WRITE_PWG :
			   outformat == CF_FILTER_OUT_FORMAT_APPLE_RASTER ? CUPS_RASTER_WRITE_APPLE :
			    CUPS_RASTER_WRITE);

  if (!raster) {
    doc.log(doc.logdata, CF_LOGLEVEL_ERROR, "Raster output failed");
    goto cleanup;
  }

  for (int page = 1; page <= npages; page++) 
  {
    char ppm_path[1024];
    snprintf(ppm_path, sizeof(ppm_path), "%s-%06d.ppm", doc.temp_base, page);

    if (render_page_to_ppm(&doc, page, header.HWResolution[0]) ||
        convert_ppm_to_raster(&doc, ppm_path, raster, &header)) {
        doc.log(doc.logdata, CF_LOGLEVEL_ERROR, 
                "Page %d conversion failed", page);
        goto cleanup;
    }
    unlink(ppm_path);
  }

  ret = 0;

cleanup:
  if (fd != -1) close(fd);
  if (raster) cupsRasterClose(raster);
  if (doc.input_filename) unlink(doc.input_filename);
  cleanup_temp_files(&doc);
  free(doc.temp_base);
  return ret;
}
