/* -LICENSE-START-
** Copyright (c) 2018 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
**
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/

#include <wincodec.h>		// For handing bitmap files
#include <atlstr.h>
#include <algorithm>
#include "ImageWriter.h"

namespace ImageWriter
{
	IWICImagingFactory*	g_wicFactory;
}

HRESULT ImageWriter::Initialize()
{
	// Create WIC Imaging factory to write image stills
	HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wicFactory));
	if (FAILED(result))
	{
		fprintf(stderr, "A WIC imaging factory could not be created.\n");
	}

	return result;
}

HRESULT ImageWriter::UnInitialize()
{
	if (g_wicFactory != NULL)
	{
		g_wicFactory->Release();
		g_wicFactory = NULL;
	}
	return S_OK;
}

HRESULT ImageWriter::GetNextFilenameWithPrefix(const std::string& path, const std::string& prefix, std::string& nextFileName)
{
	HRESULT result = E_FAIL;
	static int idx = 0;

	while (idx < 10000)
	{
		CString	filename;
		filename.Format(_T("%s\\%s%.4d.png"), CString(path.c_str()), CString(prefix.c_str()), idx++);

		if (!PathFileExists(filename))
		{
			nextFileName = std::string(CT2CA(filename.GetString())); 
			result = S_OK;
			break;
		}
	}
	
	return result;
}

HRESULT ImageWriter::WriteBgra32VideoFrameToPNG(IDeckLinkVideoFrame* bgra32VideoFrame, const std::string& pngFilename)
{
	HRESULT								result = S_OK;
	void*								bgra32FrameBytes = NULL;

	IWICBitmapEncoder*					bitmapEncoder = NULL;
	IWICBitmapFrameEncode*				bitmapFrame = NULL;
	IWICStream*							fileStream = NULL;
	WICPixelFormatGUID					pixelFormat = GUID_WICPixelFormat32bppBGRA;;

	CString filename = pngFilename.c_str();

	// Ensure video frame has expected pixel format
	if (bgra32VideoFrame->GetPixelFormat() != bmdFormat8BitBGRA)
	{
		fprintf(stderr, "Video frame is not in 8-Bit BGRA pixel format\n");
		return E_FAIL;
	}
	
	bgra32VideoFrame->GetBytes(&bgra32FrameBytes);
	if (bgra32FrameBytes == NULL)
	{
		fprintf(stderr, "Could not get DeckLinkVideoFrame buffer pointer\n");
		result = E_OUTOFMEMORY;
		goto bail;
	}

	result = g_wicFactory->CreateStream(&fileStream);
	if (FAILED(result))
		goto bail;

	result = fileStream->InitializeFromFilename(filename, GENERIC_WRITE);
	if (FAILED(result))
		goto bail;

	result = g_wicFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &bitmapEncoder);
	if (FAILED(result))
		goto bail;

	result = bitmapEncoder->Initialize(fileStream, WICBitmapEncoderNoCache);
	if (FAILED(result))
		goto bail;

	result = bitmapEncoder->CreateNewFrame(&bitmapFrame, NULL);
	if (FAILED(result))
		goto bail;

	result = bitmapFrame->Initialize(NULL);
	if (FAILED(result))
		goto bail;

	// Set bitmap frame size based on video frame
	result = bitmapFrame->SetSize(bgra32VideoFrame->GetWidth(), bgra32VideoFrame->GetHeight());
	if (FAILED(result))
		goto bail;

	// Bitmap pixel format WICPixelFormat32bppRGB will match Bgra32VideoFrame
	result = bitmapFrame->SetPixelFormat(&pixelFormat);
	if (FAILED(result) || (!IsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppBGRA)))
		// Unable to support 32-bit RGB
		goto bail;

	// Write video buffer to bitmap
	result = bitmapFrame->WritePixels(bgra32VideoFrame->GetHeight(), bgra32VideoFrame->GetRowBytes(), bgra32VideoFrame->GetHeight()*bgra32VideoFrame->GetRowBytes(), (BYTE*)bgra32FrameBytes);
	if (FAILED(result))
		goto bail;

	result = bitmapFrame->Commit();
	if (FAILED(result))
		goto bail;

	result = bitmapEncoder->Commit();
	if (FAILED(result))
		goto bail;

bail:
	if (bitmapFrame != NULL)
		bitmapFrame->Release();

	if (bitmapEncoder != NULL)
		bitmapEncoder->Release();

	if (fileStream != NULL)
		fileStream->Release();

	return result;
}
