
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <cstring>
#include <sstream>
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
    std::string m_FPDIP;
    std::string m_PCIP;
    int m_iFPDPORT;
    int m_iPCORT;
	int m_iFPDProductCode  = 0;
    int m_igainType        = 0;
    int m_iexpMili         = 0;
    int m_iCaptureFrame    = 0;
    int m_ibinningType     = 0;
	int m_ioriginalWidth   = 0;
	int m_ioriginalHeight  = 0;
	int m_izoomLeft        = 0;
	int m_izoomTop         = 0;
	int m_izoomWidth       = 0;
	int m_izoomHeight      = 0;
};

bool LoadCaptureConfig(const std::wstring& strParamsJSONPath, const std::string& strProductCodes, CaptureConfig& CaptureConfig) {
    std::ifstream ifs(strParamsJSONPath);
    if (!ifs.is_open()) {
        std::wcerr << L"Failed to open: " << strParamsJSONPath << std::endl;
        return false;
    }

    try {
        std::string strParamsJSONText((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        std::vector<std::string> vecstrProductCodes;
        std::vector<std::string> vecstrObjectTexts;
        int iArrayDepth = 0;
        int iObjectDepth = 0;
        std::size_t objectStart = std::string::npos;
        bool bIsString = false;
        bool bIsEscaped = false;
        bool bIsCaptureTopLevelString = false;
        std::string strCurrentTopLevelString;


        for (std::size_t i = 0; i < strParamsJSONText.size(); ++i) {
            const char ch = strParamsJSONText[i];

            if (bIsString) {
                if (bIsEscaped) {
                    if (bIsCaptureTopLevelString) {
                        strCurrentTopLevelString.push_back(ch);
                    }
                    bIsEscaped = false;
                    continue;
                }
                if (ch == '\\') {
                    if (bIsCaptureTopLevelString) {
                        strCurrentTopLevelString.push_back(ch);
                    }
                    bIsEscaped = true;
                    continue;
                }
                if (ch == '"') {
                    bIsString = false;
                    if (bIsCaptureTopLevelString) {
                        vecstrProductCodes.emplace_back(strCurrentTopLevelString);
                        strCurrentTopLevelString.clear();
                        bIsCaptureTopLevelString = false;
                    }
                    continue;
                }
                if (bIsCaptureTopLevelString) {
                    strCurrentTopLevelString.push_back(ch);
                }
                continue;
            }

            if (ch == '"') {
                bIsString = true;
                if (iArrayDepth == 1 && iObjectDepth == 0) {
                    bIsCaptureTopLevelString = true;
                    strCurrentTopLevelString.clear();
                }
                continue;
            }

            if (ch == '[') {
                ++iArrayDepth;
                continue;
            }

            if (ch == ']') {
                if (iArrayDepth > 0) {
                    --iArrayDepth;
                }
                continue;
            }

            if (ch == '{') {
                if (iObjectDepth == 0) {
                    objectStart = i;
                }
                ++iObjectDepth;
                continue;
            }

            if (ch == '}') {
                if (iObjectDepth > 0) {
                    --iObjectDepth;
                    if (iObjectDepth == 0 && objectStart != std::string::npos) {
                        vecstrObjectTexts.emplace_back(strParamsJSONText.substr(objectStart, i - objectStart + 1));
                        objectStart = std::string::npos;
                    }
                }
            }
        }

        if (vecstrProductCodes.empty() || vecstrObjectTexts.empty() || vecstrProductCodes.size() != vecstrObjectTexts.size()) {
            std::cerr << "Config format error. Expected [\"ProductCode\", { ... }] pairs." << std::endl;
            return false;
        }

        nlohmann::json selected;
        bool foundMatch = false;
        bool foundFallback = false;

        for (std::size_t idx = 0; idx < vecstrObjectTexts.size(); ++idx) {
            const std::string& itemProductCode = vecstrProductCodes[idx];
            const std::string& objText = vecstrObjectTexts[idx];

            std::istringstream iss(objText);
            nlohmann::json current;
            iss >> current;
            if (iss.fail()) {
                continue;
            }

            if (itemProductCode == strProductCodes) {
                selected = current;
                foundMatch = true;
                break;
            }

            if (!foundFallback && itemProductCode.empty()) {
                selected = current;
                foundFallback = true;
            }
        }

        if (!foundMatch && !foundFallback) {
            std::cerr << "No matching ProductCode in JSON. ProductCode=" << strProductCodes << std::endl;
            return false;
        }

        CaptureConfig.m_igainType       = selected.value("gainType", 0);
        CaptureConfig.m_iexpMili        = selected.value("expMili", 0);
        CaptureConfig.m_iCaptureFrame   = selected.value("CaptureFrame", 0);
        CaptureConfig.m_ibinningType    = selected.value("binningType", 0);
		CaptureConfig.m_ioriginalWidth  = selected.value("originalWidth", 0);
		CaptureConfig.m_ioriginalHeight = selected.value("originalHeight", 0);
        CaptureConfig.m_izoomWidth      = selected.value("zoomWidth", 0);
        CaptureConfig.m_izoomHeight     = selected.value("zoomHeight", 0);
		CaptureConfig.m_izoomLeft = selected.value("zoomLeft", 0);
		CaptureConfig.m_izoomTop = selected.value("zoomTop", 0);
		CaptureConfig.m_izoomWidth = selected.value("zoomWidth", 0);
		CaptureConfig.m_izoomHeight = selected.value("zoomHeight", 0);

        std::wcout << "Loaded config :"
			<< L"   gainType=" << CaptureConfig.m_igainType << L"\n"
			<< L"   expMili=" << CaptureConfig.m_iexpMili <<  L"\n"
			<< L"   CaptureFrame=" << CaptureConfig.m_iCaptureFrame <<  L"\n"
			<< L"   binningType=" << CaptureConfig.m_ibinningType <<  L"\n"
			<< L"   originalWidth=" << CaptureConfig.m_ioriginalWidth <<  L"\n"
			<< L"   originalHeight=" << CaptureConfig.m_ioriginalHeight <<  L"\n"
			<< L"   zoomLeft=" << CaptureConfig.m_izoomLeft << L"\n"
			<< L"   zoomTop=" << CaptureConfig.m_izoomTop << L"\n"
			<< L"   zoomWidth=" << CaptureConfig.m_izoomWidth <<  L"\n"
            << L"   zoomHeight=" << CaptureConfig.m_izoomHeight
			<< std::endl;
    }

    catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }

    return true;
}

int main()
{
	CaptureConfig CaptureConfig;
    std::wstring strjsonFilePath = L"D:\\github\\CapturerByHBI\\CapturerByHBI\\CapturerByHBI\\DeviceParams.json";
    std::string strProductCode;

    // パラメータの設定
    constexpr char* kpcm_FPDIP          = "192.168.10.40";
    constexpr char* kpcm_PCIP           = "192.168.10.20";
    constexpr unsigned short kusFPDPORT = 32897;
    constexpr unsigned short kusPCPORT  = 32896;



	CHBIDeviceCtrl cCHBIDeviceCtrl;

    // initialize
    bool result = false;

    result = cCHBIDeviceCtrl.Initialize();
	if (!result) {
		std::cerr << "Failed to initialize HBI. Exiting.\n";
		return -1;
	}

    cCHBIDeviceCtrl.SetCallBackFun();

    if (!cCHBIDeviceCtrl.ConectJumbo(kpcm_FPDIP, kusFPDPORT, kpcm_PCIP, kusPCPORT)) {
		std::cerr << "Failed to connect to the device. Exiting.\n";
		return -1;
    }

    ::this_thread::sleep_for(std::chrono::seconds(1));
    
    if (!cCHBIDeviceCtrl.IsConnected()) {
        std::cerr << "Device is not connected. Exiting.\n";
		return -1;
    }

    if (!cCHBIDeviceCtrl.GetSDKVersion()) {
        std::cerr << "Failed to get HBI status. Exiting.\n";
    }

    if (!cCHBIDeviceCtrl.GetFPDSerialNumber()) {
        std::cerr << "Failed to Get FPD Serial Number. Exiting.\n";
        return -1;
    }

	strProductCode = cCHBIDeviceCtrl.GetFPDProductCode();
    if (strProductCode.empty()) {
        std::cerr << "Failed to Get FPD Product Code. Exiting.\n";
        return -1;
    }

    // Produce Code に対応した撮影パラメータを取得する
    if (!LoadCaptureConfig(strjsonFilePath, strProductCode, CaptureConfig)) {
        std::cerr << "Failed to load capture config for product code. Exiting.\n";
        return -1;
    }

    const int kiCAPTUREFRAME = CaptureConfig.m_iCaptureFrame;
    const int kiGAINLEVEL = CaptureConfig.m_igainType;          // 1: 0.6, 2: 1.2PC, 3:  2.4PC, 4: 3.6PC, 5: 4.8PC, 6: 7.2PC, 8: LFW, 9: HFW, 10: 0.3PC, 11: 0.15PC
    const int kiEXPTIME_milli = CaptureConfig.m_iexpMili;
    const int kiBinningType = CaptureConfig.m_ibinningType;       // 1:1x1,2:2x2,3:3x3,4:4x4
    const int kiOriginalWidth = CaptureConfig.m_ioriginalWidth;
    const int kiOriginalHeight = CaptureConfig.m_ioriginalHeight;
    const int kiZoomWidth = CaptureConfig.m_izoomWidth;         // 横方向のクロップはできないため、実際には使用されない
    const int kiZoomHeight = CaptureConfig.m_izoomHeight;
    const int kiZoomLeft = CaptureConfig.m_izoomLeft;
    const int kiZoomTop = CaptureConfig.m_izoomTop;
    const int kiZoomWith = CaptureConfig.m_izoomWidth;

    if (kiZoomHeight % 2 != 0) {
        std::cerr << "Zoom height must be a multiple of 2. Adjusting to nearest even number.\n";
        return -1;
    }

    ::this_thread::sleep_for(std::chrono::seconds(1));

    if (!cCHBIDeviceCtrl.SetCaptureParams(
        kiGAINLEVEL,
        kiEXPTIME_milli,
        kiCAPTUREFRAME,
        kiBinningType,
        kiOriginalWidth,
        kiOriginalHeight,
		kiZoomLeft,
		kiZoomTop,
		kiZoomWith,
		kiZoomHeight
    )) {
        std::cerr << "Failed to Set Capture Params. Exiting. \n";
        return -1;
    }

    ::this_thread::sleep_for(std::chrono::seconds(3));

    if (!cCHBIDeviceCtrl.GetCaptureParams()) {
        std::cerr << "Failed to Get Capture Params. Exiting.\n";
        return -1;
    }
    ::this_thread::sleep_for(std::chrono::seconds(1));

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
    SaveAsMultiFrameTiff(
        strSaveFilePath,
        cCHBIDeviceCtrl.GetvecHBIimagebuffer(),
        cCHBIDeviceCtrl.GetImageWidth(),
        cCHBIDeviceCtrl.GetImageHeight(),
        kiCAPTUREFRAME
    );
	std::wcout << L"Done.\n";
}