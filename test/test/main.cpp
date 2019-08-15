#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2/xphoto/white_balance.hpp>
#include <highgui.h>
#include <iostream>
#include <stdio.h>

using namespace cv;
using namespace std;

#define WIDTH  1920
#define HEIGHT 1080

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

unsigned char*  pRawData;
unsigned short* pRawData_10;
IplImage*		pRgbDataInt8;
IplImage*       pYuvData_10;

int Read_File()
{
	printf("enter Read_File\n");
	char img[] = "a.raw";
	char* rawFileName = img;

	FILE *fp = NULL;

	int ret = 0;
	int width_10bit = WIDTH * 10 / 8;


	pRawData = (unsigned char*)malloc(width_10bit * HEIGHT * sizeof(unsigned char));//改为malloc在堆上分配内存

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
	ret = fread(pRawData, width_10bit * HEIGHT, 1, fp);//把raw数据读到malloc开辟的内存空间
	fclose(fp);

	if (1 != ret)
	{
		printf("Fail to fread data!\n");
		return -1;
	}
	printf("exit Read_File\n");
	return 0;
}

int Mipi_Raw_To_Raw10()
{
	printf("enter Mipi_Raw_To_Raw10\n");
	pRawData_10 = (unsigned short*)malloc(WIDTH * HEIGHT * sizeof(unsigned short));
	int j = 0;
	for (int i = 0; i < WIDTH * HEIGHT; i += 4)
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

	ret = fwrite(pRawData_10, 1, WIDTH * HEIGHT, fp_1);

	if (1 != ret)
	{
		printf("Fail to fwrite data!\n");
		return -1;
	}

	fclose(fp_1);
#endif // DEBUG

	Mat iMat(HEIGHT, WIDTH, CV_16UC1, pRawData_10);

	imwrite("a_new.bmp", iMat);


	Mat test = imread("a_new.bmp");
	imshow("test", test);
	waitKey(0);

	free(pRawData);
	printf("exit Mipi_Raw_To_Raw10\n");
	return 0;
}

int Raw10_To_Rgb()
{
	printf("enter Raw10_To_Rgb\n");
	IplImage *pBayerData_10 = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_16U, 1);
	IplImage *pRgbDataInt10 = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_16U, 3);
	pRgbDataInt8 = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	memcpy(pBayerData_10->imageData, (short *)pRawData_10, WIDTH*HEIGHT * sizeof(unsigned short));
	cvCvtColor(pBayerData_10, pRgbDataInt10, CV_BayerRG2BGR);

	/*将10bit数据转换为8bit*/
	cvConvertScale(pRgbDataInt10, pRgbDataInt8, 1, 0);


	cvReleaseImage(&pBayerData_10);
	cvNamedWindow("rgb_10", 1);
	cvShowImage("rgb_10", pRgbDataInt8);
	cvWaitKey(0);

	cvDestroyWindow("rgb_10");
	free(pRawData_10);
	printf("exit Raw10_To_Rgb\n");
	return 0;
}

unsigned char AWB_Mode = 1;
int AWB()
{
	printf("enter AWB  mode=%d\n", AWB_Mode);
	double R = 0, G = 0, B = 0;
	double KR, KG, KB;
	int TR, TG, TB;
	if (AWB_Mode == 0)
	{	
		for (int i = 0; i < HEIGHT; i++) {
			for (int j = 0; j < WIDTH; j++) {
				B += ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 0];
				G += ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 1];
				R += ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 2];
			}
		}
		B = B / (HEIGHT * WIDTH);
		G = G / (HEIGHT * WIDTH);
		R = R / (HEIGHT * WIDTH);
		printf("B = %f  G = %f R =%f\n", B, G, R);
		
		KB = (R + G + B) / (3 * B);
		KG = (R + G + B) / (3 * G);
		KR = (R + G + B) / (3 * R);

		printf("KB = %f  KG = %f KR =%f\n", KB, KG, KR);

		for (int i = 0; i < HEIGHT; i++) {
			for (int j = 0; j < WIDTH; j++) {
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
	}
	else if (AWB_Mode == 1)
	{
		CvScalar scalar;	//scalar
		for (int i = 0; i < HEIGHT; i++) {
			for (int j = 0; j < WIDTH; j++) {
				scalar = cvGet2D(pRgbDataInt8, i, j);	//获取像素点的RGB颜色分量
				B += scalar.val[0];
				G += scalar.val[1];
				R += scalar.val[2];
			}
		}

		B = B / (HEIGHT * WIDTH);
		G = G / (HEIGHT * WIDTH);
		R = R / (HEIGHT * WIDTH);

		printf("B = %f  G = %f R =%f\n", B, G, R);

		KB = (R + G + B) / (3 * B);
		KG = (R + G + B) / (3 * G);
		KR = (R + G + B) / (3 * R);
		printf("KB = %f  KG = %f KR =%f\n", KB, KG, KR);

		for (int i = 0; i < HEIGHT; i++) {
			for (int j = 0; j < WIDTH; j++) {
				scalar = cvGet2D(pRgbDataInt8, i, j);	//获取像素点的RGB颜色分量
				scalar.val[0] *= KB;
				scalar.val[1] *= KG;
				scalar.val[2] *= KR;

				scalar.val[0] = min(scalar.val[0], 255);
				scalar.val[1] = min(scalar.val[1], 255);
				scalar.val[2] = min(scalar.val[2], 255);
				cvSet2D(pRgbDataInt8, i, j, scalar);
			}
		}
	}
	else
	{
		IplImage *redImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 1);
		IplImage *greenImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 1);
		IplImage *blueImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 1);

		// RGB三通道分离
		cvSplit(pRgbDataInt8, blueImage, greenImage, redImage, NULL);

		for (int i = 0; i < HEIGHT; i++) {
			for (int j = 0; j < WIDTH; j++) {
				B += ((uchar *)(blueImage->imageData + i * blueImage->widthStep))[j];
				G += ((uchar *)(greenImage->imageData + i * greenImage->widthStep))[j];
				R += ((uchar *)(redImage->imageData + i * redImage->widthStep))[j];
			}
		}
		B = B / (HEIGHT * WIDTH);
		G = G / (HEIGHT * WIDTH);
		R = R / (HEIGHT * WIDTH);
		printf("B = %f  G = %f R =%f\n", B, G, R);

		double KR, KG, KB;
		KB = (R + G + B) / (3 * B);
		KG = (R + G + B) / (3 * G);
		KR = (R + G + B) / (3 * R);
		printf("KB = %f  KG = %f KR =%f\n", KB, KG, KR);

		int TR, TG, TB;

		for (int i = 0; i < HEIGHT; i++) {
			for (int j = 0; j < WIDTH; j++) {
				TB = ((uchar *)(blueImage->imageData + i * blueImage->widthStep))[j] * KB;
				TG = ((uchar *)(greenImage->imageData + i * greenImage->widthStep))[j] * KG;
				TR = ((uchar *)(redImage->imageData + i * redImage->widthStep))[j] * KR;
				TB = min(TB, 255);
				TG = min(TG, 255);
				TR = min(TR, 255);
				((uchar *)(blueImage->imageData + i * blueImage->widthStep))[j] = TB;
				((uchar *)(greenImage->imageData + i * greenImage->widthStep))[j] = TG;
				((uchar *)(redImage->imageData + i * redImage->widthStep))[j] = TR;
			}
		}
		//RGB三通道图像合并
		cvZero(pRgbDataInt8);
		cvMerge(blueImage, greenImage, redImage, NULL, pRgbDataInt8);

		cvReleaseImage(&blueImage);
		cvReleaseImage(&greenImage);
		cvReleaseImage(&redImage);
	}

	cvNamedWindow("rgb_awb", 1);
	cvShowImage("rgb_awb", pRgbDataInt8);
	cvWaitKey(0);

	cvDestroyWindow("rgb_awb");
	printf("exit AWB\n");
	return 0;
}

int Rgb_To_Yuv()
{
	printf("enter Rgb_To_Yuv\n");
	pYuvData_10 = cvCreateImage(cvSize(WIDTH, HEIGHT), 8, 3);
	cvCvtColor(pRgbDataInt8, pYuvData_10, CV_BGR2YUV);

	cvNamedWindow("yuv_10", 1);
	cvShowImage("yuv_10", pYuvData_10);
	cvWaitKey(0);

	cvDestroyWindow("yuv_10");
	printf("exit Rgb_To_Yuv\n");
	return 0;
}

int End_Process()
{
	cvReleaseImage(&pRgbDataInt8);
	cvReleaseImage(&pYuvData_10);

	return 0;
}


int ISP_PROCESS()
{
	Read_File();

	Mipi_Raw_To_Raw10();

	Raw10_To_Rgb();

	AWB();

	Rgb_To_Yuv();

	End_Process();

	return 0;
}


int main() 
{
	ISP_PROCESS();
	return 0;
}