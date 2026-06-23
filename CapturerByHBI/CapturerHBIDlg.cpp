
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

// #include "./CTiff/Tiff.h"
//#include "./BigTIFF/BigTIFF.h"

#include "./HBIDeviceCtrl.h"

// 撮影パラメータをjsonファイルから読み、構造体にいれるもの
struct CCaptureConfig {
    // 撮影パラメータ
    int m_iGainType                = 0; // ゲインモード
	int m_imillisecExposureTime    = 0; // 露光時間(マイクロ秒)
    int m_iCaptureFrame            = 0; // 取得するフレーム数
    int m_iBinningType             = 0; // ビニングモード
    int m_iOriginalWidth           = 0; // 元画像の幅
    int m_iOriginalHeight          = 0; // 元画像の高さ
    int m_iCaptureAreaLeft         = 0; // 取得する画像の左端座標
    int m_iCaptureAreaTop          = 0; // 取得する画像の上端座標
    int m_iCaptureAreaWidth        = 0; // 取得する画像の幅
	int m_iCaptureAreaHeight       = 0; // 取得する画像の高さ
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
    const uint16_t*    kpuiImageBuffer,
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
        
    const size_t kiFramePixelCount    = static_cast<size_t>(kuiWidth) * static_cast<size_t>(kuiHeight);
    const size_t kiRequiredPixelCount = kiFramePixelCount * static_cast<size_t>(kuiFrameCount);
    
    // バッファのサイズが、要求されたフレーム数分の画像データを保存できるサイズであるか確認する。
    if (kiImageBufferSize < kiRequiredPixelCount) {
        std::cerr << "Buffer size is smaller than required for requested frame count\n";
		std::cerr << "Buffer size: " << kiImageBufferSize << ", Required size: " << kiRequiredPixelCount << "\n";
        return;
    }

    TIFF* tiff = TIFFOpen(kstrSaveFilePath.c_str(), "w");
    if (!tiff) {
        std::cerr << "TIFFOpen failed\n";
        return;
    }

    // フレームごとに保存する
    for (int iFrame = 0; iFrame < kuiFrameCount; ++iFrame) {
        TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH     , kuiWidth);
        TIFFSetField(tiff, TIFFTAG_IMAGELENGTH    , kuiHeight);
        TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE  , 16);
        TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP   , kuiHeight);
        TIFFSetField(tiff, TIFFTAG_COMPRESSION    , COMPRESSION_NONE);
        TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC    , PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tiff, TIFFTAG_PLANARCONFIG   , PLANARCONFIG_CONTIG);
        TIFFSetField(tiff, TIFFTAG_SUBFILETYPE    , 0);
        TIFFSetField(tiff, TIFFTAG_PAGENUMBER     , iFrame, kuiFrameCount);

        const size_t kiFrameOffset = static_cast<size_t>(iFrame) * kiFramePixelCount;
        for (int iIndexRow = 0; iIndexRow < kuiHeight; ++iIndexRow) {
            const uint16_t* puiIndexRow = &kpuiImageBuffer[kiFrameOffset + static_cast<size_t>(iIndexRow) * kuiWidth];
            if (TIFFWriteScanline(tiff, (void*)puiIndexRow, iIndexRow, 0) < 0) {
                std::cerr << "TIFFWriteScanline failed at frame " << iFrame << ", iIndexRow " << iIndexRow << "\n";
                TIFFClose(tiff);
                return;
            }
        }

        if (!TIFFWriteDirectory(tiff)) {
            std::cerr << "TIFFWriteDirectory failed at frame " << iFrame << "\n";
            TIFFClose(tiff);
            return;
        }
    }

    TIFFClose(tiff);
}

/**
 * @brief  指定されたJSONファイルから、指定されたProductCodeに対応する撮影パラメータを読み込む。
 * @param  wstrParamsJsonPath JSONファイルのパス。
 * @param  strProductCode     読み込む撮影パラメータに対応するProductCode。
 * @param  CaptureConfig      撮影パラメータを格納する構造体。
 * @return 撮影パラメータを保存する構造体を返す。
 */
struct CCaptureConfig LoadCaptureConfig(const std::wstring& wstrParamsJsonPath, const std::string& strProductCode, struct CCaptureConfig& CaptureConfig) {
    
    // SystemConstantsFile等の代用
    std::ifstream ifs(wstrParamsJsonPath);

	// ファイルが開けなかった場合はエラーを返す
    if (!ifs.is_open()) {
        std::wcerr << L"Failed to open: " << wstrParamsJsonPath << std::endl;
        return CaptureConfig;
    }

    try {
		// JSONファイルの内容を文字列として読み込む
        std::string strParamsJsonText((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

		std::vector<std::string> vecstrProductCodes; // ProductCodeを格納するベクトル。トップレベルの配列に存在する。
		std::vector<std::string> vecstrObjectTexts;  // オブジェクトのテキストを格納するベクトル。トップレベルの配列に存在する。
		std::string strCurrentTopLevelString; // トップレベルの配列内の文字列(ProductCode)をキャプチャするための文字列。
        std::size_t iObjectStart = std::string::npos;
        int iArrayDepth        = 0; // トップレベルの配列の深さを追跡するための変数。
        int iObjectDepth       = 0; // オブジェクトの深さを追跡するための変数。
        bool bIsString         = false;
        bool bIsEscaped        = false;
        bool bIsPushBackString = false;

		// JSON の文字を1文字ずつ読み出して、トップレベルの配列内の文字列(ProductCode)とオブジェクトを見つける。
        for (std::size_t iIndex = 0; iIndex < strParamsJsonText.size(); ++iIndex) {
            const char kcCurrentJsonChar = strParamsJsonText[iIndex];

			// 文字列の終わりにあるエスケープを探す。
            if (bIsString) {
                if (!bIsEscaped) {
                    if (bIsPushBackString) {
                        strCurrentTopLevelString.push_back(kcCurrentJsonChar);
                    }
                    bIsEscaped = true;
                    continue;
                }
				// 取ってきた1文字がエスケープのときは、次の文字をエスケープされた文字として扱うため、状態を更新して次のループに進む。
                if (kcCurrentJsonChar == '\\') {
                    if (bIsPushBackString) {
                        strCurrentTopLevelString.push_back(kcCurrentJsonChar);
                    }
                    bIsEscaped = false;
                    continue;
                }
                // エスケープされていない["]が来た時は文字列の終了を意味する。
                if (kcCurrentJsonChar == '"') {
                    bIsString = false;
                    if (bIsPushBackString) {
                        vecstrProductCodes.emplace_back(strCurrentTopLevelString);
                        strCurrentTopLevelString.clear();
                        bIsPushBackString = false;
                    }
                    continue;
                }
                if (bIsPushBackString) {
                    strCurrentTopLevelString.push_back(kcCurrentJsonChar);
                }
                continue;
            }

			// エスケープされていない["]が来た時は文字列の開始を意味する。
            if (kcCurrentJsonChar == '"') {
                bIsString = true;
				// トップレベルの配列の深さが1で、オブジェクトの深さが0のときに文字列をキャプチャするための状態を更新する。
                if (iArrayDepth == 1 && iObjectDepth == 0) {
                    bIsPushBackString = true;
                    strCurrentTopLevelString.clear();
                }
                continue;
            }

			// JSONの構造を追うため、配列の深さをカウントする。
			// トップレベルの配列の深さが1で、オブジェクトの深さが0のときに文字列をキャプチャする。
            if (kcCurrentJsonChar == '[') {
                iArrayDepth++;
                continue;
            }

            if (kcCurrentJsonChar == ']') {
                if (iArrayDepth > 0) {
                    iArrayDepth--;
                }
                continue;
            }

			// オブジェクトの深さをカウントする。
			// トップレベルの配列の中で、オブジェクトの深さが0のときに文字列をキャプチャする。
            if (kcCurrentJsonChar == '{') {
                if (iObjectDepth == 0) {
                    iObjectStart = iIndex;
                }
                iObjectDepth++;
                continue;
            }

            if (kcCurrentJsonChar == '}') {
                if (iObjectDepth > 0) {
                    iObjectDepth--;
        			// オブジェクトの終了時にPushBackした文字列(ProductCode)とオブジェクトのテキストをそれぞれのベクトルに入れる。
                    if (iObjectDepth == 0 && iObjectStart != std::string::npos) {
						// strParamsJsonText の { と } の間のテキストを vecstrObjectTexts に入れる。
                        vecstrObjectTexts.emplace_back(strParamsJsonText.substr(iObjectStart, iIndex - iObjectStart + 1));
                        iObjectStart = std::string::npos; // 初期化
                    }
                }
            }
        }

		// トップレベルの配列内の文字列(ProductCode)とオブジェクトが同数であるか、どちらかが空でないかを確認する。
        if (vecstrProductCodes.empty() || vecstrObjectTexts.empty() || vecstrProductCodes.size() != vecstrObjectTexts.size()) {
            std::cerr << "Config format error. Expected [\"ProductCode\", { ... }] pairs." << std::endl;
            return CaptureConfig;
        }

		nlohmann::json objCaptureParams; // ProductCodeにマッチしたオブジェクトを格納するためのJSONオブジェクト。
        bool bIsMachProductCode  = false; // ProductCodeがマッチしたかどうかのフラグ。
		bool bIsEmptyProductCode = false; // ProductCodeが空のものがあったかどうかのフラグ。ProductCodeが見つからない場合はこれを使用する。

		// オブジェクトごとにProductCodeを比較し、マッチしたオブジェクトのJSONをobjCaptureParamsに格納する。
        for (std::size_t iX = 0; iX < vecstrObjectTexts.size(); ++iX) {
            const std::string& kstrProductCode = vecstrProductCodes[iX];
            const std::string& kstrObjectText  = vecstrObjectTexts[iX];

            std::istringstream InputStream(kstrObjectText);
            nlohmann::json CurrentObjectText;
            InputStream >> CurrentObjectText;
			// JSONのパースに失敗した場合は次のオブジェクトに進む。
            if (InputStream.fail()) {
                continue;
            }

			// ProductCodeがマッチした場合はobjCaptureParamsに格納し、ループを抜ける。
            if (kstrProductCode == strProductCode) {
                objCaptureParams   = CurrentObjectText;
                bIsMachProductCode = true;
                break;
            }

			// ProductCodeが空のものがあった場合は、objCaptureParamsに格納する。ただし、ProductCodeがマッチした場合は上書きされない。
            if (!bIsEmptyProductCode && kstrProductCode.empty()) {
                objCaptureParams    = CurrentObjectText;
                bIsEmptyProductCode = true;
            }
        }
        
		// 何もマッチしなかった場合はエラーを返す。
        if (!bIsMachProductCode && !bIsEmptyProductCode) {
            std::cerr << "No matching ProductCode in JSON. ProductCode=" << strProductCode << std::endl;
            return CaptureConfig;
        }

        CaptureConfig.m_iGainType               = objCaptureParams.value("GainType"            , 0);
        CaptureConfig.m_imillisecExposureTime   = objCaptureParams.value("millisecExposureTime", 0);
        CaptureConfig.m_iCaptureFrame           = objCaptureParams.value("CaptureFrame"        , 0);
        CaptureConfig.m_iBinningType            = objCaptureParams.value("BinningType"         , 0);
        CaptureConfig.m_iOriginalWidth          = objCaptureParams.value("OriginalWidth"       , 0);
        CaptureConfig.m_iOriginalHeight         = objCaptureParams.value("OriginalHeight"      , 0);
        CaptureConfig.m_iCaptureAreaLeft        = objCaptureParams.value("CaptureAreaLeft"     , 0);
        CaptureConfig.m_iCaptureAreaTop         = objCaptureParams.value("CaptureAreaTop"      , 0);
        CaptureConfig.m_iCaptureAreaWidth       = objCaptureParams.value("CaptureAreaWidth"    , 0);
        CaptureConfig.m_iCaptureAreaHeight      = objCaptureParams.value("CaptureAreaHeight"   , 0);
    }

    catch (const std::exception& error) {
        std::cerr << "JSON parse error: " << error.what() << std::endl;
        return CaptureConfig;
    }

	std::cout << "GainType: " << CaptureConfig.m_iGainType << std::endl;
	std::cout << "ExposureTime: " << CaptureConfig.m_imillisecExposureTime << std::endl;
	std::cout << "CaptureFrame: " << CaptureConfig.m_iCaptureFrame << std::endl;
	std::cout << "BinningType: " << CaptureConfig.m_iBinningType << std::endl;
	std::cout << "OriginalWidth: " << CaptureConfig.m_iOriginalWidth << std::endl;
	std::cout << "OriginalHeight: " << CaptureConfig.m_iOriginalHeight << std::endl;
	std::cout << "CaptureAreaLeft: " << CaptureConfig.m_iCaptureAreaLeft << std::endl;
	std::cout << "CaptureAreaTop: " << CaptureConfig.m_iCaptureAreaTop << std::endl;
	std::cout << "CaptureAreaWidth: " << CaptureConfig.m_iCaptureAreaWidth << std::endl;
	std::cout << "CaptureAreaHeight: " << CaptureConfig.m_iCaptureAreaHeight << std::endl;

    return CaptureConfig;
}



int main()
{
    CCaptureConfig CaptureConfig;
    const std::wstring kwstrJsonFilePath  = L"D:\\github\\CapturerByHBI\\CapturerByHBI\\CapturerByHBI\\DeviceParams.json"; // パラメータを読むJSONファイル
	constexpr const char* kpcFpdIpAddress = "192.168.10.40"; // FPDのIPアドレス
    constexpr const char* kpcPcIpAddress  = "192.168.10.20"; // PCのIPアドレス
    constexpr unsigned short kusFpdPort   = 32897; // FPDのポート番号
    constexpr unsigned short kusPcPort    = 32896; // PCのポート番号

    CHBIDeviceCtrl HbiDeviceCtrl;

    // initialize
    if (!HbiDeviceCtrl.Initialize()) {
		std::cerr << "Failed to initialize HBI. Exiting.\n";
		return -1;
	}

    // SDK のイベントコールバック関数の設定。イベントが起こった時、SDK が UserHBICallback を呼び出す。
    HbiDeviceCtrl.SetCallbackFunction();

    if (!HbiDeviceCtrl.ConnectJumbo(kpcFpdIpAddress, kusFpdPort, kpcPcIpAddress, kusPcPort)) {
		std::cerr << "Failed to connect to the device. Exiting.\n";
		return -1;
    }

	//  接続後、安定するまで少し待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 接続状態の確認。
    if (!HbiDeviceCtrl.IsConnected()) {
        std::cerr << "Device is not connected. Exiting.\n";
		return -1;
    }

    // デバイスの情報を取得する。
	std::string strSDKVersion = HbiDeviceCtrl.GetSDKVersion();
    if (strSDKVersion.empty()) {
        std::cerr << "Failed to get HBI status. Exiting.\n";
    }

    // デバイスのシリアルナンバーを取得する。
	std::string strSerialNumber = HbiDeviceCtrl.GetFPDSerialNumber();
    if (strSerialNumber.empty()) {
        std::cerr << "Failed to Get FPD Serial Number. Exiting.\n";
        return -1;
    }

    // デバイスの製品コードを取得する。
	std::string strProductCode = HbiDeviceCtrl.GetFPDProductCode();
    if (strProductCode.empty()) {
        std::cerr << "Failed to Get FPD Product Code. Exiting.\n";
        return -1;
    }

    // ProductCodeをもとに、JSONファイルから撮影パラメータを読み込む。
    CaptureConfig = LoadCaptureConfig(kwstrJsonFilePath, strProductCode, CaptureConfig);
    if (CaptureConfig.m_iCaptureFrame == 0) {
        std::cerr << "Failed to load capture config for product code. Exiting.\n";
        return -1;
    }

    const int kiCaptureFrame      = CaptureConfig.m_iCaptureFrame;
    const int kiGainType          = CaptureConfig.m_iGainType;                 // 1: 0.6, 2: 1.2PC, 3: 2.4PC, 4: 3.6PC, 5: 4.8PC, 6: 7.2PC, 8: LFW, 9: HFW, 10: 0.3PC, 11: 0.15PC
    const int kimillisecExpTime   = CaptureConfig.m_imillisecExposureTime;
    const int kiBinningType       = CaptureConfig.m_iBinningType;              // 1:1x1,2:2x2,3:3x3,4:4x4
    const int kiOriginalWidth     = CaptureConfig.m_iOriginalWidth;
    const int kiOriginalHeight    = CaptureConfig.m_iOriginalHeight;
    const int kiCaptureAreaWidth  = CaptureConfig.m_iCaptureAreaWidth;         // 現在のパネル仕様では横方向のオフセットはできないため使用しない
    const int kiCaptureAreaHeight = CaptureConfig.m_iCaptureAreaHeight;        // 縦方向のサイズ。オフセットを自由にできる
    const int kiCaptureAreaLeft   = CaptureConfig.m_iCaptureAreaLeft;          // 現在のパネル仕様では横方向のオフセットはできないため使用しない
    const int kiCaptureAreaTop    = CaptureConfig.m_iCaptureAreaTop;           // 縦方向の開始座標


    if (strProductCode == ("X-Panel3030zFDM") && (kiCaptureAreaHeight % 2 != 0)) {
        // 3030zの場合は、デュアル読出しのため、中央から等間隔にオフセットする。そのため縦方向のサイズは偶数である必要がある
        std::cerr << "Zoom height must be a multiple of 2. Adjusting to nearest even number.\n";
        return -1;
    }
    // 撮影パラメータをデバイスに設定する前に、安定するまで少し待つ。
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 撮影パラメータをデバイスに設定する。
    if (!HbiDeviceCtrl.SetCaptureParams(
        kiGainType,
        kimillisecExpTime,
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


    // 撮影パラメータをデバイスに設定した後、確認のために出力する。
    if (!HbiDeviceCtrl.PrintCaptureParams()) {
        std::cerr << "Failed to Get Capture Params. Exiting.\n";
        return -1;
    }

    // 画像プロパティを更新する。
    if (!HbiDeviceCtrl.UpdateImageProperties()) {
        std::cerr << "Failed to get image property. Exiting.\n";
        return -1;
    }

    // 画像バッファを確保する。
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