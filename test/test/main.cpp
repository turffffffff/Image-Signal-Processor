#include<opencv2\opencv.hpp>
#include <highgui.h>
#include <iostream>
#include<stdio.h>

using namespace cv;
using namespace std;

//#define SAVE_RAW
int ISP_PROCESS()
{
/* ============================================================================================================== */
/* =============================================read file======================================================== */
/* ============================================================================================================== */
	char img[] = "a.raw";
	char* rawFileName = img;

	FILE *fp = NULL;

	int ret = 0;
	int width = 1920;
	int width_10bit = width * 10 / 8;
	int height = 1080;


	unsigned char* pRawData = (unsigned char*)malloc(width_10bit * height * sizeof(unsigned char));//改为malloc在堆上分配内存

	if (NULL == pRawData)
	{
		printf("Fail to calloc pRawData!\n");
		return -1;
	}

	fp = fopen(rawFileName, "rb");

	if (NULL == fp)
	{
		printf("Fail to read %s!\n", rawFileName);
		return -1;
	}
	ret = fread(pRawData, width_10bit * height, 1, fp);//把raw数据读到malloc开辟的内存空间
	fclose(fp);

	if (1 != ret)
	{
		printf("Fail to fread data!\n");
		return -1;
	}
/* ============================================================================================================== */
/* =========================================mipi raw to raw10==================================================== */
/* ============================================================================================================== */

	unsigned short* pRawData_10 = (unsigned short*)malloc(width * height * sizeof(unsigned short));
	int j = 0;
	for (int i = 0 ; i < width * height; i+=4)
	{
		pRawData_10[i + 0] = (pRawData[j + 0] << 2) + ((pRawData[j + 4] >> 0) & 0x3);
		pRawData_10[i + 1] = (pRawData[j + 1] << 2) + ((pRawData[j + 4] >> 2) & 0x3);
		pRawData_10[i + 2] = (pRawData[j + 2] << 2) + ((pRawData[j + 4] >> 4) & 0x3);
		pRawData_10[i + 3] = (pRawData[j + 3] << 2) + ((pRawData[j + 4] >> 6) & 0x3);
		j += 5;
	}

#ifdef SAVE_RAW

	char img_new[] = "a_new.raw";
	char* new_rawFileName = img_new;

	FILE *fp_1 = NULL;
	fp_1 = fopen(new_rawFileName, "wb");

	ret = fwrite(pRawData_10, 1, width * height, fp_1);

	if (1 != ret)
	{
		printf("Fail to fwrite data!\n");
		return -1;
	}

	fclose(fp_1);
#endif // DEBUG

	Mat iMat(height, width, CV_16UC1, pRawData_10);

	imwrite("a_new.bmp", iMat);


	Mat test = imread("a_new.bmp");
	imshow("test", test);
	waitKey(0);

/* ============================================================================================================== */
/* =============================================raw 10 to rgb==================================================== */
/* ============================================================================================================== */

	IplImage *pBayerData_10 = cvCreateImage(cvSize(width, height), 16, 1);
	IplImage *pRgbDataInt10 = cvCreateImage(cvSize(width, height), 16, 3);
	IplImage *pRgbDataInt8 = cvCreateImage(cvSize(width, height), 8, 3);
	memcpy(pBayerData_10->imageData, (short *)pRawData_10, width*height * sizeof(unsigned short));
	cvCvtColor(pBayerData_10, pRgbDataInt10, CV_BayerRG2BGR);

	/*将10bit数据转换为8bit*/
	cvConvertScale(pRgbDataInt10, pRgbDataInt8, 1, 0);

	cvNamedWindow("rgb_10", 1);
	cvShowImage("rgb_10", pRgbDataInt8);
	cvWaitKey(0);

/* ============================================================================================================== */
/* ==============================================rgb to yuv====================================================== */
/* ============================================================================================================== */

	IplImage *pYuvData_10 = cvCreateImage(cvSize(width, height), 8, 3);
	cvCvtColor(pRgbDataInt8, pYuvData_10, CV_BGR2YUV);

	cvNamedWindow("yuv_10", 1);
	cvShowImage("yuv_10", pYuvData_10);
	cvWaitKey(0);



	free(pRawData);
	fclose(fp);
	cvDestroyWindow("rgb_10");
	cvDestroyWindow("yuv_10");
	cvReleaseImage(&pBayerData_10);
	cvReleaseImage(&pRgbDataInt10);
	cvReleaseImage(&pYuvData_10);

	free(pRawData);//释放malloc开辟的内存
	free(pRawData_10);//释放malloc开辟的内存

	return 0;
}


int main() 
{
	ISP_PROCESS();
	return 0;
}