
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
		std::wcout << width << height << frameCount << std::endl;
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
    constexpr char* kpcFPDIP = "192.168.10.40";
    constexpr char* kpcPCIP  = "192.168.10.20";
    constexpr unsigned short kusFPDPORT = 32897;
    constexpr unsigned short kusPCPORT  = 32896;

    constexpr int kiFRAMECOUNT    = 2;
    constexpr int kiGAINLEVEL     = 2; // 1.2PC
    constexpr int kiEXPTIME_milli = 2500; // 33ms
    constexpr int kiEXPTIME_micro = 0; // 333us
	constexpr int kiBinningType   = 1; // 1:1x1,2:2x2,3:3x3,4:4x4
    constexpr int kiZoomLeft      = 0;
    constexpr int kiZoomTop       = 0;
	constexpr int kiZoomWidth     = 3072; // 0 means no zoom
	constexpr int kiZoomHeight    = 3072; // 0 means no zoom

	CHBIDeviceCtrl cCHBIDeviceCtrl;
	CHBIDeviceProperties cHBIDeviceProperties;

    // initialize
    bool result = false;
    result = cCHBIDeviceCtrl.Initialize();
    std::wcout << L"Initialize: " << (result ? L"Success" : L"Failed") << std::endl;

    cCHBIDeviceCtrl.SetCallBackFun();

    result = cCHBIDeviceCtrl.ConectJumbo(kpcFPDIP, kusFPDPORT, kpcPCIP, kusPCPORT);
    std::wcout << L"ConectJumbo: " << (result ? L"Success" : L"Failed") << std::endl;
    cCHBIDeviceCtrl.GetFPDStatus();
    if (!result) {
		std::cerr << "Failed to connect to the device. Exiting.\n";
		return -1;
    }

	// Capture Start
    cCHBIDeviceCtrl.SetCaptureParams(
        kiGAINLEVEL,
        kiEXPTIME_milli,
        kiEXPTIME_micro,
        kiFRAMECOUNT,
        kiBinningType,
        kiZoomLeft, // ZoomLeft
        kiZoomTop, // ZoomTop
        kiZoomWidth, // ZoomWidth
        kiZoomHeight // ZoomHeight
    );
    cCHBIDeviceCtrl.GetCaptureParams();

    result = cCHBIDeviceCtrl.UpdateProperties();
    if (!result) {
        std::cerr << "Failed to get image property. Exiting.\n";
        return -1;
    }

    cCHBIDeviceCtrl.AllocateImageBuffer(kiFRAMECOUNT);

    if (cCHBIDeviceCtrl.Capture()) {
	    while (cCHBIDeviceCtrl.IsCapturering()) {
		    std::this_thread::sleep_for(
			    std::chrono::milliseconds(50)
		    );
        }
		cCHBIDeviceCtrl.StopCapture();
	}

	cCHBIDeviceCtrl.Release();

    // Save Image
    std::string strSaveFilePath = "D:\\github\\CapturerByHBI\\CapturerByHBI\\data\\Test.tif";
    SaveAsMultiFrameTiff(
        strSaveFilePath,
        cCHBIDeviceCtrl.m_vechbiimagebuffer,
        cCHBIDeviceCtrl.m_iimageWidth,
        cCHBIDeviceCtrl.m_iimageHeight,
        kiFRAMECOUNT
    );
	std::wcout << L"Done.\n";
}