
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <cstring>
#include <tiffio.h>
#include <numeric>
#include <string>
#include <nlohmann/json.hpp>

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


struct CaptureConfig {
    std::string FPDIP;
    std::string PCIP;
    int FPDPORT;
    int PCPORT;
    int gainType = 0;
    int expMili = 0;
    int CaptureFrame = 0;
    int binningType = 0;
    int zoomWidth = 0;
    int zoomHeight = 0; // 高さ方向のみ変えられる
};

bool LoadCaptureConfig(const std::wstring& path, CaptureConfig& cfg) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::wcerr << L"Failed to open: " << path << std::endl;
        return false;
    }

    try {
        nlohmann::json j;
        ifs >> j;

        cfg.gainType     = j.value("gainType", 0);
        cfg.expMili      = j.value("expMili", 0);
        cfg.CaptureFrame = j.value("CaptureFrame", 0);
        cfg.binningType  = j.value("binningType", 0);
        cfg.zoomWidth    = j.value("zoomWidth", 0);
        cfg.zoomHeight   = j.value("zoomHeight", 0);
    }
    catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }

    return true;
}

int main()
{
	CaptureConfig cfg;
    std::wstring strjsonFilePath = L"D:\\github\\CapturerByHBI\\CapturerByHBI\\CapturerByHBI\\DeviceParams.json";
    LoadCaptureConfig(strjsonFilePath, cfg);

    constexpr char* kpcFPDIP            = "192.168.10.40";
    constexpr char* kpcPCIP             = "192.168.10.20";
    constexpr unsigned short kusFPDPORT = 32897;
    constexpr unsigned short kusPCPORT  = 32896;

    const int kiCAPTUREFRAME  = cfg.CaptureFrame; 
	const int kiGAINLEVEL     = cfg.gainType;          // 1: 0.6, 2: 1.2PC, 3:  2.4PC, 4: 3.6PC, 5: 4.8PC, 6: 7.2PC, 8: LFW, 9: HFW, 10: 0.3PC, 11: 0.15PC
    const int kiEXPTIME_milli = cfg.expMili;       
	const int kiBinningType   = cfg.binningType;       // 1:1x1,2:2x2,3:3x3,4:4x4
	// const int kiZoomWidth     = cfg.zoomWidth;         // 横方向のズームはできない
	const int kiZoomHeight    = cfg.zoomHeight;        // 縦方向のズームサイズ
    

	if (kiZoomHeight % 2 != 0) {
		std::cerr << "Zoom height must be a multiple of 2. Adjusting to nearest even number.\n";
        return -1;
	}

	CHBIDeviceCtrl cCHBIDeviceCtrl;

    // initialize
    bool result = false;
    cCHBIDeviceCtrl.Release();

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
    
    cCHBIDeviceCtrl.SetCaptureParams(
        kiGAINLEVEL,
        kiEXPTIME_milli,
        kiCAPTUREFRAME,
        kiBinningType,
        kiZoomHeight
    );
    cCHBIDeviceCtrl.GetCaptureParams();
    result = cCHBIDeviceCtrl.UpdateProperties();
    if (!result) {
        std::cerr << "Failed to get image property. Exiting.\n";
        return -1;
    }

    cCHBIDeviceCtrl.AllocateImageBuffer(kiCAPTUREFRAME);

    // Capture Start
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
    SaveAsMultiFrameTiff(// get使ってアクセスしよう
        strSaveFilePath,
        cCHBIDeviceCtrl.GetvecHBIimagebuffer(),
        cCHBIDeviceCtrl.GetImageWidth(),
        cCHBIDeviceCtrl.GetImageHeight(),
        kiCAPTUREFRAME
    );
	std::wcout << L"Done.\n";
}