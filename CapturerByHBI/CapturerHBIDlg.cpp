
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

// 撮影パラメータをjsonファイルから読み、構造体にいれるもの
struct CCaptureConfig {
    // 撮影パラメータ
    int m_iGainType = 0;
    int msecExposureTime = 0;
    int m_iCaptureFrame = 0;
    int m_iBinningType = 0;
    int m_iOriginalWidth = 0;
    int m_iOriginalHeight = 0;
    int m_iCaptureAreaLeft = 0;
    int m_iCaptureAreaTop = 0;
    int m_iCaptureAreaWidth = 0;
    int m_iCaptureAreaHeight = 0;
};

/**
 * @brief 画像バッファに保存されたデータをマルチフレーム形式のTiffファイルで保存する。
 * @param filename 保存するTIFFファイルのパス。
 * @param buffer   画像データが保存されたバッファへのポインタ。フレームごとに連続して保存されている必要がある。
 * @param BufferSize bufferのサイズ。width * height * frameCount 以上である必要がある。
 * @param width    画像の幅。
 * @param height   画像の高さ。
 * @param frameCount 画像のフレーム数。
 * @return なし。
 * @note   CBigTIFF の代用。必要なものを入れてビルドするだけだが、できませんでした。
 */
void SaveAsMultiFrameTiff(
    const std::string& kstrSaveFilePath,
    const uint16_t*    kpImageBuffer,
	const size_t       kiImageBufferSize,
    const unsigned int kuiWidth,
    const unsigned int kuiHeight,
    const unsigned int kuiFrameCount
)
{
    std::cout << "Saving multi-frame TIFF: " << kstrSaveFilePath << "\n";
    
    // 引数に不備がないか確認する。
    if (kuiWidth == 0 || kuiHeight == 0 || kuiFrameCount == 0) {
        std::cerr << "Invalid image size or frame count\n";
        return;
    }
        
    const size_t kiFramePixelCount = static_cast<size_t>(kuiWidth) * static_cast<size_t>(kuiHeight);
    const size_t kiRequiredPixelCount = kiFramePixelCount * static_cast<size_t>(kuiFrameCount);
    
    // バッファのサイズが、要求されたフレーム数分の画像データを保存できるサイズであるか確認する。
    if (kiImageBufferSize < kiRequiredPixelCount) {
        std::cerr << "Buffer size is smaller than required for requested frame count\n";
		std::cerr << "Buffer size: " << kiImageBufferSize << ", Required size: " << kiRequiredPixelCount << "\n";
        return;
    }

    TIFF* tif = TIFFOpen(kstrSaveFilePath.c_str(), "w");
    if (!tif) {
        std::cerr << "TIFFOpen failed\n";
        return;
    }

    for (uint32_t frame = 0; frame < kuiFrameCount; ++frame) {
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, kuiWidth);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, kuiHeight);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, kuiHeight);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_SUBFILETYPE, 0);
        TIFFSetField(tif, TIFFTAG_PAGENUMBER, frame, kuiFrameCount);

        const size_t frameOffset = static_cast<size_t>(frame) * kiFramePixelCount;
        for (uint32_t row = 0; row < kuiHeight; ++row) {
            const uint16_t* rowPtr = &kpImageBuffer[frameOffset + static_cast<size_t>(row) * kuiWidth];
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

bool LoadCaptureConfig(const std::wstring& wstrParamsJsonPath, const std::string& strProductCode, struct CCaptureConfig& CaptureConfig) {
    
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

        CaptureConfig.m_iGainType       = selected.value("GainType", 0);
        CaptureConfig.msecExposureTime        = selected.value("msecExposureTime", 0);
        CaptureConfig.m_iCaptureFrame   = selected.value("CaptureFrame", 0);
        CaptureConfig.m_iBinningType    = selected.value("BinningType", 0);
        CaptureConfig.m_iOriginalWidth  = selected.value("OriginalWidth", 0);
        CaptureConfig.m_iOriginalHeight = selected.value("OriginalHeight", 0);
        CaptureConfig.m_iCaptureAreaLeft       = selected.value("CaptureAreaLeft", 0);
        CaptureConfig.m_iCaptureAreaTop        = selected.value("CaptureAreaTop", 0);
        CaptureConfig.m_iCaptureAreaWidth      = selected.value("CaptureAreaWidth", 0);
        CaptureConfig.m_iCaptureAreaHeight     = selected.value("CaptureAreaHeight", 0);

        std::wcout << "Loaded config :"
            << L"   GainType=" << CaptureConfig.m_iGainType << L"\n"
            << L"   msecExposureTime=" << CaptureConfig.msecExposureTime <<  L"\n"
            << L"   CaptureFrame=" << CaptureConfig.m_iCaptureFrame <<  L"\n"
            << L"   BinningType=" << CaptureConfig.m_iBinningType <<  L"\n"
            << L"   OriginalWidth=" << CaptureConfig.m_iOriginalWidth <<  L"\n"
            << L"   OriginalHeight=" << CaptureConfig.m_iOriginalHeight <<  L"\n"
            << L"   CaptureAreaLeft=" << CaptureConfig.m_iCaptureAreaLeft << L"\n"
            << L"   CaptureAreaTop=" << CaptureConfig.m_iCaptureAreaTop << L"\n"
            << L"   CaptureAreaWidth=" << CaptureConfig.m_iCaptureAreaWidth <<  L"\n"
            << L"   CaptureAreaHeight=" << CaptureConfig.m_iCaptureAreaHeight
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
    CCaptureConfig CaptureConfig;
    const std::wstring kwstrJsonFilePath = L"D:\\github\\CapturerByHBI\\CapturerByHBI\\CapturerByHBI\\DeviceParams.json";

    constexpr const char* kpcFpdIpAddress = "192.168.10.40";
    constexpr const char* kpcPcIpAddress  = "192.168.10.20";
    constexpr unsigned short kusFpdPort   = 32897;
    constexpr unsigned short kusPcPort    = 32896;

    CHBIDeviceCtrl HbiDeviceCtrl;

    // initialize
    if (!HbiDeviceCtrl.Initialize()) {
		std::cerr << "Failed to initialize HBI. Exiting.\n";
		return -1;
	}

    HbiDeviceCtrl.SetCallbackFunction();

    if (!HbiDeviceCtrl.ConnectJumbo(kpcFpdIpAddress, kusFpdPort, kpcPcIpAddress, kusPcPort)) {
		std::cerr << "Failed to connect to the device. Exiting.\n";
		return -1;
    }

	//  接続後、安定するまで少し待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    if (!HbiDeviceCtrl.IsConnected()) {
        std::cerr << "Device is not connected. Exiting.\n";
		return -1;
    }

	std::string strSDKVersion = HbiDeviceCtrl.GetSDKVersion();
    if (strSDKVersion.empty()) {
        std::cerr << "Failed to get HBI status. Exiting.\n";
    }

	std::string strSerialNumber = HbiDeviceCtrl.GetFPDSerialNumber();
    if (strSerialNumber.empty()) {
        std::cerr << "Failed to Get FPD Serial Number. Exiting.\n";
        return -1;
    }

    std::string strProductCode = HbiDeviceCtrl.GetFPDProductCode();
    if (strProductCode.empty()) {
        std::cerr << "Failed to Get FPD Product Code. Exiting.\n";
        return -1;
    }

    if (!LoadCaptureConfig(kwstrJsonFilePath, strProductCode, CaptureConfig)) {
        std::cerr << "Failed to load capture config for product code. Exiting.\n";
        return -1;
    }

    const int kiCaptureFrame   = CaptureConfig.m_iCaptureFrame;
    const int kiGainType      = CaptureConfig.m_iGainType;          // 1: 0.6, 2: 1.2PC, 3: 2.4PC, 4: 3.6PC, 5: 4.8PC, 6: 7.2PC, 8: LFW, 9: HFW, 10: 0.3PC, 11: 0.15PC
    const int kiExpTimeMilli   = CaptureConfig.msecExposureTime;
    const int kiBinningType    = CaptureConfig.m_iBinningType;       // 1:1x1,2:2x2,3:3x3,4:4x4
    const int kiOriginalWidth  = CaptureConfig.m_iOriginalWidth;
    const int kiOriginalHeight = CaptureConfig.m_iOriginalHeight;
    const int kiCaptureAreaWidth      = CaptureConfig.m_iCaptureAreaWidth;         // 現在のパネル仕様では横方向のオフセットはできないため使用しない
    const int kiCaptureAreaHeight     = CaptureConfig.m_iCaptureAreaHeight;        // 縦方向のサイズ。オフセットを自由にできる
    const int kiCaptureAreaLeft       = CaptureConfig.m_iCaptureAreaLeft;          // 現在のパネル仕様では横方向のオフセットはできないため使用しない
    const int kiCaptureAreaTop        = CaptureConfig.m_iCaptureAreaTop;           // 縦方向の開始座標


    if (strProductCode == ("X - Panel3030zFDM") && (kiCaptureAreaHeight % 2 != 0)) {
        // 3030zの場合は、デュアル読出しのため、中央から等間隔にオフセットする。そのため縦方向のサイズは偶数である必要がある
        std::cerr << "Zoom height must be a multiple of 2. Adjusting to nearest even number.\n";
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!HbiDeviceCtrl.SetCaptureParams(
        kiGainType,
        kiExpTimeMilli,
        kiCaptureFrame,
        kiBinningType,
        kiOriginalWidth,
        kiOriginalHeight,
		kiCaptureAreaLeft,
		kiCaptureAreaTop,
        kiCaptureAreaWidth,
		kiCaptureAreaHeight
    )) {
        std::cerr << "Failed to Set Capture Params. Exiting. \n";
        return -1;
    }


    if (!HbiDeviceCtrl.PrintCaptureParams()) {
        std::cerr << "Failed to Get Capture Params. Exiting.\n";
        return -1;
    }

    if (!HbiDeviceCtrl.UpdateImageProperties()) {
        std::cerr << "Failed to get image property. Exiting.\n";
        return -1;
    }

    HbiDeviceCtrl.AllocateImageBuffer(kiCaptureFrame);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Capture Start
    if (HbiDeviceCtrl.StartCapture()) {
        while (HbiDeviceCtrl.IsCapturing()) {
		    std::this_thread::sleep_for(
			    std::chrono::milliseconds(50)
		    );
        }
        HbiDeviceCtrl.StopCapture();
	}


    // Save Image
    std::string strSaveFilePath = "D:\\github\\CapturerByHBI\\CapturerByHBI\\data\\Test.tif";
    SaveAsMultiFrameTiff (
        strSaveFilePath,
        HbiDeviceCtrl.GetImageBuffer(),
        HbiDeviceCtrl.GetImageBufferSize(),
        HbiDeviceCtrl.GetImageWidth(),
        HbiDeviceCtrl.GetImageHeight(),
        kiCaptureFrame
    );

    HbiDeviceCtrl.ReleaseDevice();
	std::wcout << L"Done.\n";
}