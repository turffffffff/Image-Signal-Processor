#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2/xphoto/white_balance.hpp>
#include <highgui.h>
#include <iostream>
#include <stdio.h>

using namespace cv;
using namespace std;

//#define SAVE_RAW
#define SAVE_YUV

#define WIDTH  1920
#define HEIGHT 1080

#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#define clip(x,a,b)  (MAX(a,MIN(x,b)))
#define CLIP          clip

unsigned char*  pRawData;
unsigned short* pRawData_10;
IplImage*		pRgbDataInt8;
IplImage*       pYuYvData_8;
IplImage*		pBayerData_10;

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

	int ret;
	FILE *fp_1 = NULL;
	fp_1 = fopen(new_rawFileName, "wb");

	ret = fwrite(pRawData_10, 1, WIDTH * HEIGHT * sizeof(unsigned short), fp_1);

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

int DeadPixelDetect(int src0, int src1, int src2, int src3, int src4, int src5)
{
	int ValueSrc0 = src0;
	int ValueSrc1 = src2;
	int ValueSrc2 = src3 - src1;
	int ValueSrc3 = src5 - src3;
	int ValueSrc4 = src4;

	int Coef3 = (-4 * ValueSrc0 + 4 * ValueSrc1 - 6 * ValueSrc2 + 2 * ValueSrc3) * 7 >> 9;
	int Coef2 = (36 * ValueSrc0 - 36 * ValueSrc1 + 45 * ValueSrc2 - 9 * ValueSrc3) * 7 >> 9;
	int Coef1 = (-92 * ValueSrc0 + 92 * ValueSrc1 - 66 * ValueSrc2 + 10 * ValueSrc3) * 7 >> 9;

	int ValueEstimate1 = 8 * Coef3 + 4 * Coef2 + 2 * Coef1 + ValueSrc0;
	int ValueEstimate2 = 26 * Coef3 + 8 * Coef2 + 2 * Coef1;
	int ValueEstimate3 = 98 * Coef3 + 16 * Coef2 + 2 * Coef1;
	//int ValueEstimate4 = 64 * Coef3 + 16 * Coef2 + 4 * Coef1 + ValueSrc0;
	int ValueEstimate4 = (2 * ValueEstimate1 + ValueSrc0 - (2 * ValueEstimate3 + ValueEstimate2)) / 3;
	if (ValueEstimate4 < 0)	ValueEstimate4 = 0;
	else if (ValueEstimate4 > 1023) ValueEstimate4 = 1023;
	return ValueEstimate4;

	//int ValuediffSum = (ValueEstimate1 + ValueEstimate2 + ValueEstimate3) - (ValueSrc0 + ValueSrc1 + ValueSrc2);
	//int Valuediff4 = ValueEstimate4 - ValueSrc4;
}

int DPC()
{
	int EstimateTH = 30;
	int row, col;
	int src0Up, src1Up, src2Up, src3Up, src4Up, src5Up;
	int src0Dn, src1Dn, src2Dn, src3Dn, src4Dn, src5Dn;
	int src0Lt, src1Lt, src2Lt, src3Lt, src4Lt, src5Lt;
	int src0Rt, src1Rt, src2Rt, src3Rt, src4Rt, src5Rt;
	int ValueEstUp, ValueEstDn, ValueEstLt, ValueEstRt;
	int EstMax, EstMax1, EstMax2, EstMax3, EstMin, EstMin1, EstMin2, EstMin3;
	for (row = 0; row < HEIGHT; row++)
	{
		for (col = 0; col < WIDTH; col++)
		{
			if (row > 3 && row < (HEIGHT - 4) && col > 3 && col < (WIDTH - 4))
			{
				src0Up = pRawData_10[(row - 4)*WIDTH + col];
				src1Up = pRawData_10[(row - 3)*WIDTH + col];
				src2Up = pRawData_10[(row - 2)*WIDTH + col];
				src3Up = pRawData_10[(row - 1)*WIDTH + col];
				src4Up = pRawData_10[(row - 0)*WIDTH + col];
				src5Up = pRawData_10[(row + 1)*WIDTH + col];
				src0Dn = pRawData_10[(row + 4)*WIDTH + col];
				src1Dn = pRawData_10[(row + 3)*WIDTH + col];
				src2Dn = pRawData_10[(row + 2)*WIDTH + col];
				src3Dn = pRawData_10[(row + 1)*WIDTH + col];
				src4Dn = pRawData_10[(row + 0)*WIDTH + col];
				src5Dn = pRawData_10[(row - 1)*WIDTH + col];
				src0Lt = pRawData_10[row*WIDTH + (col - 4)];
				src1Lt = pRawData_10[row*WIDTH + (col - 3)];
				src2Lt = pRawData_10[row*WIDTH + (col - 2)];
				src3Lt = pRawData_10[row*WIDTH + (col - 1)];
				src4Lt = pRawData_10[row*WIDTH + (col - 0)];
				src5Lt = pRawData_10[row*WIDTH + (col + 1)];
				src0Rt = pRawData_10[row*WIDTH + (col + 4)];
				src1Rt = pRawData_10[row*WIDTH + (col + 3)];
				src2Rt = pRawData_10[row*WIDTH + (col + 2)];
				src3Rt = pRawData_10[row*WIDTH + (col + 1)];
				src4Rt = pRawData_10[row*WIDTH + (col + 0)];
				src5Rt = pRawData_10[row*WIDTH + (col - 1)];
				ValueEstUp = DeadPixelDetect(src0Up, src1Up, src2Up, src3Up, src4Up, src5Up);
				ValueEstDn = DeadPixelDetect(src0Dn, src1Dn, src2Dn, src3Dn, src4Dn, src5Dn);
				ValueEstLt = DeadPixelDetect(src0Lt, src1Lt, src2Lt, src3Lt, src4Lt, src5Lt);
				ValueEstRt = DeadPixelDetect(src0Rt, src1Rt, src2Rt, src3Rt, src4Rt, src5Rt);
				EstMax1 = MAX(ValueEstDn, ValueEstUp);
				EstMin1 = MIN(ValueEstDn, ValueEstUp);
				EstMax2 = MAX(ValueEstLt, ValueEstRt);
				EstMin2 = MIN(ValueEstLt, ValueEstRt);
				EstMax3 = MAX((EstMax1 + EstimateTH), (EstMax2 + EstimateTH));
				EstMin3 = MIN((EstMin1 - EstimateTH), (EstMin2 - EstimateTH));
				EstMax  = CLIP(EstMax3, 0, 1023);
				EstMin  = CLIP(EstMin3, 0, 1023);
				if (pRawData_10[row*WIDTH + col] > EstMax)
				{
					pRawData_10[row*WIDTH + col] = EstMax;
					//DPCFlag[row*WIDTH + col] = 1;
				}
				else if (pRawData_10[row*WIDTH + col] < EstMin)
				{
					pRawData_10[row*WIDTH + col] = EstMin;
					//DPCFlag[row*WIDTH + col] = 1;
				}
				else
				{
					pRawData_10[row*WIDTH + col] = pRawData_10[row*WIDTH + col];
					//DPCFlag[row*WIDTH + col] = 0;
				}
			}
			else
			{
				pRawData_10[row*WIDTH + col] = pRawData_10[row*WIDTH + col];
				//DPCFlag[row*WIDTH + col] = 0;
			}
		}
	}
	return 0;
}

int Luma_Correction()
{
	printf("enter luma_correction\n");
	short luma_c;
	luma_c = -40;

	for (int i = 0; i < HEIGHT * WIDTH; i++) {
		pRawData_10[i] = CLIP((pRawData_10[i] + luma_c), 0, 1023);
	}
	printf("luma_c = %d\n", luma_c);

	printf("exit luma_correction\n");
	return 0;
}

int Gamma_Correction()
{
	printf("enter gamma_correction\n");
	char gamma_LUT[] = "gamma_LUT.txt";
	char* gamma_LUTFileName = gamma_LUT;

	FILE *fp = NULL;

	fp = fopen(gamma_LUTFileName, "wb");
	
	unsigned short LUT[1024];
	float gamma_val = 1.2;

	for (int i = 0; i < 1024; i++)
	{
		LUT[i] = (unsigned short)(pow((float)(i / 1023.0), gamma_val)*(float)1023.0);
		LUT[i] = CLIP(LUT[i], 0, 1023);
		//printf("LUT[%d]:%d\n", i, LUT[i]);
		fprintf(fp, "%d\n ", LUT[i]);
	}

	//fwrite(LUT, 1024, 2, fp);
	fclose(fp);

	for (int i = 0; i < HEIGHT * WIDTH; i++) {
		pRawData_10[i] = LUT[pRawData_10[i]];
	}

	printf("exit gamma_correction\n");
	return 0;
}


int Raw10_To_Rgb()
{
	printf("enter Raw10_To_Rgb\n");
	IplImage *pRgbDataInt10 = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_16U, 3);
	pRgbDataInt8 = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	pBayerData_10 = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_16U, 1);
	memcpy(pBayerData_10->imageData, (short *)pRawData_10, WIDTH * HEIGHT * sizeof(unsigned short));
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

unsigned char AWB_Mode = 0;
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
				TB = ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 0];
				TG = ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 1];
				TR = ((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 2];

				TB = TB + (G - B) *  (TB + TG + TR) / (R + G + B);
				TR = TR + (G - R) *  (TB + TG + TR) / (R + G + B);

				TB = CLIP(TB, 0, 255); 
				TG = CLIP(TG, 0, 255);
				TR = CLIP(TR, 0, 255);

				((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 0] = (uchar)TB;
				((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 1] = (uchar)TG;
				((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 2] = (uchar)TR;
			}
		}
	}
	else if (AWB_Mode == 1)
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

				TB = CLIP(TB, 0, 255);
				TG = CLIP(TG, 0, 255);
				TR = CLIP(TR, 0, 255);

				((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 0] = (uchar)TB;
				((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 1] = (uchar)TG;
				((uchar *)(pRgbDataInt8->imageData + i * pRgbDataInt8->widthStep))[j * pRgbDataInt8->nChannels + 2] = (uchar)TR;
			}
		}
	}
	else if (AWB_Mode == 2)
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

				scalar.val[0] = CLIP(scalar.val[0], 0, 255);
				scalar.val[1] = CLIP(scalar.val[1], 0, 255);
				scalar.val[2] = CLIP(scalar.val[2], 0, 255);
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
				TB = CLIP(TB, 0, 255);
				TG = CLIP(TG, 0, 255);
				TR = CLIP(TR, 0, 255);
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

	Mat iMat_rgb(HEIGHT, WIDTH, CV_8UC3, pRgbDataInt8->imageData);
	imwrite("a_rgb.bmp", iMat_rgb);

	cvDestroyWindow("rgb_awb");
	printf("exit AWB\n");
	return 0;
}

int Rgb_To_Yuv()
{
	printf("enter Rgb_To_Yuv\n");
	pYuYvData_8 = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	cvCvtColor(pRgbDataInt8, pYuYvData_8, CV_BGR2YUV);

	cvNamedWindow("yuv_10", 1);
	cvShowImage("yuv_10", pYuYvData_8);
	cvWaitKey(0);

#ifdef SAVE_YUV
	char img_new[] = "a_new.yuv";
	char* new_rawFileName = img_new;

	int ret;
	FILE *fp_1 = NULL;
	fp_1 = fopen(new_rawFileName, "wb");

	ret = fwrite(pYuYvData_8->imageData, 1, WIDTH * HEIGHT * 3, fp_1);

	if (1 != ret)
	{
		printf("Fail to fwrite data!\n");
		return -1;
	}

	fclose(fp_1);
#endif

	cvDestroyWindow("yuv_10");
	printf("exit Rgb_To_Yuv\n");
	return 0;
}


int End_Process()
{
	cvReleaseImage(&pRgbDataInt8);
	cvReleaseImage(&pYuYvData_8);

	return 0;
}


int ISP_PROCESS()
{
	Read_File();

	Mipi_Raw_To_Raw10();

	DPC();

	Luma_Correction();

	Gamma_Correction();

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