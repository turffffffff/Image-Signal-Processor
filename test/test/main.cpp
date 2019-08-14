#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2/xphoto/white_balance.hpp>
#include <highgui.h>
#include <iostream>
#include <stdio.h>

using namespace cv;
using namespace std;

int min(int a, int b)
{
	if (a > b) 
	{
		return b;
	}
	else
	{
		return a;
	}
}

//#define SAVE_RAW
#define AWB_1
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

	IplImage *pBayerData_10 = cvCreateImage(cvSize(width, height), IPL_DEPTH_16U, 1);
	IplImage *pRgbDataInt10 = cvCreateImage(cvSize(width, height), IPL_DEPTH_16U, 3);
	IplImage *pRgbDataInt8 = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
	memcpy(pBayerData_10->imageData, (short *)pRawData_10, width*height * sizeof(unsigned short));
	cvCvtColor(pBayerData_10, pRgbDataInt10, CV_BayerRG2BGR);

	/*将10bit数据转换为8bit*/
	cvConvertScale(pRgbDataInt10, pRgbDataInt8, 1, 0);

	cvNamedWindow("rgb_10", 1);
	cvShowImage("rgb_10", pRgbDataInt8);
	cvWaitKey(0);

	double R = 0, G = 0, B = 0;

#ifdef SPLIT

	IplImage *redImage = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
	IplImage *greenImage = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
	IplImage *blueImage = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);

	// RGB三通道分离
	cvSplit(pRgbDataInt8, blueImage, greenImage, redImage, NULL);

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			B += ((uchar *)(blueImage->imageData + i * blueImage->widthStep))[j];
			G += ((uchar *)(greenImage->imageData + i * greenImage->widthStep))[j];
			R += ((uchar *)(redImage->imageData + i * redImage->widthStep))[j];
		}
	}
	B = B / (height * width);
	G = G / (height * width);
	R = R / (height * width);
	printf("1  B = %d  G = %d R =%d\n", B, G, R);

	double KR, KG, KB;
	KB = (R + G + B) / (3 * B);
	KG = (R + G + B) / (3 * G);
	KR = (R + G + B) / (3 * R);

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			((uchar *)(blueImage->imageData + i * blueImage->widthStep))[j] *= KB;
			((uchar *)(greenImage->imageData + i * greenImage->widthStep))[j] *= KG;
			((uchar *)(redImage->imageData + i * redImage->widthStep))[j] *= KR;
			MIN(((uchar *)(blueImage->imageData + i * blueImage->widthStep))[j], 255);
			MIN(((uchar *)(greenImage->imageData + i * greenImage->widthStep))[j], 255);
			MIN(((uchar *)(redImage->imageData + i * redImage->widthStep))[j], 255);
		}
	}
	//RGB三通道图像合并
	cvMerge(blueImage, greenImage, redImage, NULL, pRgbDataInt8);

	cvNamedWindow("rgb_awb", 1);
	cvShowImage("rgb_awb", pRgbDataInt8);
	cvWaitKey(0);
#endif

#ifdef AWB_1
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			B += ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 0];
			G += ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 1];
			R += ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 2];
		}
	}
	B = B / (height * width);
	G = G / (height * width);
	R = R / (height * width);

	double KR, KG, KB;
	KB = (R + G + B) / (3 * B);
	KG = (R + G + B) / (3 * G);
	KR = (R + G + B) / (3 * R);

	printf("1  KB = %f  KG = %f KR =%f\n", KB, KG, KR);
	int TR, TG, TB;

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			TB = ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 0] * KB;
			TG = ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 1] * KG;
			TR = ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 2] * KR;
			TB = min(TB, 255);
			TG = min(TG, 255);
			TR = min(TR, 255);
			
			((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 0] = (uchar)TB;
			((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 1] = (uchar)TG;
			((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 2] = (uchar)TR;
		}
	}

	cvNamedWindow("rgb_awb", 1);
	cvShowImage("rgb_awb", pRgbDataInt8);
	cvWaitKey(0);

#else

	CvScalar scalar;	//scalar
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			scalar = cvGet2D(pRgbDataInt8, i, j);	//获取像素点的RGB颜色分量
			if (i == 1 && j == 1) {
				printf("3   B = %f  G = %f R =%f\n", scalar.val[0], scalar.val[1], scalar.val[2]);
			}
			B += scalar.val[0];
			G += scalar.val[1];
			R += scalar.val[2];
		}
	}
	printf("4   B = %d  G = %d R =%d\n", ((uchar *)(pRgbDataInt8->imageData + 1 * pRgbDataInt8->widthStep))[1 * pRgbDataInt8->nChannels + 0], scalar.val[1], scalar.val[2]);

	B = B / (height * width);
	G = G / (height * width);
	R = R / (height * width);

	printf("5   B = %d  G = %d R =%d\n", B, G, R);
	//需要调整的RGB分量的增益

	double KR, KG, KB;
	KB = (R + G + B) / (3 * B);
	KG = (R + G + B) / (3 * G);
	KR = (R + G + B) / (3 * R);
	printf("1  KB = %f  KG = %f KR =%f\n", KB, KG, KR);

	//调整RGB三个通道各自的值
	IplImage *pRgbDataInt8awb = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			scalar = cvGet2D(pRgbDataInt8, i, j);	//获取像素点的RGB颜色分量
			if (i == 1 && j == 1) {
				printf("3   B = %f  G = %f R =%f\n", scalar.val[0], scalar.val[1], scalar.val[2]);
			}
			scalar.val[0] *= KB;
			scalar.val[1] *= KG;
			scalar.val[2] *= KR;

			scalar.val[0] = min(scalar.val[0], 255);
			scalar.val[1] = min(scalar.val[1], 255);
			scalar.val[2] = min(scalar.val[2], 255);
			if (i == 1 && j == 1) {
				printf("3   B = %f  G = %f R =%f\n", scalar.val[0], scalar.val[1], scalar.val[2]);
			}
			cvSet2D(pRgbDataInt8awb, i, j, scalar);
		}
	}

	cvNamedWindow("rgb_awb", 1);
	cvShowImage("rgb_awb", pRgbDataInt8awb);
	cvWaitKey(0);

#endif

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