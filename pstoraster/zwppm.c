/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* zwppm.c */
/* Obsolete PPM-file-writing operator */
#include "memory_.h"
#include "ghost.h"
#include "gp.h"
#include "errors.h"
#include "oper.h"
#include "stream.h"
#include "files.h"
#include "gscdefs.h"
#include "gsmatrix.h"			/* for gxdevice.h */
#include "gxdevice.h"
#include "gxdevmem.h"

/* Forward references */
private int gs_writeppmfile(P2(gx_device_memory *, FILE *));

/* <file> <device> writeppmfile - */
private int
zwriteppmfile(register os_ptr op)
{	stream *s;
	int code;
	check_read_type(*op, t_device);
	check_write_file(s, op - 1);
	if ( !gs_device_is_memory(op->value.pdevice) )
	  return_error(e_rangecheck);
	sflush(s);
	code = gs_writeppmfile((gx_device_memory *)(op->value.pdevice), s->file);
	if ( code >= 0 )
	  pop(2);
	return code;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zwppm_op_defs) {
	{"2writeppmfile", zwriteppmfile},
END_OP_DEFS(0) }

/* ------ Non-operator routines ------ */

/* Dump the contents of a memory device in PPM format. */
private int
gs_writeppmfile(gx_device_memory *md, FILE *file)
{	int raster = gx_device_raster((gx_device *)md, 0);
	int height = md->height;
	int depth = md->color_info.depth;
	uint rsize = raster * 3;	/* * 3 just for mapped color */
	byte *row = (byte *)gs_malloc(rsize, 1, "ppm file buffer");
	const char *header;
	int y;
	int code = 0;			/* return code */
	if ( row == 0 )			/* can't allocate row buffer */
	  return_error(e_VMerror);

	/* A PPM file consists of: magic number, comment, */
	/* width, height, maxvalue, (r,g,b).... */

	/* Dispatch on the type of the device -- 1-bit mono, */
	/* 8-bit (gray scale or color), 24- or 32-bit true color. */

	switch ( depth )
	   {
	case 1:
	  header = "P4\n# %s 1 bit mono image dump\n%d %d\n";
	  break;

	case 8:
	  header = (gx_device_has_color(md) ?
	    "P6\n# %s 8 bit mapped color image dump\n%d %d\n255\n" :
	    "P5\n# %s 8 bit gray scale image dump\n%d %d\n255\n");
	  break;

	case 24:
	  header = "P6\n# %s 24 bit color image dump\n%d %d\n255\n";
	  break;

	case 32:
	  header = "P6\n# %s 32 bit color image dump\n%d %d\n255\n";
	  break;

	default:			/* shouldn't happen! */
	  code = e_undefinedresult;
	  goto done;
	   }

	/* Write the header. */
	fprintf(file, header, gs_product, md->width, height);

	/* Dump the contents of the image. */
	for ( y = 0; y < height; y++ )
	   {	int count;
		register byte *from, *to, *end;
		(*dev_proc(md, get_bits))((gx_device *)md, y, row, NULL);
		switch ( depth )
		   {
		case 8:
		   {	/* Mapped color, consult the map. */
			if ( gx_device_has_color(md) )
			   {	/* Map color */
				const byte *palette = md->palette.data;
				from = row + raster + raster;
				memcpy(from, row, raster);
				to = row;
				end = from + raster;
				while ( from < end )
				   {	register const byte *cp =
					  palette + (int)*from++ * 3;
					to[0] = cp[0];	/* red */
					to[1] = cp[1];	/* green */
					to[2] = cp[2];	/* blue */
					to += 3;
				   }
				count = raster * 3;
			   }
			else
			   {	/* Map gray scale */
				register const byte *palette =
				  md->palette.data;
				from = to = row, end = row + raster;
				while ( from < end )
				  *to++ = palette[(int)*from++ * 3];
				count = raster;
			   }
		   }
			break;
		case 32:
		   {	/* This case is different, because we must skip */
			/* every fourth byte. */
			from = to = row, end = row + raster;
			while ( from < end )
			   {	/* from[0] is unused */
				to[0] = from[1];	/* red */
				to[1] = from[2];	/* green */
				to[2] = from[3];	/* blue */
				from += 4;
				to += 3;
			   }
			count = to - row;
		   }
			break;
		default:
			count = raster;
		   }
		if ( fwrite(row, 1, count, file) < count )
		   {	code = e_ioerror;
			goto done;
		   }
	   }

done:	gs_free((char *)row, rsize, 1, "ppm file buffer");
	return code;
}
