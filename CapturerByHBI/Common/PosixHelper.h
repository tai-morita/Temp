#pragma once

#ifdef _WIN32
/* MSC */
#include <crtdbg.h>
#if defined(_MSC_VER) && (_MSC_VER <= 1500)
	typedef unsigned __int64 uint64_t;
	typedef          __int64  int64_t;
	typedef unsigned __int32 uint32_t;
	typedef          __int32  int32_t;
	typedef unsigned __int16 uint16_t;
	typedef          __int16  int16_t;
#else
	/* VC10 ˆÈ~‚Í stdint.h ‚ª“Ç‚ß‚é */
    #include <stdint.h>
#endif

#else 
/* GCC 4
 defined(__GNUC__) && __GNUC__ >= 4 
*/
#include <cstddef>
#include <cassert>
#define _ASSERT assert
#define ASSERT assert
#include <stdint.h>
#define TRACE printf
typedef uint64_t    ULONGLONG;
typedef int64_t     LONGLONG;
typedef uint16_t    WORD, ULONG;
typedef uint32_t    DWORD, *DWORD_PTR, UINT;
typedef uint8_t     BYTE, UCHAR;
typedef DWORD       COLORREF;
typedef long        LONG;
typedef char        *LPSTR, CHAR;
typedef char        *LPTSTR;
typedef const char  *LPCTSTR;
typedef int         BOOL;
typedef DWORD       HANDLE;
typedef int32_t     *LRESULT, *LPARAM;
typedef uint32_t    *WPARAM;
typedef void        *LPVOID;

#define TRUE 1
#define FALSE 0


#define TRACE printf

#define _T(x)               (x)
#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((BYTE)(((DWORD)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))

#define RGB(r,g,b)          ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))

#define GetRValue(rgb)      (LOBYTE(rgb))
#define GetGValue(rgb)      (LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb)      (LOBYTE((rgb)>>16))

/* constants for the biCompression field */
#define BI_RGB        0L
#define BI_RLE8       1L
#define BI_RLE4       2L
#define BI_BITFIELDS  3L
#define BI_JPEG       4L
#define BI_PNG        5L


typedef struct tagRGBTRIPLE {
        BYTE    rgbtBlue;
        BYTE    rgbtGreen;
        BYTE    rgbtRed;
}__attribute__((packed)) RGBTRIPLE, *PRGBTRIPLE, *NPRGBTRIPLE, *LPRGBTRIPLE;

typedef struct tagRGBQUAD {
        BYTE    rgbBlue;
        BYTE    rgbGreen;
        BYTE    rgbRed;
        BYTE    rgbReserved;
}__attribute__((packed)) RGBQUAD;
typedef RGBQUAD * LPRGBQUAD;

typedef struct tagBITMAPFILEHEADER {
        WORD    bfType;
        DWORD   bfSize;
        WORD    bfReserved1;
        WORD    bfReserved2;
        DWORD   bfOffBits;
}__attribute__((packed)) BITMAPFILEHEADER, *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;
typedef struct tagBITMAPINFOHEADER{
        DWORD      biSize;
        LONG       biWidth;
        LONG       biHeight;
        WORD       biPlanes;
        WORD       biBitCount;
        DWORD      biCompression;
        DWORD      biSizeImage;
        LONG       biXPelsPerMeter;
        LONG       biYPelsPerMeter;
        DWORD      biClrUsed;
        DWORD      biClrImportant;
}__attribute__((packed)) BITMAPINFOHEADER, *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER    bmiHeader;
    RGBQUAD             bmiColors[1];
}__attribute__((packed)) BITMAPINFO, *LPBITMAPINFO, *PBITMAPINFO;


#define _MBC_SINGLE  (0)
#define _MBC_LEAD (1)
#define _MBC_TRAIL (2)
#define _MBC_ILLEGAL (-1)
int _mbsbtype(
   const unsigned char *mbstr,
   size_t count
) {
    unsigned char cC = mbstr[count];
    if (false) {
    } else if (cC >= 0x20 && cC <= 0x7E || cC >= 0XA1 && cC <= 0XDF) {
        return _MBC_SINGLE;
    } else if (cC >= 0x81 && cC <= 0X9F || cC >= 0xE0 && cC <= 0XFC) {
        return _MBC_LEAD;
    } else if (cC >= 0x40 && cC <= 0X7E || cC >= 0x80 && cC <= 0XFC) {
        return _MBC_TRAIL;
    }
    return _MBC_ILLEGAL;
}

#ifndef SW_HIDE
#define SW_HIDE 0
#endif

#endif
