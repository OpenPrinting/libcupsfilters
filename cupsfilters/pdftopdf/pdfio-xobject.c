#include "pdfio-xobject-private.h"
#include "pdfio-tools-private.h"
#include "pdfio-pdftopdf-private.h"

pdfio_obj_t*
_cfPDFToPDFMakeXObject(pdfio_file_t *pdf, pdfio_obj_t *page)
{
	
	pdfio_dict_t *dict = pdfioDictCreate(pdf);
	pdfio_dict_t *pageDict = pdfioObjGetDict(page);

	pdfioDictSetName(dict, "Type", "XObject");
	pdfioDictSetName(dict, "Subtype", "Form");

	pdfio_rect_t box = _cfPDFToPDFGetTrimBox(page);
	pdfioDictSetRect(dict, "BBox", &box);

	_cfPDFToPDFMatrix mtx;
	_cfPDFToPDFMatrix_init(&mtx);
	if(pdfioDictGetNumber(pageDict, "UserUnit"))
	{
		double valUserUnit = pdfioDictGetNumber(pageDict, "UserUnit");
		_cfPDFToPDFMatrix_scale_uniform(&mtx, valUserUnit);
	}

	pdftopdf_rotation_e rot = _cfPDFToPDFGetRotate(page);

	_cfPDFToPDFPageRect bbox, tmp; 
	_cfPDFToPDFPageRect_init(&bbox);
	_cfPDFToPDFPageRect_init(&tmp);

	bbox = _cfPDFToPDFGetBoxAsRect(&box);

	tmp.left = 0;
	tmp.bottom = 0;
	tmp.right = 0;
	tmp.top = 0;

	_cfPDFToPDFPageRect_rotate_move(&tmp, rot, bbox.width, bbox.height);

	_cfPDFToPDFMatrix_translate(&mtx, tmp.left, tmp.bottom);
	_cfPDFToPDFMatrix_rotate_function(&mtx, rot);
	_cfPDFToPDFMatrix_translate(&mtx, -(bbox.left), -(bbox.bottom));

	pdfio_array_t *matrixArray = _cfPDFToPDFMatrix_get_array(&mtx);
	pdfioDictSetArray(dict, "Matrix", matrixArray);

	
	pdfio_dict_t *tempDict = pdfioDictGetDict(pageDict, "Resources");
	pdfioDictSetDict(dict, "Resources", tempDict);

	if(pdfioDictGetObj(pageDict, "Group"))
	{
		pdfio_obj_t *groupObj = pdfioDictGetObj(pageDict, "Group");
		pdfioDictSetObj(dict, "Group", groupObj);
	}
	
	pdfio_obj_t *returnObj = pdfioFileCreateObj(pdf, dict);

	return returnObj;

}
