#include <thread>
#include <fstream>
#include <chrono>
#include "pch.h"
#include "framework.h"
#include "../Common/nlohmann/json.hpp"
#include "../CSmartLog/SmartLog.h"

struct CCaptureConfig {
    int m_iGainType;          //!< ゲインモード:  1 から 11 までの整数値で表される
    int m_imsExposureTime;    //!< 露光時間(ミリ秒)
    int m_iCaptureFrame;      //!< 取得するフレーム数。
    int m_iBinningType;       //!< ビニングモード: 1 = 1x1, 2 = 2x2, 3 = 3x3, 4 = 4x4
    int m_iOriginalWidth;     //!< 元画像の幅
    int m_iOriginalHeight;    //!< 元画像の高さ
    int m_iCaptureAreaLeft;   //!< 取得領域の左端座標
    int m_iCaptureAreaTop;    //!< 取得領域の上端座標
    int m_iCaptureAreaWidth;  //!< 取得領域の幅
    int m_iCaptureAreaHeight; //!< 取得領域の高さ

    /**
     * @brief JSON ファイル内の ProductCode を格納するベクトル。トップレベルの配列に格納されている。
     * @memo  このベクトルは JSON 内に記述されているすべての ProductCode を格納するので、 s をつけています。
    */
    std::vector<std::string> m_vecstrProductCodes;
    /** @brief オブジェクトのテキストを格納するベクトル。トップレベルの配列に格納されている。 */
    std::vector<std::string> m_vecstrObjectTexts;

    /**
     *  @brief 外部から渡された設定ファイルと ProductCode で初期化する
	 *  @param[in] krwstrParamsJsonPath JSON ファイルのパス
	 *  @param[in] krstrProductCode デバイスの製品コード
	 *  @details JSON ファイルを読み込み、指定された ProductCode に対応する設定を見つけて、メンバ変数に格納する。
    */
    CCaptureConfig(const std::wstring& krwstrParamsJsonPath, const std::string& krstrProductCode)
    {
        LOG_BEGINF0(7, "5s8E| CaptureConfig::CaptureConfig()");
        Clear();
        // JSON ファイルを読み込み、文字列形式に変換する。
        std::string strJsonText            = ReadJsonFile(krwstrParamsJsonPath);
        // JSON ファイルの内容を解析し、 ProductCode ごとに設定をベクトルに格納する。
        const bool bIsExtractCaptureConfig = ExtractCaptureConfigFromJsonText(strJsonText);
        // 指定された ProductCode に対応する設定をメンバ変数に格納する。
        bool bIsFindCaptureConfig          = ApplyCaptureConfig(krstrProductCode);
        if (!strJsonText.empty() && bIsExtractCaptureConfig && bIsFindCaptureConfig) {
            // 成功した場合はログに出力する。
            // memo: 「~の設定を読み込みました。（→パラメータを出力）」なら理解できるが、
            // 「（全てのパラメータ）のコンフィグファイルを読みました」は書き方が適切なのか。
            // 「読み込んだパラメータは～」では？
            LOG_INPROGRESSF("l7qt| Applied CaptureConfig for Product Code: %s", krstrProductCode.c_str());
            LOG_INPROGRESSF("steJ|    Product Code       : %s"   , krstrProductCode.c_str());
            LOG_INPROGRESSF("bI88|    Gain Type          : %d"   , m_iGainType);
            LOG_INPROGRESSF("PPPL|    Exposure Time      : %d ms", m_imsExposureTime);
            LOG_INPROGRESSF("HiSV|    Capture Frame      : %d"   , m_iCaptureFrame);
            LOG_INPROGRESSF("3sGX|    Binning Type       : %d"   , m_iBinningType);
            LOG_INPROGRESSF("KuNk|    Original Width     : %d"   , m_iOriginalWidth);
            LOG_INPROGRESSF("DpL2|    Original Height    : %d"   , m_iOriginalHeight);
            LOG_INPROGRESSF("6TdK|    Capture Area Left  : %d"   , m_iCaptureAreaLeft);
            LOG_INPROGRESSF("2NYy|    Capture Area Top   : %d"   , m_iCaptureAreaTop);
            LOG_INPROGRESSF("jrHQ|    Capture Area Width : %d"   , m_iCaptureAreaWidth);
            LOG_INPROGRESSF("Smr3|    Capture Area Height: %d"   , m_iCaptureAreaHeight);
        } else {
            LOG_INPROGRESSF("l7qt| Failed to load CaptureConfig for Product Code: %s", krstrProductCode.c_str());
        }
    }

    /**
     * @brief 保持しているパラメータを初期化する
     */
    void Clear() {
        m_iGainType          = 0;
        m_imsExposureTime    = 0;
        m_iCaptureFrame      = 0;
        m_iBinningType       = 0;
        m_iOriginalWidth     = 0;
        m_iOriginalHeight    = 0;
        m_iCaptureAreaLeft   = 0;
        m_iCaptureAreaTop    = 0;
        m_iCaptureAreaWidth  = 0;
        m_iCaptureAreaHeight = 0;
		m_vecstrProductCodes.clear();
		m_vecstrObjectTexts.clear();
    }

    /**
     * @brief     JSON ファイルを読み込み、 string 形式に変換する。
     * @param[in] krwstrParamsJsonPath JSONファイルのパス。
     * @return    読み取った JSON ファイルの内容を文字列として返す。読み込みに失敗した場合は空文字列を返す。
     */
    std::string ReadJsonFile(const std::wstring& krwstrParamsJsonPath) {
        LOG_BEGINF0(7, "8lp9| CaptureConfig::ReadJsonFile()");
        std::string strParamsJsonText;
        try {
            std::ifstream ifs(krwstrParamsJsonPath);
            if (!ifs.is_open()) {
                LOG_INPROGRESSF("xH6U| Failed to open: %s", std::string(krwstrParamsJsonPath.begin(), krwstrParamsJsonPath.end()).c_str());
                return "";
            }
            // JSONファイルの内容を文字列として読み込む
            strParamsJsonText.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        } catch (const std::exception& eError) {
            LOG_INPROGRESSF("SbeY| Exception occurred while opening JSON file: %s", eError.what());
            return "";
        }
        return strParamsJsonText;
    }

    /**
     * @brief   JSON ファイル形式の文字列を解析し、トップレベルの配列に存在する ProductCode とオブジェクトのテキストをそれぞれのベクトルに格納する。
     * @return  true : ProductCode とオブジェクト文字列の数が一致した。
     *          false: ProductCode とオブジェクト文字列の数が一致しなかった。
     * @details JSONファイルを文字列形式で解析し、トップレベルの配列に存在する ProductCode とオブジェクトのテキストをそれぞれのベクトルに格納する。
     *          1 文字ずつ解析して、エスケープ文字で開始と終了を判断する。
     */
    bool ExtractCaptureConfigFromJsonText(const std::string& strParamsJsonText) {
        LOG_BEGINF0(7, "cLiC| CaptureConfig::ExtractCaptureConfigFromJsonText()");
        std::size_t szObjectStart = std::string::npos;
        int iArrayDepth                      = 0;     // トップレベルの配列の深さを追跡するための変数。
        int iObjectDepth                     = 0;     // オブジェクトの深さを追跡するための変数。
        bool bIsInString                     = false; // 文字列の中にいるかどうかを追跡するための変数。 // memo: bIsInStringでは
        bool bIsEscaped                      = false; // 文字列内でエスケープされているかどうかを追跡するための変数。
        bool bIsPushBackString               = false; // 文字列をベクトルに追加するかどうかを追跡するための変数。
        std::string strCurrentTopLevelString = "";    // トップレベルの配列内の文字列(ProductCode)をキャプチャするための変数。
    
        for (int iIndex = 0; iIndex < strParamsJsonText.size(); ++iIndex) {
            // １文字ずつ抽出して、文字列を抜き取る。
            const char kcCurrentJsonChar = strParamsJsonText[iIndex];

            // 現在は文字列なので、文字列の終わりにある["]を探す。
            if (bIsInString) {
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
                    bIsInString = false;
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
                bIsInString  = true;
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
                        // strParamsJsonText の { と } の間のテキストを m_vecstrObjectTexts に入れる。
                        m_vecstrObjectTexts.emplace_back(strParamsJsonText.substr(szObjectStart, iIndex - szObjectStart + 1));
                        szObjectStart = std::string::npos; // 初期化
                    }
                }
            }
        }
        // ProductCode とオブジェクトのテキストの数が一致してない場合は、ログに出力して false を返す。
        if (m_vecstrProductCodes.size() != m_vecstrObjectTexts.size()) {
            LOG_INPROGRESSF("pC5R| Mismatch between ProductCodes and ObjectTexts: %zu and %zu", m_vecstrProductCodes.size(), m_vecstrObjectTexts.size());
            return false;
        }
        // 読み込んだオブジェクトをログに出力する。
        for (int iIndex = 0; iIndex < static_cast<int>(m_vecstrProductCodes.size()); ++iIndex) {
            LOG_INPROGRESSF("WjD6| m_vecstrProductCodes[%d] = %s", static_cast<int>(iIndex), m_vecstrProductCodes[iIndex].c_str());
            LOG_INPROGRESSF("jsHs| m_vecstrObjectTexts [%d] = %s", static_cast<int>(iIndex), m_vecstrObjectTexts[iIndex].c_str());
        }
        return true;
    }

    // memo: 「krstrProductCode と krstrProductCode が一致するオブジェクト」の意味が分からない。
    // 返り値は成功・失敗ではない？
    /**
     * @brief     指定された ProductCode に対応するオブジェクトのテキストを取得し、メンバ変数に格納する。 // memo: スペースがない
     * @param[in] krstrTargetProductCode ProductCode
	 * @details   m_vecstrProductCodes と m_vecstrObjectTexts を走査し、 krstrTargetProductCode と一致する ProductCode を探す
     *            1. 一致する ProductCode が見つかった場合は、そのインデックスのオブジェクトのテキストを取得し、メンバ変数に格納する
     *            2. 一致する ProductCode が見つからなかった場合は、空の ProductCode のオブジェクトのテキストを取得し、メンバ変数に格納する
	 * @return    true: メンバ変数への格納に成功, false: 失敗
     */
    bool ApplyCaptureConfig(const std::string& krstrTargetProductCode) {
        LOG_BEGINF0(7, "Dgw4| CaptureConfig::ApplyCaptureConfig()");

        // memo: vec~s のsは冗長では？どこかで指摘していたと思うので、他もチェックしてください。
        // ProductCode とオブジェクトのテキストが空の場合は、ログに出力して false を返す。
        if (m_vecstrProductCodes.empty() || m_vecstrObjectTexts.empty()) {
            LOG_INPROGRESSF("kZQH| No ProductCodes or ObjectTexts found in JSON.");
            return false;
        }

        // memo: ↓の変数は最初のチェック後に宣言した方がよいのでは？
        nlohmann::json objCaptureParams; // memo: objCaptureParamsは"obj"なので統一した方が理解しやすい
        bool bIsProductCodeMatched = false; // memo: bIsProductCodeMatched?(自信ないです)
        bool bIsProductCodeEmpty   = false; // memo: bIsProductCodeEmpty?


        for (int iIndex = 0; iIndex < static_cast<int>(m_vecstrProductCodes.size()); ++iIndex) {
            const std::string& krstrProductCode = m_vecstrProductCodes[iIndex];
            const std::string& krstrObjectText  = m_vecstrObjectTexts[iIndex];

            std::istringstream InputStream(krstrObjectText);
            nlohmann::json objectText;
            InputStream >> objectText;

            // JSONの読み取りに失敗した場合は、ログに出力して次のループに進む。
            if (InputStream.fail()) {
                LOG_INPROGRESSF("mI9d| JSON parse error at index %d", iIndex);
                continue;
            }

            // ProductCode が一致した場合は、ログに出力して objCaptureParams に格納し、ループを抜ける。
            if (krstrProductCode == krstrTargetProductCode) {
                LOG_INPROGRESSF("SuUN| Found ProductCode: %s at index %d", krstrProductCode.c_str(), iIndex);
                objCaptureParams = objectText;
                bIsProductCodeMatched = true;
                break;
            }

            // memo: 関数のコメントでこれが分からないので、関数自体のコメントにも書いた方がいいと思います。
            // ProductCode が空の設定をフォールバックとして保持する。
            if (!bIsProductCodeEmpty && krstrProductCode.empty()) {
                objCaptureParams = objectText;
                bIsProductCodeEmpty = true;
            }
        }

		// memo: config file に krstrTargetProductCode が存在しない場合～
        // !(bIsProductCodeMatched || bIsProductCodeEmpty)
        // krstrTargetProductCode に一致する ProductCode が見つからなかった場合、かつ空の設定が見つからなかった場合は、ログに出力して false を返す。
        if (!(bIsProductCodeMatched || bIsProductCodeEmpty)) {
            LOG_INPROGRESSF("dBqE| No matching ProductCode in JSON. ProductCode=%s", krstrTargetProductCode.c_str());
            return false;
        }

        // objCaptureParams に格納された設定をメンバ変数に格納する。
        m_iGainType          = objCaptureParams.value("GainType"         , 0);
        m_imsExposureTime    = objCaptureParams.value("msExposureTime"   , 0);
        m_iCaptureFrame      = objCaptureParams.value("CaptureFrame"     , 0);
        m_iBinningType       = objCaptureParams.value("BinningType"      , 0);
        m_iOriginalWidth     = objCaptureParams.value("OriginalWidth"    , 0);
        m_iOriginalHeight    = objCaptureParams.value("OriginalHeight"   , 0);
        m_iCaptureAreaLeft   = objCaptureParams.value("CaptureAreaLeft"  , 0);
        m_iCaptureAreaTop    = objCaptureParams.value("CaptureAreaTop"   , 0);
        m_iCaptureAreaWidth  = objCaptureParams.value("CaptureAreaWidth" , 0);
        m_iCaptureAreaHeight = objCaptureParams.value("CaptureAreaHeight", 0);
        
        return true;
    }
};
