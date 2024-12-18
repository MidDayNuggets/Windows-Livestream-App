#include "screencapture.h"
#include <stdio.h>
#include <windows.h>
#include <gdiplus.h>
#include <time.h>

using namespace Gdiplus;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT  num = 0;
    UINT  size = 0;

    ImageCodecInfo* pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if(size == 0) {
        return -1;
    }

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if(pImageCodecInfo == NULL) {
        return -1;
    }

    GetImageEncoders(num, size, pImageCodecInfo);
    for(UINT j = 0; j < num; ++j) {
        if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 ) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }    
    }

    free(pImageCodecInfo);
    return 0;
}

IStream* captureScreenToStream() {
    IStream* imageStream = nullptr;
    CreateStreamOnHGlobal(NULL, TRUE, &imageStream);

    HDC scrdc = GetDC(0);
    int Height = GetSystemMetrics(SM_CYSCREEN);
    int Width = GetSystemMetrics(SM_CXSCREEN);

    HDC memdc = CreateCompatibleDC(scrdc);
    HBITMAP membit = CreateCompatibleBitmap(scrdc, Width, Height);
    SelectObject(memdc, membit);
    
    BitBlt(memdc, 0, 0, Width, Height, scrdc, 0, 0, SRCCOPY);
    
    Bitmap bitmap(membit, NULL);
    CLSID clsid;
    GetEncoderClsid(L"image/jpeg", &clsid);
    bitmap.Save(imageStream, &clsid, NULL);
    
    LARGE_INTEGER li = {0};
    imageStream->Seek(li, STREAM_SEEK_SET, NULL);

    DeleteObject(memdc);
    DeleteObject(membit);
    ReleaseDC(0,scrdc);

    STATSTG stats;
    imageStream->Stat(&stats, STATFLAG_NONAME);
    
    return imageStream;
}
