
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
IMAGE_PROPERTY HBIDeviceProperties::m_imgProp = {};
IMAGE_PROPERTY CHBIDeviceCtrl::m_imgProp = {};
FPD_AQC_MODE CHBIDeviceCtrl::m_aqcMode = {};
IMAGE_DATA_ST CHBIDeviceCtrl::m_imgData = {};
COMM_CFG CHBIDeviceCtrl::m_commCfg = {};
std::vector<uint16_t> CHBIDeviceCtrl::m_vechbiimagebuffer = {};

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


	CHBIDeviceCtrl cCHBIDeviceCtrl;
	HBIDeviceProperties cHBIDeviceProperties;

    // initialize
    bool result = false;
    result = cCHBIDeviceCtrl.Initialize();
    std::wcout << L"Initialize: " << (result ? L"Success" : L"Failed") << std::endl;

    cCHBIDeviceCtrl.SetCallBackFun();

    result = cCHBIDeviceCtrl.ConectJumbo();
    std::wcout << L"ConectJumbo: " << (result ? L"Success" : L"Failed") << std::endl;

    if (!result) {
		std::cerr << "Failed to connect to the device. Exiting.\n";
		return -1;
    }

    result = cCHBIDeviceCtrl.UpdateProperties();
    std::wcout << L"UpdateProperties: " << (result ? L"Success" : L"Failed") << std::endl;
    if (!result) {
        std::cerr << "Failed to get image property. Exiting.\n";
        return -1;
    }

    cCHBIDeviceCtrl.SetImageBuffer(cCHBIDeviceCtrl.m_itotalCaptureFrame);

	// Capture Start
    cCHBIDeviceCtrl.SetCaptureParams(
        cCHBIDeviceCtrl.kiGAINLEVEL,
        cCHBIDeviceCtrl.kiEXPTIME_milli,
        cCHBIDeviceCtrl.kiEXPTIME_micro
    );
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
    std::string strSaveFilePath = "D:\\github\\CapturerByHBI\\CapturerByHBI\\data\\20260423_Test_100Frame.tif";
    SaveAsMultiFrameTiff(
        strSaveFilePath,
        cCHBIDeviceCtrl.m_vechbiimagebuffer,
		cHBIDeviceProperties.GetImageWidth(),
		cHBIDeviceProperties.GetImageHeight(),
        cCHBIDeviceCtrl.m_itotalCaptureFrame
    );
}