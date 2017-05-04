
#ifndef __FAST_CU_HELPERP_H
#define __FAST_CU_HELPERP_H

#include "common.h"
#include "primitives.h"
#include "constants.h"

enum CPSIZE
{
	SIZE_4x4,
	SIZE_8x8,
	SIZE_16x16,
	SIZE_64x64,
};

enum INTRA_CUSPILIT
{
	INTRA_CU_SPLIT,
	INTRA_CU_NOT_SPLIT,
	INTRA_CU_OTHERS,
};

class fastCuHelper
{
public:
	fastCuHelper();
	~fastCuHelper();

	int loadFencCtu(pixel* src, uint16_t srcStride);
	int	UseFencCtu(pixel *src, uint16_t srcStride, int srcPart);//use the point
	INTRA_CUSPILIT cuSplitDecide(int cuSize,int partIdx,int qp);//partIdx: index of this CU first block in terms of 8x8 blocks.
	INTRA_CUSPILIT cuSplitDecideVar(int cuSize, int partIdx, int qp);

private:
	void  getCUPos8(int partIdx, int &x, int &y);
	void  calcVar(int cuSize, int partIdx);
	void  calcThres(int cuSize,int qp);
	int   calcSadCu( int cuSize, int partIdx);
	float sad_c(const pixel* pix1, intptr_t stride_pix1, const pixel* pix2, intptr_t stride_pix2, int width, int height);
	float var_c(const pixel  *fenc, int width, int height, int stride);
	x265::LumaPU  getMpartFromCusize(int cuSize);

	float testSubCu(int cuSize, int cuPosX, int cuPosY);
	
private:
	pixel   *buf;//buffer malloc 
	pixel   *bufFromFenc;//buffer used fenc //stride is FENC_STRIDE
	int		strideFenc;
	
	pixel   *ctuPicture;
	int		stride;
	int     mPart;

	bool    bUseBuffer;

	float   sadCu4Direction[4];//4 directions sad
	float   sadVar;//方差
	float   sadMean;//sad 均值
	float   thresLow;//阈值
	float   thresHigh;

	float   sadVarTest[4];//test
};

#endif