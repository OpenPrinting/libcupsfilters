//
//
//
//
#include "pdfio-pdftopdf-private.h"
#include "pdfio-tools-private.h"
#include "cupsfilters/debug-internal.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <pdfio.h>

_cfPDFToPDFPageRect
_cfPDFToPDFGetBoxAsRect(pdfio_rect_t *box)
{
	_cfPDFToPDFPageRect ret;

	ret.left = box->x1;
	ret.bottom = box->y1;
	ret.right = box->x2;
	ret.top = box->y2;

	ret.width = ret.right - ret.left;
	ret.height = ret.top - ret.bottom;

	return ret;
}

pdfio_rect_t
_cfPDFToPDFGetRectAsBox(const _cfPDFToPDFPageRect *rect)
{
	return (_cfPDFToPDFMakeBox(rect->left, rect->bottom, rect->right, rect->top));
}


pdftopdf_rotation_e
_cfPDFToPDFGetRotate(pdfio_obj_t *page)
{
	pdfio_dict_t *page_dict = pdfioObjGetDict(page);
        if(!pdfioDictGetNumber(page_dict, "Rotate"))
               return (ROT_0);
        double rot = pdfioDictGetNumber(page_dict, "Rotate");

	rot = fmod(rot, 360.0);
	if (rot<0)
		rot += 360.0;
	else if (rot == 90.0)
		return (ROT_270);
	else if (rot == 180.0)
		return (ROT_180);
	else if (rot == 270.0)
		return (ROT_90);
	else if (rot != 0.0)
	{
		char str[100]; 
    		sprintf(str, "%f", rot); 
		printf("Unexpected /Rotate value: %s\n", str); 
		return -1;
	}
	return (ROT_0);
}

int 
_cfPDFToPDFMakeRotate(pdftopdf_rotation_e rot)
{
	switch(rot)
	{
		case ROT_0:
			return 0;

		case ROT_90:
			return 270;

		case ROT_180:
			return 180;

		case ROT_270:
			return 90;

		default:
			printf("Bad Rotation\n");
			return -1;
	}
	

}

double 
_cfPDFToPDFGetUserUnit(pdfio_obj_t *page)
{
	pdfio_dict_t *page_dict = pdfioObjGetDict(page);
        if(!pdfioDictGetNumber(page_dict, "UserUnit"))
                return 1.0;

        int userUnit_value = pdfioDictGetNumber(page_dict, "UserUnit");
	return userUnit_value;
}

void 
_cfPDFToPDFMatrix_init(_cfPDFToPDFMatrix *matrix) 
{
  matrix->ctm[0] = 1;
  matrix->ctm[1] = 0;
  matrix->ctm[2] = 0;
  matrix->ctm[3] = 1;
  matrix->ctm[4] = 0;
  matrix->ctm[5] = 0;
}

void 
_cfPDFToPDFMatrix_init_with_handle(_cfPDFToPDFMatrix *matrix,
				   pdfio_array_t *ar)
{
	if( pdfioArrayGetSize(ar) != 6)
		printf("Not a ctm Matrix\n");
	for (int iA = 0; iA<6 ; iA++)
		matrix->ctm[iA] = pdfioArrayGetNumber(ar, iA);
}

_cfPDFToPDFMatrix
_cfPDFToPDFMatrix_rotate_function(_cfPDFToPDFMatrix *matrix,
				  pdftopdf_rotation_e rot)
{
	switch (rot)
	{
		case ROT_0:
			break;
		case ROT_90:
			double temp = matrix->ctm[0];
			matrix->ctm[0] = matrix->ctm[2];
			matrix->ctm[2] = temp;

			temp = matrix->ctm[1];
			matrix->ctm[1] = matrix->ctm[3];
			matrix->ctm[3] = temp;
			
			matrix->ctm[2] = -(matrix->ctm[2]);
			matrix->ctm[3] = -(matrix->ctm[3]);
			break;

		case ROT_180:
			matrix->ctm[0] = -(matrix->ctm[0]);
			matrix->ctm[3] = -(matrix->ctm[3]);
			break;

		case ROT_270:
			temp = matrix->ctm[0];
			matrix->ctm[0] = matrix->ctm[2];
			matrix->ctm[2] = temp;

			temp = matrix->ctm[1];
			matrix->ctm[1] = matrix->ctm[3];
			matrix->ctm[3] = temp;
			
			matrix->ctm[0] = -(matrix->ctm[0]);
			matrix->ctm[1] = -(matrix->ctm[1]);
			break;

		default:
			DEBUG_assert(0);		
	}
	return *matrix;   //right now we don't know the full extent on this function,
			 //can even return the funtion as void.
}

_cfPDFToPDFMatrix 
_cfPDFToPDFMatrix_rotate_move(_cfPDFToPDFMatrix *matrix, 
			      pdftopdf_rotation_e rot, 
			      double width, 
			      double height)
{
	_cfPDFToPDFMatrix_rotate_function(matrix, rot);
	switch(rot)
	{
		case ROT_0:
			break;
		case ROT_90:
			_cfPDFToPDFMatrix_translate(matrix, width, 0);
			break;
		case ROT_180:
			_cfPDFToPDFMatrix_translate(matrix, width, height);
			break;
		case ROT_270:
			_cfPDFToPDFMatrix_translate(matrix, 0, height);
			break;
	}
	return (*matrix); //right now don't know the full scope of this function,
			  //can even return it as void
}

_cfPDFToPDFMatrix 
_cfPDFToPDFMatrix_rotate_rad(_cfPDFToPDFMatrix *matrix, 
			     double rad)
{
	_cfPDFToPDFMatrix tmp;
	_cfPDFToPDFMatrix multiplied;
	
	tmp.ctm[0] = cos(rad);
	tmp.ctm[1] = sin(rad);
	tmp.ctm[2] = -sin(rad);
	tmp.ctm[3] = cos(rad);

	multiplied = _cfPDFToPDFMatrix_multiply(matrix,
						&tmp);
	return multiplied;
}


_cfPDFToPDFMatrix 
_cfPDFToPDFMatrix_translate(_cfPDFToPDFMatrix *matrix,
			    double tx,
			    double ty)
{
	matrix->ctm[4] +=  matrix->ctm[0] * tx + matrix->ctm[2] * ty;
	matrix->ctm[5] += matrix->ctm[1] * tx + matrix->ctm[3] * ty;
	return *matrix;
}

_cfPDFToPDFMatrix 
_cfPDFToPDFMatrix_scale(_cfPDFToPDFMatrix *matrix, 
			double sx,
			double sy)
{
	matrix->ctm[0] *= sx;
	matrix->ctm[1] *= sx;
	matrix->ctm[2] *= sy;
	matrix->ctm[3] *= sy;

	return *matrix;
}

_cfPDFToPDFMatrix 
_cfPDFToPDFMatrix_scale_uniform(_cfPDFToPDFMatrix *matrix, 
				double s)
{
	return (_cfPDFToPDFMatrix_scale(matrix, s, s));
}

_cfPDFToPDFMatrix 
_cfPDFToPDFMatrix_multiply(_cfPDFToPDFMatrix *matrix, 
			   const _cfPDFToPDFMatrix *rhs)
{
	double tmp[6];
	
	for (int i = 0; i < 6; ++i) 
	{
        	tmp[i] = matrix->ctm[i];
    	}

	matrix->ctm[0] = tmp[0] * rhs->ctm[0] + tmp[2] * rhs->ctm[1];
	matrix->ctm[1] = tmp[1] * rhs->ctm[0] + tmp[3] * rhs->ctm[1];
	
	matrix->ctm[2] = tmp[0] * rhs->ctm[2] + tmp[2] * rhs->ctm[3];
	matrix->ctm[3] = tmp[1] * rhs->ctm[2] + tmp[3] * rhs->ctm[3];
	
	matrix->ctm[4] = tmp[0] * rhs->ctm[4] + tmp[2] * rhs->ctm[4] + tmp[4];
	matrix->ctm[5] = tmp[1] * rhs->ctm[4] + tmp[3] * rhs->ctm[5] + tmp[5];

	return *matrix;
}

pdfio_array_t *_cfPDFToPDFMatrix_get_array(const _cfPDFToPDFMatrix *matrix)
{
	pdfio_file_t *pdf = pdfioFileCreate("example.pdf", NULL, NULL, NULL, NULL, NULL);
	pdfio_array_t *ret = pdfioArrayCreate(pdf);

	pdfioArrayAppendNumber(ret, matrix->ctm[0]);
	pdfioArrayAppendNumber(ret, matrix->ctm[1]);
	pdfioArrayAppendNumber(ret, matrix->ctm[2]);
	pdfioArrayAppendNumber(ret, matrix->ctm[3]);
	pdfioArrayAppendNumber(ret, matrix->ctm[4]);
	pdfioArrayAppendNumber(ret, matrix->ctm[5]);

	pdfioFileClose(pdf);	
	return (ret);
}

char 
_cfPDFToPDFMatrix_get_string(const _cfPDFToPDFMatrix *matrix)
{
 	// Helper function to convert double to string
    	void double_to_string(double value, char *str, size_t size) 
	{
        	snprintf(str, size, "%f", value);
    	}

    	// Assuming each double string will not exceed 32 characters including the null terminator
    	char buffer[32];
    	size_t total_size = 32 * 6 + 5; // 6 doubles and 5 spaces
    	char *ret = (char*)malloc(total_size);
    
    	ret[0] = '\0'; // Initialize the string

    	for (int i = 0; i < 6; ++i) 
	{
        	double_to_string(matrix->ctm[i], buffer, sizeof(buffer));
        	strcat(ret, buffer);
        	if (i < 5) 
		{
            		strcat(ret, " ");
        	}
    	}

    	return *ret;
}
