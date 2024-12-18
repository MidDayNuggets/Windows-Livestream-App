#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <mutex>

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
IStream* captureScreenToStream();

#endif