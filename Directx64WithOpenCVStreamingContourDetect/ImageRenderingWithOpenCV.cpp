//// ImageRenderingWithOpenCV.cpp : 
//// Displays the Image using DirectX, uses OpenCV for edge detection
//
//-----------------------------------------------------------------------------
//  Description: Read binary file and use OpenCV to do edge detection and display continuous image in DirectX
//  F1 - Gray scale 
//  F2 - Contour
//  F3 - Sobel 
//  F4 - Canny 
//  F5 - Regular
//                 
//  Add d3d9.lib to the linker's additional dependencies 
//  OpenCV setup
//  OPENCV_DIR = C:\Users\<name>\source\opencv\build\x64\vc15
//  Path = C:\Users\<name>\source\opencv\build\x64\vc15\bin;
//  VC++directories->IncludeDirectories : add $(OPENCV_DIR)\..\..\include;
//  Linker->General->Additional library Directories : $(OPENCV_DIR)\lib
//  Linker->Input->AdditionalDependencies : opencv_world320.lib;
//-----------------------------------------------------------------------------

#define STRICT

#include "stdafx.h"
typedef unsigned char BYTE;

#include <fstream>
#include <iostream>


using namespace std;
#include <windows.h>
#include <stdio.h>      

#include <d3d9.h>
#include <d3dx9.h>
#include "resource.h"

#include <vector>
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <sstream>

#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <stdlib.h>
#include <stdio.h>


using namespace cv;
using namespace std;

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 540

typedef unsigned char BYTE;

#define CV_BGR2GRAY cv::COLOR_BGR2GRAY
#define CV_GRAY2BGR cv::COLOR_GRAY2BGR
#define CV_BGR2GRAY cv::COLOR_BGR2GRAY
#define CV_BGR2YCrCb cv::COLOR_BGR2YCrCb
#define CV_YCrCb2BGR cv::COLOR_YCrCb2BGR
#define CV_GRAY2RGB cv::COLOR_GRAY2RGB

//-----------------------------------------------------------------------------
// GLOBALS
//-----------------------------------------------------------------------------
HWND                    g_hWnd = NULL;
LPDIRECT3D9             g_pD3D = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVertexBuffer = NULL;
LPDIRECT3DTEXTURE9      g_pTexture = NULL;

unsigned char* myFrameBitmapBits; // not yet allocated
HBITMAP myFrameBitmap;
HDC myBMPHdc;

Mat src_gray;
int thresh = 100;
Mat canny_output;
vector<vector<Point> > contours;
vector<Vec4i> hierarchy;

char* myLocalBuffer;
uchar* dstBuffer;

int datacount = 0;
int size = 0;
int numChunksInFile = 0;
enum class alg { regular, canny, sobel, contour, gray };
alg myAlg = alg::regular;

#define D3DFVF_CUSTOMVERTEX ( D3DFVF_XYZ | D3DFVF_TEX1 )

struct Vertex
{
	float x, y, z;
	float tu, tv;
};

Vertex g_quadVertices[] =
{
	{ -1.0f, 1.0f, 0.0f, 0.0f, 0.0f },
	{ 1.0f, 1.0f, 0.0f, 1.0f, 0.0f },
	{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
	{ 1.0f, -1.0f, 0.0f, 1.0f, 1.0f }
};

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void init(void);
void shutDown(void);
void render(void);


bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile);
void copyRectangle(
	unsigned char* dest,
	const unsigned char* src,
	int bytesPerPixel,
	int destOffsetLateralPixels,
	int destOffsetVerticalRows,
	int srcWidthPixels,
	int srcHeightRows,
	int destWidthPixels,
	int destHeightRows);

void computeContourImage(
	uchar* src,
	uchar* dst);
void computeSobelImage(
	uchar* src,
	uchar* dst);
void computeCannyEdgeImage(
	uchar* src,
	uchar* dst);
void computeGrayScaleImage(
	uchar* src,
	uchar* dst);

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow)
{
	WNDCLASSEX winClass;
	MSG        uMsg;

	memset(&uMsg, 0, sizeof(uMsg));


	winClass.lpszClassName = L"MY_WINDOWS_CLASS";
	winClass.cbSize = sizeof(WNDCLASSEX);
	winClass.style = CS_HREDRAW | CS_VREDRAW;
	winClass.lpfnWndProc = WindowProc;
	winClass.hInstance = hInstance;
	winClass.hIcon = NULL;//LoadIcon(hInstance, (LPCTSTR)IDI_DIRECTX_ICON);
	winClass.hIconSm = LoadIcon(hInstance, (LPCTSTR)IDI_DIRECTX64WITHOPENCVSTREAMINGCONTOURDETECT);
	winClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	winClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	winClass.lpszMenuName = NULL;
	winClass.cbClsExtra = 0;
	winClass.cbWndExtra = 0;

	//1. Register class
	if (!RegisterClassEx(&winClass))
		return E_FAIL;

	//2. Create Window
	g_hWnd = CreateWindowEx(NULL, L"MY_WINDOWS_CLASS",
		L"Direct3D (DX9) - OpenCV Image Filter",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (g_hWnd == NULL)
		return E_FAIL;

	//3. Show Window
	ShowWindow(g_hWnd, nCmdShow);
	::SetTimer(g_hWnd, 1, 100, nullptr);

	init(); /// initialize directx objects


	while (uMsg.message != WM_QUIT)
	{
		if (PeekMessage(&uMsg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&uMsg);
			DispatchMessage(&uMsg);
		}
	}

	shutDown();

	UnregisterClass(L"MY_WINDOWS_CLASS", winClass.hInstance);

	return uMsg.wParam;
}

//-----------------------------------------------------------------------------
// Name: WindowProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND   hWnd,
	UINT   msg,
	WPARAM wParam,
	LPARAM lParam)
{
	{
		std::ofstream Moutdata("d:\\temp\\Log.log", std::ios::app);
		Moutdata << " pid:" << ::GetProcessId << std::endl;
		Moutdata.close();
	}
	switch (msg)
	{
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
			case VK_ESCAPE:
				PostQuitMessage(0);
				break;

			case VK_F1: // gray
				myAlg = alg::gray;
				render();
				break;

			case VK_F2: // contour
				myAlg = alg::contour;
				render();
				break;

			case VK_F3: // sobel
				myAlg = alg::sobel;
				render();
				break;

			case VK_F4: // canny
				myAlg = alg::canny;
				render();
				break;

			case VK_F5: // regular
				myAlg = alg::regular;
				render();
				break;
		}
	}
	break;

	case WM_TIMER:
		::InvalidateRect(g_hWnd, nullptr, TRUE);
		::UpdateWindow(g_hWnd);  // this will call WM_PAINT

		return 0;

	case WM_PAINT:
	{
		render();
		break;
	}

	case WM_CLOSE:
	{
		PostQuitMessage(0);
	}

	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	break;

	default:
	{
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	break;
	}

	return 0;
}

void GetImageDataFromHBITMAP(HDC hdc, HBITMAP bitmap, BYTE* pImageData)
{
	BITMAPINFO bmpInfo;
	memset(&bmpInfo, 0, sizeof(BITMAPINFOHEADER));
	bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	GetDIBits(hdc, bitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS);
	bmpInfo.bmiHeader.biBitCount = 32;
	bmpInfo.bmiHeader.biCompression = BI_RGB;
	GetDIBits(hdc, bitmap, 0, bmpInfo.bmiHeader.biHeight, pImageData, &bmpInfo, DIB_RGB_COLORS);
}

void init(void)
{
	//1. create directx3d
	g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.BackBufferWidth = SCREEN_WIDTH;
	d3dpp.BackBufferHeight = SCREEN_HEIGHT;
	d3dpp.BackBufferCount = 1;
	d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;

	DWORD qualityLevels;
	if (g_pD3D->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
		TRUE, D3DMULTISAMPLE_NONMASKABLE, &qualityLevels) == D3D_OK)
	{
		d3dpp.MultiSampleType = D3DMULTISAMPLE_NONMASKABLE;
	}
	else if (g_pD3D->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
		TRUE, D3DMULTISAMPLE_4_SAMPLES, &qualityLevels) == D3D_OK)
	{
		d3dpp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
	}
	else if (g_pD3D->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
		TRUE, D3DMULTISAMPLE_2_SAMPLES, &qualityLevels) == D3D_OK)
	{
		d3dpp.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
	}
	d3dpp.MultiSampleQuality = qualityLevels - 1;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = g_hWnd;
	d3dpp.Windowed = TRUE;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.Flags = D3DPRESENTFLAG_VIDEO;
	d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;


	//4. create device
	g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hWnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,
		&d3dpp, &g_pd3dDevice);

	if (g_pd3dDevice)
	{
		g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

		g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

		D3DCAPS9 caps;
		g_pd3dDevice->GetDeviceCaps(&caps);

		if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) == D3DPTFILTERCAPS_MAGFANISOTROPIC)
		{
			g_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
		}
		else if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR) == D3DPTFILTERCAPS_MAGFLINEAR)
		{
			g_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		}

		if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) == D3DPTFILTERCAPS_MINFANISOTROPIC)
		{
			g_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
		}
		else if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) == D3DPTFILTERCAPS_MINFLINEAR)
		{
			g_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		}
	}

	//5. create vertex buffer
	g_pd3dDevice->CreateVertexBuffer(4 * sizeof(Vertex), D3DUSAGE_WRITEONLY,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT,
		&g_pVertexBuffer, NULL);
	void *pVertices = NULL;
	g_pVertexBuffer->Lock(0, sizeof(g_quadVertices), (void**)&pVertices, 0);
	memcpy(pVertices, g_quadVertices, sizeof(g_quadVertices));
	g_pVertexBuffer->Unlock();

	//6. create texture
	if (g_pd3dDevice)
	{
		g_pd3dDevice->CreateTexture(
			SCREEN_WIDTH,
			SCREEN_HEIGHT,
			1,
			D3DUSAGE_DYNAMIC,
			D3DFMT_X8R8G8B8,
			D3DPOOL_DEFAULT,
			&g_pTexture,
			0);


		if (g_pTexture)
		{
			IDirect3DSurface9 *surface = 0;
			g_pTexture->GetSurfaceLevel(0, &surface);
			if (surface)
			{
				HDC hdc;
				surface->GetDC(&hdc);

				RECT rect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
				::FillRect(hdc, &rect, (HBRUSH)::GetStockObject(WHITE_BRUSH)); 

				surface->ReleaseDC(hdc);
				surface->Release();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Name: shutDown()
// Desc: 
//-----------------------------------------------------------------------------
void shutDown(void)
{
	if (g_pTexture != NULL)
		g_pTexture->Release();

	if (g_pVertexBuffer != NULL)
		g_pVertexBuffer->Release();

	if (g_pd3dDevice != NULL)
		g_pd3dDevice->Release();

	if (g_pD3D != NULL)
		g_pD3D->Release();

	delete[] myLocalBuffer;
}

void render()
{
	// read it once
	if (datacount == 0)
	{
		ifstream myfile("Sample_BGR.raw", ios::binary);

		myfile.seekg(0, ios::end);
		int fileSize = myfile.tellg();
		size = SCREEN_WIDTH * SCREEN_HEIGHT * 3;
		numChunksInFile = fileSize / size;

		myLocalBuffer = new char[size * numChunksInFile];
		myfile.seekg(0, ios::beg);
		myfile.read((char*)myLocalBuffer, size * numChunksInFile);
		myfile.close();
		cout << "the entire file content is in memory size:" << fileSize << std::endl;

		// create dst buffer one time
		dstBuffer = new uchar[size * 1];
	}

	int j = datacount%numChunksInFile;
	int offset = j * size;
	uchar* buffer = (uchar*)myLocalBuffer;
	buffer = (uchar*)myLocalBuffer + offset;
	datacount++;


	memset(dstBuffer, 0, size);
	switch (myAlg)
	{
		case alg::canny:
			computeCannyEdgeImage(buffer, dstBuffer);
			break;
		case alg::sobel:
			computeSobelImage(buffer, dstBuffer);
			break;
		case alg::contour:
			computeContourImage(buffer, dstBuffer);
			break;
		case alg::gray:
			computeGrayScaleImage(buffer, dstBuffer);
			break;
		case alg::regular:
			memcpy(dstBuffer, buffer, size);
			break;
	}

	//
	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(BITMAPINFO));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmi.bmiHeader.biWidth = SCREEN_WIDTH;
	bmi.bmiHeader.biHeight = -SCREEN_HEIGHT; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	void *pvBits;
	myFrameBitmap = ::CreateDIBSection(0, &bmi, DIB_RGB_COLORS, &pvBits, 0, 0);
	myFrameBitmapBits = reinterpret_cast<unsigned char*>(pvBits);
	::ZeroMemory(myFrameBitmapBits, SCREEN_WIDTH * SCREEN_HEIGHT * 3);

	copyRectangle(myFrameBitmapBits,
		(const unsigned char*)dstBuffer, 
		3,
		0,
		0,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		SCREEN_WIDTH,
		SCREEN_HEIGHT);

	myBMPHdc = ::CreateCompatibleDC(0);
	::SelectObject(myBMPHdc, myFrameBitmap);

	// just for checking
	SaveBMP(myFrameBitmapBits, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH* SCREEN_HEIGHT * 3, L"D:\\temp\\copy.bmp");


	HRESULT hr;
	IDirect3DSurface9* BackBuffer = NULL;         


	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(g_hWnd, &ps);

	IDirect3DSurface9 *surface = nullptr;
	hr = g_pTexture->GetSurfaceLevel(0, &surface);
	if (surface)
	{
		HDC surfaceDc;
		surface->GetDC(&surfaceDc);

		::BitBlt(surfaceDc, 0, 0,
			SCREEN_WIDTH, SCREEN_HEIGHT,
			myBMPHdc, 0, 0, SRCCOPY);

		surface->ReleaseDC(surfaceDc);

		if (surface)
		{
			g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(55, 90, 0), 1.0f, 0);
			g_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &BackBuffer);
			g_pd3dDevice->SetRenderTarget(0, BackBuffer);

			g_pd3dDevice->BeginScene();

			g_pd3dDevice->SetTexture(0, g_pTexture);

			g_pd3dDevice->SetStreamSource(0, g_pVertexBuffer, 0, sizeof(Vertex));
			g_pd3dDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
			g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

			g_pd3dDevice->StretchRect(surface, NULL, BackBuffer, NULL, D3DTEXF_NONE);
			g_pd3dDevice->EndScene();

			//clean up here
			surface->Release();
			surface = nullptr;
			BackBuffer->Release();
			BackBuffer = nullptr;

			if (myBMPHdc)
			{
				::DeleteDC(myBMPHdc);
				myBMPHdc = nullptr;
			}
			if (myFrameBitmap)
			{
				::DeleteObject(myFrameBitmap);
				myFrameBitmap = nullptr;
				myFrameBitmapBits = nullptr;
			}


			if (D3D_OK != g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr))
			{
				{
					std::ofstream Moutdata("d:\\temp\\Log.log", std::ios::app);
					Moutdata << " g_pd3dDevice->Present failed:" << hr << std::endl;
					Moutdata.close();
				}
			}
		}
	}

	EndPaint(g_hWnd, &ps);
}




bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile)
{
	// declare bmp structures 
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER info;

	// andinitialize them to zero
	memset(&bmfh, 0, sizeof(BITMAPFILEHEADER));
	memset(&info, 0, sizeof(BITMAPINFOHEADER));

	// fill the fileheader with data
	bmfh.bfType = 0x4d42;       // 0x4d42 = 'BM'
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paddedsize;
	bmfh.bfOffBits = 0x36;		// number of bytes to start of bitmap bits

	// fill the infoheader

	info.biSize = sizeof(BITMAPINFOHEADER);
	info.biWidth = width;
	info.biHeight = height;
	info.biPlanes = 1;			// we only have one bitplane
	info.biBitCount = 24;		// RGB mode is 24 bits
	info.biCompression = BI_RGB;
	info.biSizeImage = 0;		// can be 0 for 24 bit images
	info.biXPelsPerMeter = 0x0ec4;     // paint and PSP use this values
	info.biYPelsPerMeter = 0x0ec4;
	info.biClrUsed = 0;			// we are in RGB mode and have no palette
	info.biClrImportant = 0;    // all colors are important

	// now we open the file to write to
	HANDLE file = CreateFile(bmpfile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == NULL)
	{
		CloseHandle(file);
		return false;
	}

	// write file header
	unsigned long bwritten;
	if (WriteFile(file, &bmfh, sizeof(BITMAPFILEHEADER), &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}
	// write infoheader
	if (WriteFile(file, &info, sizeof(BITMAPINFOHEADER), &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}
	// write image data
	if (WriteFile(file, Buffer, paddedsize, &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}

	// and close file
	CloseHandle(file);

	return true;
}


void copyRectangle(
	unsigned char* dest,
	const unsigned char* src,
	int bytesPerPixel,
	int destOffsetLateralPixels,
	int destOffsetVerticalRows,
	int srcWidthPixels,
	int srcHeightRows,
	int destWidthPixels,
	int destHeightRows)
{
	int copyWidthPixels = min(srcWidthPixels, destWidthPixels - destOffsetLateralPixels);
	int copyHeightRows = min(srcHeightRows, destHeightRows - destOffsetVerticalRows);

	if ((copyWidthPixels > 0) && (copyHeightRows > 0))
	{
		int destStride = destWidthPixels * bytesPerPixel;
		unsigned char* destRowP = dest +
			(destOffsetVerticalRows * destStride) +
			(destOffsetLateralPixels * bytesPerPixel);

		int srcStride = srcWidthPixels * bytesPerPixel;
		const unsigned char* srcRowP = src;

		int copyStride = copyWidthPixels * bytesPerPixel;

		for (int row = 0; row < copyHeightRows; row++)
		{
			::memcpy(destRowP, srcRowP, copyStride);
			destRowP += destStride;
			srcRowP += srcStride;
		}
	}
}


void computeContourImage(
	uchar* buffer,
	uchar* dstBuffer)
{
	const int HEIGHT = SCREEN_WIDTH;
	const int WIDTH = SCREEN_HEIGHT;
	Mat src = Mat(WIDTH, HEIGHT, CV_8UC3, buffer);


	/// Convert image to gray and blur it 
	cvtColor(src, src_gray, COLOR_BGR2GRAY);
	blur(src_gray, src_gray, Size(3, 3));

	/// Detect edges using canny 
	Canny(src_gray, canny_output, thresh, thresh * 2, 3);
	/// Find contours 
	findContours(canny_output, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));


	/// Draw contours 
	Mat drawing = Mat(WIDTH, HEIGHT, CV_8UC3, dstBuffer);
	for (size_t i = 0; i< contours.size(); i++)
	{
		Scalar color = Scalar(45, 115, 135); // one color
		drawContours(drawing, contours, (int)i, color, 3, 8, hierarchy, 0, Point());
		
	}

}

void computeCannyEdgeImage(
	uchar* buffer,
	uchar* dstBuffer)
{

	const int HEIGHT = SCREEN_WIDTH;
	const int WIDTH = SCREEN_HEIGHT;

	Mat src1 = Mat(WIDTH, HEIGHT, CV_8UC3, buffer);

	Mat gray, edge, draw;
	/// Convert it to gray
	cvtColor(src1, gray, CV_BGR2GRAY);

	// find the canny edge- result ia 8bit
	Canny(gray, edge, 80, 150, 3);

	edge.convertTo(draw, CV_8U);

	Mat dest1 = Mat(WIDTH, HEIGHT, CV_8UC3, dstBuffer);
	cvtColor(draw, dest1, CV_GRAY2RGB);
	
}

void computeGrayScaleImage(
	uchar* buffer,
	uchar* dstBuffer)
{

	const int HEIGHT = SCREEN_WIDTH;
	const int WIDTH = SCREEN_HEIGHT;

	Mat src1 = Mat(WIDTH, HEIGHT, CV_8UC3, buffer);
	
	Mat gray;
	/// Convert it to gray
	cvtColor(src1, gray, CV_BGR2GRAY);
	//grayFrame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

	Mat dest1 = Mat(WIDTH, HEIGHT, CV_8UC3, dstBuffer);
	cvtColor(gray, dest1, CV_GRAY2RGB);
}

void computeSobelImage(
	uchar* buffer,
	uchar* dstBuffer)
{
	Mat  src_gray;
	Mat grad;
	int scale = 1;
	int delta = 0;
	int ddepth = CV_16S;

	const int HEIGHT = SCREEN_WIDTH;
	const int WIDTH = SCREEN_HEIGHT;

	// create Mat with src img
	Mat src = Mat(WIDTH, HEIGHT, CV_8UC3, buffer);
	

	GaussianBlur(src, src, Size(3, 3), 0, 0, BORDER_DEFAULT);

	/// Convert it to gray
	cvtColor(src, src_gray, COLOR_RGB2GRAY);

	/// Generate grad_x and grad_y
	Mat grad_x, grad_y;
	Mat abs_grad_x, abs_grad_y;

	/// Gradient X
	Sobel(src_gray, grad_x, ddepth, 1, 0, 3, scale, delta, BORDER_DEFAULT);
	convertScaleAbs(grad_x, abs_grad_x);

	/// Gradient Y
	Sobel(src_gray, grad_y, ddepth, 0, 1, 3, scale, delta, BORDER_DEFAULT);
	convertScaleAbs(grad_y, abs_grad_y);

	/// Total Gradient (approximate)
	addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, grad);

	// convert the greyscale image back to rgb image
	Mat dest1 = Mat(WIDTH, HEIGHT, CV_8UC3, dstBuffer);
	cvtColor(grad, dest1, CV_GRAY2RGB);
	//imwrite("SobelDest_Color.jpg", dest1);
}








