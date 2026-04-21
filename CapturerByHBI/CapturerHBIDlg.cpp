#include <windows.h>
#include <winnt.h>
PCONTEXT __pctx_probe = nullptr;

#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <cstring>
#include <tiffio.h>
#include <numeric>
#include <string>

/*
#include "../../BigTIFF/BigTIFF.h"
#include "../..\CTiff\Tiff.h"
#include "../../Common/Sprintf.h"
*/
#include "HbiFpd.h"
#include "HbiType.h"
#include "HbiError.h"

#include "./HBIDeviceCtrl.h"

using namespace std;

void SaveAsMultiFrameTiff(
    const std::string& filename,
    const std::vector<uint16_t>& buffer,
    unsigned int width,
    unsigned int height,
    unsigned int frameCount
)
{
    std::cout << "Saving multi-frame TIFF: " << filename << "\n";
    if (width == 0 || height == 0 || frameCount == 0) {
        std::cerr << "Invalid image size or frame count\n";
        return;
    }

    const size_t framePixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t requiredPixelCount = framePixelCount * static_cast<size_t>(frameCount);
    if (buffer.size() < requiredPixelCount) {
        std::cerr << "Buffer size is smaller than required for requested frame count\n";
        return;
    }

    TIFF* tif = TIFFOpen(filename.c_str(), "w");
    if (!tif) {
        std::cerr << "TIFFOpen failed\n";
        return;
    }

    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_SUBFILETYPE, 0);
        TIFFSetField(tif, TIFFTAG_PAGENUMBER, frame, frameCount);

        const size_t frameOffset = static_cast<size_t>(frame) * framePixelCount;
        for (uint32_t row = 0; row < height; ++row) {
            const uint16_t* rowPtr = &buffer[frameOffset + static_cast<size_t>(row) * width];
            if (TIFFWriteScanline(tif, (void*)rowPtr, row, 0) < 0) {
                std::cerr << "TIFFWriteScanline failed at frame " << frame << ", row " << row << "\n";
                TIFFClose(tif);
                return;
            }
        }

        if (!TIFFWriteDirectory(tif)) {
            std::cerr << "TIFFWriteDirectory failed at frame " << frame << "\n";
            TIFFClose(tif);
            return;
        }
    }

    TIFFClose(tif);
}

int main()
{
	constexpr int FRAMECOUNT = 100;

	CHBIDeviceCtrl cCHBIDeviceCtrl;

    // initialize
    cCHBIDeviceCtrl.Initialize();

    // コールバック関数の設定
    cCHBIDeviceCtrl.SetCallBackFun();

    cCHBIDeviceCtrl.ConectJumbo();

	cCHBIDeviceCtrl.SetImageBuffer(FRAMECOUNT);
}