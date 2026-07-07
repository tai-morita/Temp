#include <thread>
#include <fstream>
#include <chrono>
#include "pch.h"
#include "framework.h"
#include "../Common/nlohmann/json.hpp"
#include "../CSmartLog/SmartLog.h"

struct CaptureConfig {
    int m_iGainType;             // ゲインモード
    int m_imsExpTime;            // 露光時間(マイクロ秒)
    int m_iCaptureFrame;         // 取得するフレーム数
    int m_iBinningType;          // ビニングモード
    int m_iOriginalWidth;        // 元画像の幅
    int m_iOriginalHeight;       // 元画像の高さ
    int m_iCaptureAreaLeft;      // 取得する画像の左端座標
    int m_iCaptureAreaTop;       // 取得する画像の上端座標
    int m_iCaptureAreaWidth;     // 取得する画像の幅
    int m_iCaptureAreaHeight;    // 取得する画像の高さ

    std::vector<std::string> m_vecstrProductCodes; // ProductCodeを格納するベクトル。トップレベルの配列に存在する。
    std::vector<std::string> m_vecstrObjectTexts;  // オブジェクトのテキストを格納するベクトル。トップレベルの配列に存在する。

    // 外部から渡された設定ファイルとProductCodeで初期化する
    CaptureConfig(std::wstring& rwstrParamsJsonPath, std::string& rstrProductCode)
    {
		LOG_BEGINF0(7, "5s8E| CaptureConfig::CaptureConfig()");
		Clear();
		bool bIsLoadJsonFile = LoadJsonFile(rwstrParamsJsonPath);
        bool bIsFindCaptureConfig = FindCaptureConfig(rstrProductCode);
        if (bIsLoadJsonFile && bIsFindCaptureConfig) {
            // 成功した場合はログに出力する。
            LOG_INPROGRESSF("l7qt| Loaded CaptureConfig for");
            LOG_INPROGRESSF("steJ|    ProductCode      : %s", rstrProductCode.c_str());
            LOG_INPROGRESSF("bI88|    GainType         : %d", m_iGainType);
            LOG_INPROGRESSF("PPPL|    ExposureTime     : %d ms", m_imsExpTime);
            LOG_INPROGRESSF("HiSV|    CaptureFrame     : %d", m_iCaptureFrame);
            LOG_INPROGRESSF("3sGX|    BinningType      : %d", m_iBinningType);
            LOG_INPROGRESSF("KuNk|    OriginalWidth    : %d", m_iOriginalWidth);
            LOG_INPROGRESSF("DpL2|    OriginalHeight   : %d", m_iOriginalHeight);
            LOG_INPROGRESSF("6TdK|    CaptureAreaLeft  : %d", m_iCaptureAreaLeft);
            LOG_INPROGRESSF("2NYy|    CaptureAreaTop   : %d", m_iCaptureAreaTop);
            LOG_INPROGRESSF("jrHQ|    CaptureAreaWidth : %d", m_iCaptureAreaWidth);
            LOG_INPROGRESSF("Smr3|    CaptureAreaHeight: %d", m_iCaptureAreaHeight);
        }
        else {
            LOG_INPROGRESSF("l7qt| Failed to load CaptureConfig for Product Code: %s", rstrProductCode.c_str());
        }
    }

    /**
     * @brief 保持しているパラメータを初期化する
     */
    void Clear() {
        m_iGainType          = 0;
        m_imsExpTime         = 0;
        m_iCaptureFrame      = 0;
        m_iBinningType       = 0;
        m_iOriginalWidth     = 0;
        m_iOriginalHeight    = 0;
        m_iCaptureAreaLeft   = 0;
        m_iCaptureAreaTop    = 0;
        m_iCaptureAreaWidth  = 0;
        m_iCaptureAreaHeight = 0;
    }

    /**
     * @brief   指定されたJSONファイルを読み込み、トップレベルの配列に存在するProductCodeとオブジェクトのテキストをそれぞれのベクトルに格納する。
     * @param   krwstrParamsJsonPath JSONファイルのパス。
     * @return  true: 読み込みに成功, false: 読み込みに失敗
     * @details JSONファイルを文字列形式で読み込み、トップレベルの配列に存在するProductCodeとオブジェクトのテキストをそれぞれのベクトルに格納する。
     *          1 文字ずつ解析して、エスケープ文字で開始と終了を判断する。
     */
    bool LoadJsonFile(const std::wstring& krwstrParamsJsonPath) {
        LOG_BEGINF0(7, "EePC| CaptureConfig::LoadJsonFile()");
        std::ifstream ifs(krwstrParamsJsonPath);
        if (!ifs.is_open()) {
            LOG_INPROGRESSF("Ikny| Failed to open: %s", std::string(krwstrParamsJsonPath.begin(), krwstrParamsJsonPath.end()).c_str());
            return false;
        }
        // JSONファイルの内容を文字列として読み込む
        const std::string kstrParamsJsonText((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        std::size_t szObjectStart = std::string::npos;
        int iArrayDepth = 0; // トップレベルの配列の深さを追跡するための変数。
        int iObjectDepth = 0; // オブジェクトの深さを追跡するための変数。
        bool bIsString = false; // 文字列の中にいるかどうかを追跡するための変数。
        bool bIsEscaped = false; // 文字列内でエスケープされているかどうかを追跡するための変数。
        bool bIsPushBackString = false; // 文字列をベクトルに追加するかどうかを追跡するための変数。
        std::string strCurrentTopLevelString = ""; // トップレベルの配列内の文字列(ProductCode)をキャプチャするための変数。

        for (int iIndex = 0; iIndex < kstrParamsJsonText.size(); ++iIndex) {
            // １文字ずつ抽出して、文字列を抜き取る。
            const char kcCurrentJsonChar = kstrParamsJsonText[iIndex];

            // 現在は文字列なので、文字列の終わりにある["]を探す。
            if (bIsString) {
                // 直前がエスケープ(\)なら、この文字はそのまま文字列に取り込む。
                if (bIsEscaped) {
                    if (bIsPushBackString) {
                        strCurrentTopLevelString.push_back(kcCurrentJsonChar);
                    }
                    bIsEscaped = false;
                    continue;
                }
                // エスケープ開始。次の1文字をエスケープ文字として扱う。
                if (kcCurrentJsonChar == '\\') {
                    bIsEscaped = true;
                    continue;
                }
                // エスケープされていない["]が来た時は文字列の終了を意味する。
                if (kcCurrentJsonChar == '"') {
                    bIsString = false;
                    if (bIsPushBackString) {
                        // 文字列をベクトルに追加して、次の文字列をキャプチャするためにクリアする。
                        m_vecstrProductCodes.emplace_back(strCurrentTopLevelString);
                        strCurrentTopLevelString.clear();
                        bIsPushBackString = false;
                    }
                    continue;
                }
                // 文字列の中の文字はそのまま文字列に取り込む。
                if (bIsPushBackString) {
                    strCurrentTopLevelString.push_back(kcCurrentJsonChar);
                }
                continue;
            }

            // エスケープされていない["]が来た時は文字列の開始を意味する。
            if (kcCurrentJsonChar == '"') {
                bIsString = true;
                bIsEscaped = false;
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
                    szObjectStart = iIndex;
                }
                iObjectDepth++;
                continue;
            }

            if (kcCurrentJsonChar == '}') {
                if (iObjectDepth > 0) {
                    iObjectDepth--;
                    // オブジェクトの終了時にPushBackした文字列(ProductCode)とオブジェクトのテキストをそれぞれのベクトルに入れる。
                    if (iObjectDepth == 0 && szObjectStart != std::string::npos) {
                        // kstrParamsJsonText の { と } の間のテキストを m_vecstrObjectTexts に入れる。
                        m_vecstrObjectTexts.emplace_back(kstrParamsJsonText.substr(szObjectStart, iIndex - szObjectStart + 1));
                        szObjectStart = std::string::npos; // 初期化
                    }
                }
            }
        }
        // ProductCode とオブジェクトのテキストの数は一致しているはず。
        if (m_vecstrProductCodes.size() != m_vecstrObjectTexts.size()) {
            LOG_INPROGRESSF("Mismatch between ProductCodes and ObjectTexts: %zu and %zu", m_vecstrProductCodes.size(), m_vecstrObjectTexts.size());
            return false;
        }
        // 読み込んだオブジェクトをログに出力する。
        for (int iIndex = 0; iIndex < static_cast<int>(m_vecstrProductCodes.size()); ++iIndex) {
            LOG_INPROGRESSF("m_vecstrProductCodes[%d] = %s",
                static_cast<int>(iIndex),
                m_vecstrProductCodes[iIndex].c_str());
            LOG_INPROGRESSF("m_vecstrObjectTexts[%d] = %s",
                static_cast<int>(iIndex),
                m_vecstrObjectTexts[iIndex].c_str());
        }
        return true;
    }

    /**
     * @brief  指定されたProductCodeに対応するオブジェクトのテキストを取得する。
     * @param  krstrProductCode ProductCode
     */
    bool FindCaptureConfig(const std::string& rstrTargetProductCode) {
        LOG_BEGINF0(7, "CaptureConfig::FindCaptureConfig()");
        nlohmann::json objCaptureParams;
        bool bIsMatchedProductCode = false;
        bool bIsEmptyProductCode = false;
        
        // ProductCode とオブジェクトのテキストが空の場合は、ログに出力して false を返す。
        if (m_vecstrProductCodes.empty() || m_vecstrObjectTexts.empty()) {
            LOG_INPROGRESSF("No ProductCodes or ObjectTexts found in JSON.");
            return false;
        }

        for (int iIndex = 0; iIndex < static_cast<int>(m_vecstrProductCodes.size()); ++iIndex) {
            const std::string& krstrProductCode = m_vecstrProductCodes[iIndex];
            const std::string& krstrObjectText = m_vecstrObjectTexts[iIndex];

            std::istringstream InputStream(krstrObjectText);
            nlohmann::json objectText;
            InputStream >> objectText;
            // JSONのパースに失敗した場合は、ログに出力して次のループに進む。
            if (InputStream.fail()) {
                LOG_INPROGRESSF("JSON parse error at index %d", iIndex);
                continue;
            }

            if (krstrProductCode == rstrTargetProductCode) {
                LOG_INPROGRESSF("Found ProductCode: %s at index %d", krstrProductCode.c_str(), iIndex);
                objCaptureParams = objectText;
                bIsMatchedProductCode = true;
                break;
            }

            // ProductCode が空の設定をフォールバックとして保持する。
            if (!bIsEmptyProductCode && krstrProductCode.empty()) {
                objCaptureParams = objectText;
                bIsEmptyProductCode = true;
            }
        }
        
        // ProductCode が一致しなかった場合、かつ空のProductCodeが見つからなかった場合は、ログに出力して false を返す。
        if (!bIsMatchedProductCode && !bIsEmptyProductCode) {
            LOG_INPROGRESSF("No matching ProductCode in JSON. ProductCode=%s", rstrTargetProductCode.c_str());
            return false;
        }

        m_iGainType          = objCaptureParams.value("GainType", 0);
        m_imsExpTime         = objCaptureParams.value("msExpTime", 0);
        m_iCaptureFrame      = objCaptureParams.value("CaptureFrame", 0);
        m_iBinningType       = objCaptureParams.value("BinningType", 0);
        m_iOriginalWidth     = objCaptureParams.value("OriginalWidth", 0);
        m_iOriginalHeight    = objCaptureParams.value("OriginalHeight", 0);
        m_iCaptureAreaLeft   = objCaptureParams.value("CaptureAreaLeft", 0);
        m_iCaptureAreaTop    = objCaptureParams.value("CaptureAreaTop", 0);
        m_iCaptureAreaWidth  = objCaptureParams.value("CaptureAreaWidth", 0);
        m_iCaptureAreaHeight = objCaptureParams.value("CaptureAreaHeight", 0);

        return true;
    }
};
