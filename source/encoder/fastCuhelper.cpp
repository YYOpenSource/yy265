
#include "fastCuhelper.h"
#include <stdio.h>
#include <stdlib.h>
#include "primitives.h"
#include "constants.h"

#define FAST_CU_DEBUG_OPEN 0

#define INTRA_CTU_BUFFER_SIZE 66
#define FT_CTU_SIZE 64

#define GET_ADDR_OFFSET(addr,x,y) (addr + (y)*(INTRA_CTU_BUFFER_SIZE) +(x))
#define GET_ADDR_OFFSET_STRIDE(addr,x,y,stride)  (addr + (y)*(stride) +(x))


//for test
#define TEST_CU_32 0
#define CHECK_CU32_SAD 0

fastCuHelper::fastCuHelper()
{
	buf  = bufFromFenc = NULL;
	ctuPicture = NULL;

	stride = 0;
	strideFenc = 0;
	mPart = -1;
	bUseBuffer = false;

	sadVar = -1;
	sadMean = -1;
	thresLow = thresHigh = 0;
	for (int i = 0; i < 4; i ++)
		sadCu4Direction[i] = 0;
}

fastCuHelper::~fastCuHelper()
{
	if (buf)
	{
		X265_NS::x265_free(buf);
		buf = NULL;
	}
	bufFromFenc = NULL;
	ctuPicture = NULL;
}

int fastCuHelper::loadFencCtu(pixel *src, uint16_t srcStride)
{
	//malloc 
	if ( buf == NULL)
	{
		buf = (pixel *)X265_NS::x265_malloc(sizeof(pixel) * INTRA_CTU_BUFFER_SIZE * INTRA_CTU_BUFFER_SIZE );
		if (!buf)
			return -1;
	}
	ctuPicture = GET_ADDR_OFFSET(buf, 1, 1);

	//load fenc and padding 
	X265_NS::primitives.cu[X265_NS::LUMA_64x64].copy_pp(ctuPicture, INTRA_CTU_BUFFER_SIZE, src, srcStride);

	memcpy(GET_ADDR_OFFSET(ctuPicture, 0, -1),
		GET_ADDR_OFFSET(ctuPicture, 0, 0),
			FENC_STRIDE);
	memcpy(GET_ADDR_OFFSET(ctuPicture, 0, FT_CTU_SIZE),
		GET_ADDR_OFFSET(ctuPicture, 0, (FT_CTU_SIZE - 1)/*63*/),
		    FENC_STRIDE);
	for (int i = 0; i < INTRA_CTU_BUFFER_SIZE; i++)
	{
		buf[i*INTRA_CTU_BUFFER_SIZE] = buf[i*INTRA_CTU_BUFFER_SIZE +1];
		buf[i*INTRA_CTU_BUFFER_SIZE + (INTRA_CTU_BUFFER_SIZE - 1)/*65*/] = buf[i*INTRA_CTU_BUFFER_SIZE + (INTRA_CTU_BUFFER_SIZE - 2)/*64*/];
	}

	stride = INTRA_CTU_BUFFER_SIZE; 
	bufFromFenc = src;
	strideFenc = FENC_STRIDE;

#if TEST_CU_32
	/*test use ctu src */
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++)
			sadVarTest[j * 2 + i] = testSubCu(32, i, j);

	//for (int i = 0; i < 4; i++)
	//	for (int j = 0; j < 4; j++)
	//		testSubCu(16, i, j);

	//for (int i = 0; i < 8; i++)
	//	for (int j = 0; j < 8; j++)
	//		testSubCu(8, i, j);
#endif

	return 0;
}

float  fastCuHelper::testSubCu(int cuSize,int cuPosX, int cuPosY)
{
	pixel *pStart = bufFromFenc + (cuPosY*cuSize*strideFenc) + cuPosX*cuSize;
	float   sadCtu[4];
	float sadV = 0;

	sadCtu[0] = sad_c(pStart, strideFenc, pStart + 1, strideFenc, cuSize - 1, cuSize);
	sadCtu[1] = sad_c(pStart, strideFenc, pStart + strideFenc, strideFenc, cuSize, cuSize - 1);
	sadCtu[2] = sad_c(pStart, strideFenc, pStart + strideFenc + 1, strideFenc, cuSize - 1, cuSize - 1);
	sadCtu[3] = sad_c(pStart + 1, strideFenc, pStart + strideFenc, strideFenc, cuSize - 1, cuSize - 1);

	float sadAvg = ((sadCtu[0] + sadCtu[1] + sadCtu[2] + sadCtu[3])) / 4;
	for (int i = 0; i < 4; i++)
		sadV += (sadCtu[i] - sadAvg)*(sadCtu[i] - sadAvg);

	sadV = sadV / 4;

	float thresL = ((float)(37 * cuSize + 400)) / 2048;
	//thresLow = ((float)(qp * cuSize + 0)) / 3048;
	float thresH = ((float)(37 * cuSize + 9000)) / 2048;

//#if  (!CHECK_CU32_SAD)
//	if (sadV < thresL)
//	{
//		for (int i = 2; i < cuSize - 2; i++)
//			for (int j = 2; j < cuSize - 2; j++)
//				pStart[i + j*strideFenc] = 255;
//	}
//	
//	if (sadV > thresH)
//	{
//		for (int i = 2; i < cuSize - 2; i++)
//			for (int j = 2; j < cuSize - 2; j++)
//				pStart[i + j*strideFenc] = 0;
//	}
//#endif
	return sadV;
}

int fastCuHelper::UseFencCtu(pixel *src, uint16_t srcStride,int srcPart)
{
	bufFromFenc = src;
	strideFenc = FENC_STRIDE;
	stride = srcStride;
	mPart = srcPart;

	ctuPicture = src;
	bUseBuffer = true;

#if TEST_CU_32
	/*test use ctu src */
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++)
			sadVarTest[j*2+i] = testSubCu(32, i, j);

	//for (int i = 0; i < 4; i++)
	//	for (int j = 0; j < 4; j++)
	//		testSubCu(16, i, j);

	//for (int i = 0; i < 8; i++)
	//	for (int j = 0; j < 8; j++)
	//		testSubCu(8, i, j);
#endif

	return 0;
}


void fastCuHelper::getCUPos8(int partIdx, int &x, int &y)
{
	int partIdx32 = partIdx / 16;//32x32 idx
	int pos32X = partIdx32 % 2;
	int pox32Y = partIdx32 / 2;

	int partIdx16 = (partIdx % 16) / 4;//sub 16x16 idx
	int pos16X = partIdx16 % 2;
	int pos16Y = partIdx16 / 2;

	int partIdx8 = (partIdx % 4);
	int pos8X = partIdx8 % 2;
	int pos8Y = partIdx8 / 2;

	x = pos32X * 32 + pos16X * 16 + pos8X * 8;
	y = pox32Y * 32 + pos16Y * 16 + pos8Y * 8;

}

float fastCuHelper::var_c(const pixel  *fenc, int width,int height, int stride)
{
	int64_t sum = 0;
	int64_t squ_sum = 0;
	for (int w = 0; w < width; w++) {
		for (int h = 0; h < height; h++){
			squ_sum += (fenc[w*stride + h] * fenc[w*stride + h]);
			sum += fenc[w*stride + h];
		}
	}

	float var = (squ_sum - ((sum * sum) / width / height));
	return (var / width / height);
}

float fastCuHelper::sad_c(const pixel* pix1, intptr_t stride_pix1, const pixel* pix2, intptr_t stride_pix2, int width, int height)
{
	float sum = 0;

	for (int y = 0; y <  height; y++)
	{
		for (int x = 0; x <  width; x++)
			sum += abs(pix1[x] - pix2[x]);

		pix1 += stride_pix1;
		pix2 += stride_pix2;
	}

	sum = sum/width/height;
	return sum;
}

int fastCuHelper::calcSadCu(int cuSize, int partIdx)
{
	int offSetX, offsetY;
	pixel *pixelCuStart;
	pixel *pixelCuStartFenc;

	getCUPos8(partIdx, offSetX, offsetY);
	pixelCuStart = ctuPicture + (offsetY*stride) + offSetX;
	pixelCuStartFenc = bufFromFenc + (offsetY*FENC_STRIDE) + offSetX;

	X265_NS::LumaPU mpart = getMpartFromCusize(cuSize);
	
	if (bUseBuffer)
	{
		sadCu4Direction[0] = sad_c(pixelCuStart, stride, pixelCuStart + 1, stride, cuSize-1 ,cuSize);
		sadCu4Direction[1] = sad_c(pixelCuStart, stride, pixelCuStart + stride, stride,cuSize , cuSize -1);
		sadCu4Direction[2] = sad_c(pixelCuStart, stride, pixelCuStart + stride +1, stride, cuSize -1, cuSize - 1);
		sadCu4Direction[3] = sad_c(pixelCuStart+1, stride, pixelCuStart + stride, stride,cuSize - 1, cuSize - 1);
	}
	else
	{
		ALIGN_VAR_32(int32_t, sad[4]);
		pixel *ref1 = GET_ADDR_OFFSET(pixelCuStart, 1, 0);
		pixel *ref2 = GET_ADDR_OFFSET(pixelCuStart, 0, 1);
		pixel *ref3 = GET_ADDR_OFFSET(pixelCuStart, 1, 1);
		pixel *ref4 = GET_ADDR_OFFSET(pixelCuStart, 1, -1);

		x265::primitives.pu[mpart].sad_x4(
			pixelCuStartFenc,
			ref1,
			ref2,
			ref3,
			ref4,
			INTRA_CTU_BUFFER_SIZE,
			sad);

		for (int i = 0; i < 4; i++)
		{
			sadCu4Direction[i] = ((float)sad[i]) / cuSize / cuSize;
		}
	}

	return 0;
}
X265_NS::LumaPU fastCuHelper::getMpartFromCusize(int cuSize)
{
	X265_NS::LumaPU ret;
	switch (cuSize)
	{
		case 64:
			ret = X265_NS::LUMA_64x64;
			break;
		case 32:
			ret = X265_NS::LUMA_32x32;
			break;
		case 16:
			ret = X265_NS::LUMA_16x16;
			break;
		case 8:
			ret = X265_NS::LUMA_8x8;
			break;
		default:
			ret = X265_NS::LUMA_8x8;
			break;
	}
	return ret;
}
void fastCuHelper::calcThres(int cuSize, int qp)
{
	thresLow = ((float)(qp * cuSize + 400)) / 2048;
	//thresLow = ((float)(qp * cuSize + 0)) / 3048;
	thresHigh = ((float)(qp * cuSize + 9000)) / 2048;
}

void fastCuHelper::calcVar(int cuSize, int partIdx)
{
	float sadAvg = ((sadCu4Direction[0] + sadCu4Direction[1] + sadCu4Direction[2] + sadCu4Direction[3])) / 4;

	sadVar = 0;
	for (int i = 0; i < 4; i++)
		sadVar += (sadCu4Direction[i] - sadAvg)*(sadCu4Direction[i] - sadAvg);

	sadVar = sadVar / 4;
	sadMean = sadAvg;
}


INTRA_CUSPILIT fastCuHelper::cuSplitDecide(int cuSize, int partIdx, int qp)
{
	INTRA_CUSPILIT ret = INTRA_CU_OTHERS;

	//if (cuSize == 32)
	//	cuSize = cuSize;

	calcSadCu(cuSize, partIdx );
	calcVar(cuSize , partIdx);
	calcThres(cuSize ,qp);

	#if TEST_CU_32
	#if CHECK_CU32_SAD
	if (cuSize == 32)
	{
		int partIdx32 = partIdx / 16;//32x32 idx
		int pos32X = partIdx32 % 2;
		int pox32Y = partIdx32 / 2;
		assert (sadVarTest[pox32Y * 2 + pos32X] == sadVar);
	}
	#endif
	#endif

	if (sadVar < 0)
		return ret;

	if (sadVar < thresLow  && sadMean < 2)
		ret = INTRA_CU_NOT_SPLIT /*INTRA_CU_SPLIT*/; //INTRA_CU_NOT_SPLIT
	else if (sadVar > thresHigh /*1000*/ /*|| sadMean > 5*/)
		ret = INTRA_CU_SPLIT /*INTRA_CU_NOT_SPLIT*/; //INTRA_CU_NOT_SPLIT
	else
		ret = INTRA_CU_OTHERS;

#if FAST_CU_DEBUG_OPEN
	printf("cusize %d, partIDx %d, qp %d,var %f,thred %f,%f,ret %d \n", cuSize, partIdx, qp, var, thresLow, thresHigh, ret);
#endif
	return ret;
}

INTRA_CUSPILIT fastCuHelper::cuSplitDecideVar(int cuSize, int partIdx, int qp)
{

	INTRA_CUSPILIT ret = INTRA_CU_OTHERS;

	int offSetX, offsetY;
	pixel *pixelCuStart;
	pixel *pixelCuStartFenc;
	float varCu;

	if (cuSize == 32)
		cuSize = cuSize;

	getCUPos8(partIdx, offSetX, offsetY);
	pixelCuStart = ctuPicture + (offsetY*stride) + offSetX;

	varCu = var_c(pixelCuStart, cuSize, cuSize, stride);

	float threLow = 70.0 - (36 - cuSize)/2;
	float threHeight = 2000.0 - (36 - cuSize) * 12;
	if (varCu < threLow)
		ret = INTRA_CU_NOT_SPLIT /*INTRA_CU_SPLIT*/;
	//else if (varCu > threLow && cuSize == 32)
	//	ret = INTRA_CU_SPLIT;

	return ret;
}