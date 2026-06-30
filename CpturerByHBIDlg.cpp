// CpturerByHBI_rev2.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include "pch.h"
#include "framework.h"
#include "HBIDeviceCtrl.h"
#include "../CSmartLog/SmartLog.h"
#include "../BigTIFF/BigTIFF.h"
#include "../Registry/Registry.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 唯一のアプリケーション オブジェクトです

CWinApp theApp;

using namespace std;

struct CaptureConfig {
    int m_iGainType = 0; // ゲインモード
    int m_imillisecExposureTime = 0; // 露光時間(マイクロ秒)
    int m_iCaptureFrame = 0; // 取得するフレーム数
    int m_iBinningType = 0; // ビニングモード
    int m_iOriginalWidth = 0; // 元画像の幅
    int m_iOriginalHeight = 0; // 元画像の高さ
    int m_iCaptureAreaLeft = 0; // 取得する画像の左端座標
    int m_iCaptureAreaTop = 0; // 取得する画像の上端座標
    int m_iCaptureAreaWidth = 0; // 取得する画像の幅
    int m_iCaptureAreaHeight = 0; // 取得する画像の高さ

    // コンストラクタで初期化する
    CaptureConfig() = default;

    CaptureConfig(const std::wstring& wstrParamsJsonPath, const std::string& strProductCode)
    {
        *this = LoadCaptureConfig(wstrParamsJsonPath, strProductCode);
    }

    /**
     * @brief 指定されたJSONファイルから、指定されたProductCodeに対応する撮影パラメータを読み込む。
     * @param wstrParamsJsonPath JSONファイルのパス。
     * @param strProductCode 読み込む撮影パラメータに対応するProductCode。
     * @return 読み込んだ撮影パラメータ。
     */
    static CaptureConfig LoadCaptureConfig(const std::wstring& wstrParamsJsonPath, const std::string& strProductCode)
    {
        CaptureConfig captureConfig;

        std::ifstream ifs(wstrParamsJsonPath);
        if (!ifs.is_open()) {
            std::wcerr << L"Failed to open: " << wstrParamsJsonPath << std::endl;
            return captureConfig;
        }

        try {
            const std::string jsonText((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

            std::vector<std::string> productCodes;
            std::vector<std::string> objectTexts;
            std::string currentString;
            std::size_t objectStart = std::string::npos;
            int arrayDepth = 0;
            int objectDepth = 0;
            bool inString = false;
            bool escaped = false;
            bool captureString = false;

            for (std::size_t index = 0; index < jsonText.size(); ++index) {
                const char currentChar = jsonText[index];

                if (inString) {
                    if (!escaped) {
                        if (captureString) {
                            currentString.push_back(currentChar);
                        }
                        escaped = true;
                        continue;
                    }

                    if (currentChar == '\\') {
                        if (captureString) {
                            currentString.push_back(currentChar);
                        }
                        escaped = false;
                        continue;
                    }

                    if (currentChar == '"') {
                        inString = false;
                        if (captureString) {
                            productCodes.emplace_back(currentString);
                            currentString.clear();
                            captureString = false;
                        }
                        continue;
                    }

                    if (captureString) {
                        currentString.push_back(currentChar);
                    }
                    continue;
                }

                if (currentChar == '"') {
                    inString = true;
                    if (arrayDepth == 1 && objectDepth == 0) {
                        captureString = true;
                        currentString.clear();
                    }
                    continue;
                }

                if (currentChar == '[') {
                    ++arrayDepth;
                    continue;
                }

                if (currentChar == ']') {
                    if (arrayDepth > 0) {
                        --arrayDepth;
                    }
                    continue;
                }

                if (currentChar == '{') {
                    if (objectDepth == 0) {
                        objectStart = index;
                    }
                    ++objectDepth;
                    continue;
                }

                if (currentChar == '}') {
                    if (objectDepth > 0) {
                        --objectDepth;
                        if (objectDepth == 0 && objectStart != std::string::npos) {
                            objectTexts.emplace_back(jsonText.substr(objectStart, index - objectStart + 1));
                            objectStart = std::string::npos;
                        }
                    }
                }
            }

            if (productCodes.empty() || objectTexts.empty() || productCodes.size() != objectTexts.size()) {
                std::cerr << "Config format error. Expected [\"ProductCode\", { ... }] pairs." << std::endl;
                return captureConfig;
            }

            std::string matchedObjectText;
            bool hasMatchedProductCode = false;
            bool hasDefaultProductCode = false;

            for (std::size_t index = 0; index < objectTexts.size(); ++index) {
                const std::string& currentProductCode = productCodes[index];
                const std::string& currentObjectText = objectTexts[index];

                if (currentProductCode == strProductCode) {
                    matchedObjectText = currentObjectText;
                    hasMatchedProductCode = true;
                    break;
                }

                if (!hasDefaultProductCode && currentProductCode.empty()) {
                    matchedObjectText = currentObjectText;
                    hasDefaultProductCode = true;
                }
            }

            if (!hasMatchedProductCode && !hasDefaultProductCode) {
                std::cerr << "No matching ProductCode in JSON. ProductCode=" << strProductCode << std::endl;
                return captureConfig;
            }

            const auto getIntValue = [&matchedObjectText](std::initializer_list<const char*> keys) -> int {
                for (const char* key : keys) {
                    const std::string keyPattern = std::string("\"") + key + "\"";
                    const std::size_t keyPos = matchedObjectText.find(keyPattern);
                    if (keyPos == std::string::npos) {
                        continue;
                    }

                    std::size_t valuePos = matchedObjectText.find(':', keyPos + keyPattern.size());
                    if (valuePos == std::string::npos) {
                        continue;
                    }

                    ++valuePos;
                    while (valuePos < matchedObjectText.size() &&
                        (matchedObjectText[valuePos] == ' ' || matchedObjectText[valuePos] == '\t' ||
                         matchedObjectText[valuePos] == '\r' || matchedObjectText[valuePos] == '\n')) {
                        ++valuePos;
                    }

                    const char* numberStart = matchedObjectText.c_str() + valuePos;
                    char* numberEnd = nullptr;
                    const long value = std::strtol(numberStart, &numberEnd, 10);
                    if (numberEnd != numberStart) {
                        return static_cast<int>(value);
                    }
                }

                return 0;
            };

            captureConfig.m_iGainType = getIntValue({ "GainType" });
            captureConfig.m_imillisecExposureTime = getIntValue({ "millisecExposureTime", "msecExposureTime" });
            captureConfig.m_iCaptureFrame = getIntValue({ "CaptureFrame" });
            captureConfig.m_iBinningType = getIntValue({ "BinningType" });
            captureConfig.m_iOriginalWidth = getIntValue({ "OriginalWidth" });
            captureConfig.m_iOriginalHeight = getIntValue({ "OriginalHeight" });
            captureConfig.m_iCaptureAreaLeft = getIntValue({ "CaptureAreaLeft" });
            captureConfig.m_iCaptureAreaTop = getIntValue({ "CaptureAreaTop" });
            captureConfig.m_iCaptureAreaWidth = getIntValue({ "CaptureAreaWidth" });
            captureConfig.m_iCaptureAreaHeight = getIntValue({ "CaptureAreaHeight" });
        }
        catch (const std::exception& error) {
            std::cerr << "JSON parse error: " << error.what() << std::endl;
            return captureConfig;
        }

        return captureConfig;
    }


};

void CapturerByHBIMain() {
    LOG_BEGINF0(7, "MAIN: CapturerByHBIMain()");
    CHBIDeviceCtrl cHbiDeviceCtrl;

}

int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // MFC を初期化して、エラーの場合は結果を出力する
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: アプリケーションの動作を記述するコードをここに挿入してください。
            wprintf(L"致命的なエラー: MFC の初期化が失敗しました\n");
            nRetCode = 1;
        }
        else
        {
            // TODO: アプリケーションの動作を記述するコードをここに挿入してください。
            CSmartLog::GetiLogFileAccessKeyW(L"CapturerByHBILog.log");
            LOG_BEGINF0(7, "MAIN");
			CapturerByHBIMain();
            CBigTIFF TiffIn;
        }
    }
    else
    {
        // TODO: 必要に応じてエラー コードを変更してください
        wprintf(L"致命的なエラー: GetModuleHandle が失敗しました\n");
        nRetCode = 1;
    }

    return nRetCode;
}


