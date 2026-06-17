
#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>
#include <sstream>
#include <tiffio.h>
#include <numeric>
#include <string>
#include <nlohmann/json.hpp>

#include "HbiFpd.h"
#include "HbiType.h"
#include "HbiError.h"

#include "./HBIDeviceCtrl.h"

void SaveAsMultiFrameTiff(
    const std::string& filename,
    const std::vector<uint16_t>& buffer,
    unsigned int width,
    unsigned int height,
    unsigned int frameCount
)
// CBigTIFF の代用 マルチフレームtifの保存に使用
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
    // 撮影パラメータ
    int m_iGainType        = 0;
    int m_iExpMili         = 0;
    int m_iCaptureFrame    = 0;
    int m_iBinningType     = 0;
    int m_iOriginalWidth   = 0;
    int m_iOriginalHeight  = 0;
    int m_iZoomLeft        = 0;
    int m_iZoomTop         = 0;
    int m_iZoomWidth       = 0;
    int m_iZoomHeight      = 0;
};

bool LoadCaptureConfig(const std::wstring& wstrParamsJsonPath, const std::string& strProductCode, CaptureConfig& captureConfig) {
    // 撮影パラメータをjsonファイルから読み、構造体にいれるもの
    // SystemConstantsFile等の代用
    std::ifstream ifs(wstrParamsJsonPath);
    if (!ifs.is_open()) {
        std::wcerr << L"Failed to open: " << wstrParamsJsonPath << std::endl;
        return false;
    }

    try {
        std::string strParamsJsonText((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        std::vector<std::string> vecstrProductCodes;
        std::vector<std::string> vecstrObjectTexts;
        int iArrayDepth = 0;
        int iObjectDepth = 0;
        std::size_t objectStart = std::string::npos;
        bool bIsString = false;
        bool bIsEscaped = false;
        bool bIsCaptureTopLevelString = false;
        std::string strCurrentTopLevelString;


        for (std::size_t iIndex = 0; iIndex < strParamsJsonText.size(); ++iIndex) {
            const char ch = strParamsJsonText[iIndex];

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
                    objectStart = iIndex;
                }
                ++iObjectDepth;
                continue;
            }

            if (ch == '}') {
                if (iObjectDepth > 0) {
                    --iObjectDepth;
                    if (iObjectDepth == 0 && objectStart != std::string::npos) {
                        vecstrObjectTexts.emplace_back(strParamsJsonText.substr(objectStart, iIndex - objectStart + 1));
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

			if (itemProductCode == strProductCode) {
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
            std::cerr << "No matching ProductCode in JSON. ProductCode=" << strProductCode << std::endl;
            return false;
        }

        captureConfig.m_iGainType       = selected.value("gainType", 0);
        captureConfig.m_iExpMili        = selected.value("expMili", 0);
        captureConfig.m_iCaptureFrame   = selected.value("CaptureFrame", 0);
        captureConfig.m_iBinningType    = selected.value("binningType", 0);
        captureConfig.m_iOriginalWidth  = selected.value("originalWidth", 0);
        captureConfig.m_iOriginalHeight = selected.value("originalHeight", 0);
        captureConfig.m_iZoomLeft       = selected.value("zoomLeft", 0);
        captureConfig.m_iZoomTop        = selected.value("zoomTop", 0);
        captureConfig.m_iZoomWidth      = selected.value("zoomWidth", 0);
        captureConfig.m_iZoomHeight     = selected.value("zoomHeight", 0);

        std::wcout << "Loaded config :"
            << L"   gainType=" << captureConfig.m_iGainType << L"\n"
            << L"   expMili=" << captureConfig.m_iExpMili <<  L"\n"
            << L"   CaptureFrame=" << captureConfig.m_iCaptureFrame <<  L"\n"
            << L"   binningType=" << captureConfig.m_iBinningType <<  L"\n"
            << L"   originalWidth=" << captureConfig.m_iOriginalWidth <<  L"\n"
            << L"   originalHeight=" << captureConfig.m_iOriginalHeight <<  L"\n"
            << L"   zoomLeft=" << captureConfig.m_iZoomLeft << L"\n"
            << L"   zoomTop=" << captureConfig.m_iZoomTop << L"\n"
            << L"   zoomWidth=" << captureConfig.m_iZoomWidth <<  L"\n"
            << L"   zoomHeight=" << captureConfig.m_iZoomHeight
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
    CaptureConfig captureConfig;
    std::wstring wstrJsonFilePath = L"D:\\github\\CapturerByHBI\\CapturerByHBI\\CapturerByHBI\\DeviceParams.json";
    std::string strProductCode;

    constexpr const char* kpcFpdIpAddress = "192.168.10.40";
    constexpr const char* kpcPcIpAddress = "192.168.10.20";
    constexpr unsigned short kusFpdPort = 32897;
    constexpr unsigned short kusPcPort = 32896;



    CHBIDeviceCtrl cHbiDeviceCtrl;

    // initialize
    if (!cHbiDeviceCtrl.Initialize()) {
		std::cerr << "Failed to initialize HBI. Exiting.\n";
		return -1;
	}

    cHbiDeviceCtrl.SetCallbackFunction();

    if (!cHbiDeviceCtrl.ConnectJumbo(kpcFpdIpAddress, kusFpdPort, kpcPcIpAddress, kusPcPort)) {
		std::cerr << "Failed to connect to the device. Exiting.\n";
		return -1;
    }

	//  接続後、安定するまで少し待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    if (!cHbiDeviceCtrl.IsConnected()) {
        std::cerr << "Device is not connected. Exiting.\n";
		return -1;
    }

    if (!cHbiDeviceCtrl.GetSDKVersion()) {
        std::cerr << "Failed to get HBI status. Exiting.\n";
    }

    if (!cHbiDeviceCtrl.GetFPDSerialNumber()) {
        std::cerr << "Failed to Get FPD Serial Number. Exiting.\n";
        return -1;
    }

    strProductCode = cHbiDeviceCtrl.GetFPDProductCode();
    if (strProductCode.empty()) {
        std::cerr << "Failed to Get FPD Product Code. Exiting.\n";
        return -1;
    }

    if (!LoadCaptureConfig(wstrJsonFilePath, strProductCode, captureConfig)) {
        std::cerr << "Failed to load capture config for product code. Exiting.\n";
        return -1;
    }

    const int kiCaptureFrame   = captureConfig.m_iCaptureFrame;
    const int kiGainLevel      = captureConfig.m_iGainType;          // 1: 0.6, 2: 1.2PC, 3: 2.4PC, 4: 3.6PC, 5: 4.8PC, 6: 7.2PC, 8: LFW, 9: HFW, 10: 0.3PC, 11: 0.15PC
    const int kiExpTimeMilli   = captureConfig.m_iExpMili;
    const int kiBinningType    = captureConfig.m_iBinningType;       // 1:1x1,2:2x2,3:3x3,4:4x4
    const int kiOriginalWidth  = captureConfig.m_iOriginalWidth;
    const int kiOriginalHeight = captureConfig.m_iOriginalHeight;
    const int kiZoomWidth      = captureConfig.m_iZoomWidth;         // 現在のパネル仕様では横方向のオフセットはできないため使用しない
    const int kiZoomHeight     = captureConfig.m_iZoomHeight;        // 縦方向のサイズ。オフセットを自由にできる
    const int kiZoomLeft       = captureConfig.m_iZoomLeft;          // 現在のパネル仕様では横方向のオフセットはできないため使用しない
    const int kiZoomTop        = captureConfig.m_iZoomTop;           // 縦方向の開始座標


    if (strProductCode == ("X - Panel3030zFDM") && (kiZoomHeight % 2 != 0)) {
        // 3030zの場合は、デュアル読出しのため、中央から等間隔にオフセットする。そのため縦方向のサイズは偶数である必要がある
        std::cerr << "Zoom height must be a multiple of 2. Adjusting to nearest even number.\n";
        return -1;
    }


    if (!cHbiDeviceCtrl.SetCaptureParams(
        kiGainLevel,
        kiExpTimeMilli,
        kiCaptureFrame,
        kiBinningType,
        kiOriginalWidth,
        kiOriginalHeight,
		kiZoomLeft,
		kiZoomTop,
        kiZoomWidth,
		kiZoomHeight
    )) {
        std::cerr << "Failed to Set Capture Params. Exiting. \n";
        return -1;
    }


    if (!cHbiDeviceCtrl.GetCaptureParams()) {
        std::cerr << "Failed to Get Capture Params. Exiting.\n";
        return -1;
    }

    if (!cHbiDeviceCtrl.UpdateImageProperties()) {
        std::cerr << "Failed to get image property. Exiting.\n";
        return -1;
    }

    cHbiDeviceCtrl.AllocateImageBuffer(kiCaptureFrame);

    // Capture Start
    if (cHbiDeviceCtrl.StartCapture()) {
        while (cHbiDeviceCtrl.IsCapturing()) {
		    std::this_thread::sleep_for(
			    std::chrono::milliseconds(50)
		    );
        }
        cHbiDeviceCtrl.StopCapture();
	}

    cHbiDeviceCtrl.ReleaseDevice();

    // Save Image
    std::string strSaveFilePath = "D:\\github\\CapturerByHBI\\CapturerByHBI\\data\\Test.tif";
    SaveAsMultiFrameTiff(
        strSaveFilePath,
        cHbiDeviceCtrl.GetImageBuffer(),
        cHbiDeviceCtrl.GetImageWidth(),
        cHbiDeviceCtrl.GetImageHeight(),
        kiCaptureFrame
    );
	std::wcout << L"Done.\n";
}