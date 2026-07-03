#include <thread>
#include <fstream>
#include <chrono>
#include "pch.h"
#include "framework.h"
#include "../Common/nlohmann/json.hpp"
#include "../CSmartLog/SmartLog.h"

struct CaptureConfig {
    int m_iGainType;             // ゲインモード
    int m_imsExposureTime; // 露光時間(マイクロ秒)
    int m_iCaptureFrame;         // 取得するフレーム数
    int m_iBinningType;          // ビニングモード
    int m_iOriginalWidth;        // 元画像の幅
    int m_iOriginalHeight;       // 元画像の高さ
    int m_iCaptureAreaLeft;      // 取得する画像の左端座標
    int m_iCaptureAreaTop;       // 取得する画像の上端座標
    int m_iCaptureAreaWidth;     // 取得する画像の幅
    int m_iCaptureAreaHeight;    // 取得する画像の高さ

    // 外部から渡された設定ファイルとProductCodeで初期化する
    CaptureConfig(const std::wstring& wstrParamsJsonPath, const std::string& strProductCode)
        : m_iGainType(0)
        , m_imsExposureTime(0)
        , m_iCaptureFrame(0)
        , m_iBinningType(0)
        , m_iOriginalWidth(0)
        , m_iOriginalHeight(0)
        , m_iCaptureAreaLeft(0)
        , m_iCaptureAreaTop(0)
        , m_iCaptureAreaWidth(0)
        , m_iCaptureAreaHeight(0)
    {
        LoadCaptureConfig(wstrParamsJsonPath, strProductCode);
    }

    /**
     * @brief  指定されたJSONファイルから、指定されたProductCodeに対応する撮影パラメータを読み込み、CaptureConfig構造体に格納する。
     * @param  wstrParamsJsonPath JSONファイルのパス。
     * @param  strProductCode     読み込む撮影パラメータに対応するProductCode。
     */
    void LoadCaptureConfig(const std::wstring& wstrParamsJsonPath, const std::string& strProductCode) {
        LOG_BEGINF0(7, "EePC| CaptureConfig::LoadCaptureConfig()");

        // SystemConstantsFile等の代用
        std::ifstream ifs(wstrParamsJsonPath);

        // ファイルが開けなかった場合はエラーを返す
        if (!ifs.is_open()) {
            LOG_INPROGRESSF("Ikny| Failed to open: %s", std::string(wstrParamsJsonPath.begin(), wstrParamsJsonPath.end()).c_str());
            return;
        }

        try {
            // JSONファイルの内容を文字列として読み込む
            std::string strParamsJsonText((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

            std::vector<std::string> vecstrProductCodes; // ProductCodeを格納するベクトル。トップレベルの配列に存在する。
            std::vector<std::string> vecstrObjectTexts;  // オブジェクトのテキストを格納するベクトル。トップレベルの配列に存在する。
            std::string strCurrentTopLevelString; // トップレベルの配列内の文字列(ProductCode)をキャプチャするための文字列。
            std::size_t iObjectStart = std::string::npos;
            int iArrayDepth = 0; // トップレベルの配列の深さを追跡するための変数。
            int iObjectDepth = 0; // オブジェクトの深さを追跡するための変数。
            bool bIsString = false;
            bool bIsEscaped = false;
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
                LOG_INPROGRESSF("NVOp| Config format error. Expected [\"ProductCode\", { ... }] pairs.");
                return;
            }

            nlohmann::json objCaptureParams; // ProductCodeにマッチしたオブジェクトを格納するためのJSONオブジェクト。
            bool bIsMachProductCode = false; // ProductCodeがマッチしたかどうかのフラグ。
            bool bIsEmptyProductCode = false; // ProductCodeが空のものがあったかどうかのフラグ。ProductCodeが見つからない場合はこれを使用する。

            // オブジェクトごとにProductCodeを比較し、マッチしたオブジェクトのJSONをobjCaptureParamsに格納する。
            for (std::size_t iX = 0; iX < vecstrObjectTexts.size(); ++iX) {
                const std::string& kstrProductCode = vecstrProductCodes[iX];
                const std::string& kstrObjectText = vecstrObjectTexts[iX];

                std::istringstream InputStream(kstrObjectText);
                nlohmann::json CurrentObjectText;
                InputStream >> CurrentObjectText;
                // JSONのパースに失敗した場合は次のオブジェクトに進む。
                if (InputStream.fail()) {
                    continue;
                }

                // ProductCodeがマッチした場合はobjCaptureParamsに格納し、ループを抜ける。
                if (kstrProductCode == strProductCode) {
                    objCaptureParams = CurrentObjectText;
                    bIsMachProductCode = true;
                    break;
                }

                // ProductCodeが空のものがあった場合は、objCaptureParamsに格納する。ただし、ProductCodeがマッチした場合は上書きされない。
                if (!bIsEmptyProductCode && kstrProductCode.empty()) {
                    objCaptureParams = CurrentObjectText;
                    bIsEmptyProductCode = true;
                }
            }

            // 何もマッチしなかった場合はエラーを返す。
            if (!bIsMachProductCode && !bIsEmptyProductCode) {
                LOG_INPROGRESSF("hD6X| No matching ProductCode in JSON. ProductCode=%s", strProductCode.c_str());
                return;
            }

            m_iGainType = objCaptureParams.value("GainType", 0);
            m_imsExposureTime = objCaptureParams.value("millisecExposureTime", 0);
            m_iCaptureFrame = objCaptureParams.value("CaptureFrame", 0);
            m_iBinningType = objCaptureParams.value("BinningType", 0);
            m_iOriginalWidth = objCaptureParams.value("OriginalWidth", 0);
            m_iOriginalHeight = objCaptureParams.value("OriginalHeight", 0);
            m_iCaptureAreaLeft = objCaptureParams.value("CaptureAreaLeft", 0);
            m_iCaptureAreaTop = objCaptureParams.value("CaptureAreaTop", 0);
            m_iCaptureAreaWidth = objCaptureParams.value("CaptureAreaWidth", 0);
            m_iCaptureAreaHeight = objCaptureParams.value("CaptureAreaHeight", 0);
        }

        catch (const std::exception& error) {
            LOG_INPROGRESSF("qz6n| JSON parse error: %s", error.what());
            return;
        }
        return;
    }

};