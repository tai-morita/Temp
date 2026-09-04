// CpturerByHBI_rev2.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <thread>
#include <fstream>
#include <chrono>
#include "pch.h"
#include "framework.h"
#include "hbidevicectrl.h"
#include "../Common/nlohmann/json.hpp"
#include "../CSmartLog/SmartLog.h"
#include "../BigTIFF/BigTIFF.h"
#include "../Registry/Registry.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 唯一のアプリケーション オブジェクトです

CWinApp theApp;

using namespace std;

// 08/24: 引数 ka4duiImage は参照渡しにするべき。コピーされるとメモリ使用量が大きくなるため。
//        -> const CArray4D<uint16_t> &kra4duiImage に変更しました。
//        サイズやパスはログ出力しない？
//        -> 52, 53 行目に実装しています。
/**
 * @brief   画像を保存する関数
 * @param   kra4duiImage        保存する画像データの 4 次元配列 (フレーム番号は T 軸で指定される。 Z 軸は 0 で固定される。)
 * @param   kiImageWidth       画像の幅
 * @param   kiImageHeight      画像の高さ
 * @param   krwstrSaveFilePath 保存先のファイルパス
 * @return  成功: true, 失敗: false
 */
bool SaveImage(const CArray4D<uint16_t> &kra4duiImage, const int kiImageWidth, const int kiImageHeight, const wstring& krwstrSaveFilePath) {
    LOG_BEGINF0(7, "HzZX| SaveImage()");
    // 高さ、幅が一致しない or 入力画像のバッファ長が 0 の場合は保存しない。
    if ((kiImageWidth != kra4duiImage.XLen() || kiImageHeight != kra4duiImage.YLen()) || kra4duiImage.BufferLen() == 0) {
        // 08/24 occured -> occurred. "Something problem" も不自然なので、データ形式が不正であることを示す内容が望ましい。
        //       -> ログの文章を修正しました。
		// LOG_INPROGRESSF("NrwT| Something problem occurred.");
        LOG_INPROGRESSF("NrwT| Error: Invalid image data.");
        LOG_INPROGRESSF("Mu7z|  kiImageWidth            = %d", kiImageWidth);
        LOG_INPROGRESSF("4ZzF|  kiImageHeight           = %d", kiImageHeight);
        LOG_INPROGRESSF("JsUK|  kra4duiImage.XLen()      = %d", kra4duiImage.XLen());
        LOG_INPROGRESSF("MYnw|  kra4duiImage.YLen()      = %d", kra4duiImage.YLen());
        LOG_INPROGRESSF("VNmm|  kra4duiImage.BufferLen() = %d", kra4duiImage.BufferLen());
        return false;
    }
    LOG_INPROGRESSF("Oqj2| Saving image data: TotalFrame = %d, Height = %d, Width = %d", kra4duiImage.TLen(), kiImageHeight, kiImageWidth);
    LOG_INPROGRESSF("QZrw| Saving image to  : %s", std::string(krwstrSaveFilePath.begin(), krwstrSaveFilePath.end()).c_str());
    CBigTIFF tiffOut;
    // BigTIFF 形式で保存する。
    tiffOut.OpenFileToWrite(krwstrSaveFilePath, CBigTIFF::EWriteFormat::TIFF8);
    for (int iFrame = kra4duiImage.TMin(); iFrame <= kra4duiImage.TMax(); ++iFrame) {
        CArray2D<uint16_t> a2duiTemp(0, kiImageWidth - 1, 0, kiImageHeight - 1);
        a2duiTemp    = kra4duiImage.Geta2dPlane(0, iFrame);
        double dMean = a2duiTemp.Mean(double());
        LOG_INPROGRESSF("kB9F| Frame %d: Mean pixel value = %f", iFrame, dMean);
        tiffOut.WriteFrame(a2duiTemp);
    }
    tiffOut.CloseFile();
    return true;
}

// memo: クラス名と関数名を同じにしない方がよいと思う。
//       準備・キャプチャ・切断とかに分けた方がいいと思う。
//       1つにまとめたいんだった他の名前を検討すべき。
/**
* @brief  HBI SDK を使用して、デバイスの接続、切断、画像取得などの操作を行う関数
* @details HBI SDK の初期化をして、デバイスへ接続する。
*          接続後、デバイスの Product Code に対応する撮影パラメータを Json ファイルから読み込む。
*          撮影パラメータをデバイスに設定し、画像を取得する。
*          取得した画像を BigTIFF 形式で保存する。
*          最後に、デバイスの切断と SDK の終了処理を行う。
* @return 成功: true, 失敗: false
* @note   CapturerByHBI 開発の練習として、 SDK を使用して画像取得できる関数を作成した。
*/
bool CapturerByHBIDlg() {
    LOG_BEGINF0(7, "3HGr| MAIN: CapturerByHBIDlg()");
    const wstring kwstrParamsJsonPath    = LR"(D:\_2026\CapturerByHBI\CapturerByHBI_rev2\DeviceParams.json)";              // パラメータを読む JSON ファイル
    const wstring kwstrSaveFilePath      = LR"(D:\_2026\CapturerByHBI\CapturerByHBI_rev2\CaptureData\CapturedImage.tif)";  // 保存する画像ファイルのパス
    const std::string kstrDestIpAddr     = "192.168.10.40"; // FPD の IP アドレス
    const std::string kstrSrcIpAddr      = "192.168.10.20"; // PC の IP アドレス
    constexpr unsigned short kusDestPort = 32897;           // FPD のポート番号
    constexpr unsigned short kusSrcPort  = 32896;           // PC のポート番号

    CHBIDeviceCtrl hbidevicectrl;

    // HBI の Initialize
    if (!hbidevicectrl.Initialize()) {
        LOG_INPROGRESSF("47WE| HBI Initialize failed.");
        return false;
    }

    // SDK のイベントコールバック関数を設定する。イベントが発生したとき、 HBI SDK が UserHBICallback を呼び出す。
    if (!hbidevicectrl.SetCallbackFunction()) {
        return false;
    }

    // memo: ここで return したら後ろのリトライは意味無い。
    // -> デバッグで使用していたものが残っていました。すみません。
    // 08/24: 削除したリトライはデバッグ目的で実装したんですか？ ConnectDevice() が1回で安定して接続できるのであればリトライ不要だと思います。
    //        -> 安定接続のため、接続を最大 3 回までリトライするようにしました。
    // Device の接続
    {
        for (int iRetryCount = 0; iRetryCount < 3; iRetryCount++) {
            if (!hbidevicectrl.ConnectDevice(&kstrDestIpAddr, kusDestPort, &kstrSrcIpAddr, kusSrcPort)) {
                if (iRetryCount >= 2) {
                    LOG_INPROGRESSF("eK6h| Error: Failed to connect to the device after %d attempts.", iRetryCount+1);
                    return false;
                }
            } else {
                break;
            }
        }
    }

    // memo: 下でバージョン等を取得しているが出力していない？
    // 取得できるか試す目的？製品コードとシリアル番号は残しておいた方が後で役立つと思う。
    // -> SDK バージョン、シリアル番号、製品コードをログに出力するようしました。
    // 08/24: Print~()後ろで再度 product code を取得しなおしているので、冗長な感じがします。
    //        ↓のコメントアウトを改造して、1. 先に3つ取得する。2. ログ出力する。 3.  if (~.empty()) をまとめて行ってもいいと思います。

    /*
    // HBI SDK のバージョンを取得する。
    const std::string kstrSDKVersion = hbidevicectrl.GetSDKVersion();
    if (kstrSDKVersion.empty()) {
        LOG_INPROGRESSF("BhLx| Failed to get SDK version.");
    }

    // デバイスのシリアルナンバーを取得する。
    const std::string kstrSerialNumber = hbidevicectrl.GetFPDSerialNumber();
    if (kstrSerialNumber.empty()) {
        LOG_INPROGRESSF("GZMP| Failed to get FPD Serial Number.");
        return false;
    }

    // デバイスの製品コードを取得する。
    const std::string kstrProductCode = hbidevicectrl.GetFPDProductCode();
    if (kstrProductCode.empty()) {
        LOG_INPROGRESSF("uBK0| Failed to get FPD Product Code.");
        return false;
    }
    */

    
    // 撮影パラメータをデバイスに設定する前に、安定するまで少し待つ。
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // HBI SDK のバージョンをログに出力する。
    if (!hbidevicectrl.PrintSDKVersion()) {
        LOG_INPROGRESSF("E31p| Failed to print HBI SDK version.");
        return false;
    }

    // デバイスの情報を取得してログに出力する。
    if (!hbidevicectrl.PrintDeviceInfo()) {
        LOG_INPROGRESSF("So8s| Failed to print device info.");
        return false;
    }

    // ProductCode をもとに、 JSON ファイルから撮影パラメータを読み込む。
    // JSON ファイルの内容は、 CaptureConfig クラスのコンストラクタで読み取り、撮影パラメータが設定される。
    const std::string kstrProductCode = hbidevicectrl.GetFpdProductCode();
    CCaptureConfig captureConfig(kwstrParamsJsonPath, kstrProductCode);

    // 撮影枚数が 0 の場合はエラーとして終了する。
    if (captureConfig.m_iCaptureFrame == 0) {
        LOG_INPROGRESSF("x1Vf| Failed to load CaptureConfig for Product Code: %s", kstrProductCode.c_str());
        return false;
    }

    if (kstrProductCode == ("X-Panel3030zFDM") && (captureConfig.m_iCaptureAreaHeight % 2 != 0)) {
        // 3030z の場合は、デュアル読出しのため、中央から等間隔にオフセットする。そのため高さ方向のサイズは偶数である必要がある
        // memo: ログに Failed とか Error とか書かなくてもいい？
        LOG_INPROGRESSF("6Ysy| Error: Zoom height must be a multiple of 2 for Product Code: %s.", kstrProductCode.c_str());
		LOG_INPROGRESSF("d6aH|  m_iCaptureAreaHeight: %d", captureConfig.m_iCaptureAreaHeight);
        return false;
    }

    // 撮影パラメータをデバイスに設定する前に、安定するまで少し待つ。
    // 08/24: 上でも待ってるけど2回待ってるのは理由がある？
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // デバイスに撮影パラメータを設定する。
    if (!hbidevicectrl.SetCaptureParams(captureConfig)) {
        LOG_INPROGRESSF("lKkL| Failed to set CaptureParams.");
        return false;
    }

    // 撮影パラメータをデバイスに設定した後、確認のためにログに出力する。
    if (!hbidevicectrl.PrintCaptureParams()) {
        LOG_INPROGRESSF("IaqJ| Failed to print CaptureParams.");
        return false;
    }

    // 画像プロパティを更新する。
    if (!hbidevicectrl.UpdateImageProperties()) {
        LOG_INPROGRESSF("mPM3| Failed to update image properties.");
        return false;
    }

    // 画像バッファを確保する。
    if (!hbidevicectrl.AllocateImageBuffer(captureConfig.m_iCaptureFrame)) {
        LOG_INPROGRESSF("q7lu| Failed to allocate image buffer.");
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        // キャプチャ処理
        // キャプチャ開始
        if (!hbidevicectrl.StartCapture()) {
            LOG_INPROGRESSF("IiDa| Failed to start capture.");
            return false;
        }

        // 50 ms ごとに終了フラグの状態を確認する。
        // 指定枚数の画像を取得したら、終了フラグが立つ。
        // memo: 本当は別スレッドでタイムアウトを設けたほうがいいと思いますが、異常終了の処理同様未実装です。
        while (!hbidevicectrl.IsCaptureFinished()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // memo: キャプチャ中じゃなかったらすでに StopCapture() されているのでは？単純な疑問
        // -> キャプチャ中かどうかのフラグを確認して、キャプチャ中であれば StopCapture() を呼び出すようにしました。
        // キャプチャ中であれば停止する。
        if (hbidevicectrl.IsCapturing()) {
            if (!hbidevicectrl.StopCapture()) {
                LOG_INPROGRESSF("V5um| Failed to stop capture");
                return false;
            }
        }

        // memo: 異常終了時の処理は未実装。Capturer開発の時にDCAMを参考に実装します。
        //       → OKです。
		// キャプチャ枚数が指定枚数でない場合はエラーを出力する。
        if (hbidevicectrl.GetCapturedFrameCount() != captureConfig.m_iCaptureFrame) {
            // memo: このログも Error とか Failed とか書いた方がよさそう。Error: ~ みたいな風に書いてます。正解かわからないので3dxdの他のコード参考にしてみてください。
            // -> 基本 Error で書いていたので、ログに Error と書くようにしました。
            LOG_INPROGRESSF("K9ig| Error: Captured frame count (%d) does not match expected frame count (%d)", hbidevicectrl.GetCapturedFrameCount(), captureConfig.m_iCaptureFrame);
        }
    }
    // memo: 異常終了時の処理は未実装とのことだったので余計かもしれませんが、途中で撮影終わってもSaveしておいてよさそう。
    // 画像を保存する。
    const int kiImageHeight = hbidevicectrl.GetImageHeight();
    const int kiImageWidth  = hbidevicectrl.GetImageWidth();
    const CArray4D<uint16_t> kra4duiImage = hbidevicectrl.GetImageBuffer();

    if (!SaveImage(kra4duiImage, kiImageWidth, kiImageHeight, kwstrSaveFilePath)) {
		LOG_INPROGRESSF("9Is0| Failed to save image");
		return false;
    }

    // デバイスの接続を切断する。
	if (!hbidevicectrl.Shutdown()) {
		LOG_INPROGRESSF("Failed to Shutdown the device.");
		return false;
	}

	return true;

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
        } else {
            // TODO: アプリケーションの動作を記述するコードをここに挿入してください。
            CSmartLog::GetiLogFileAccessKeyW(L"CapturerByHBILog.log");
            LOG_BEGINF0(7, "iX1b| MAIN");
            bool bResult = CapturerByHBIDlg();
            if (bResult) {
                LOG_INPROGRESSF("2dok| CapturerByHBIDlg() completed successfully.");
				nRetCode = 0;
            } else {
                LOG_INPROGRESSF("Nwtm| CapturerByHBIDlg() failed.");
                nRetCode = 1;
            }
        }
    } else {
        // TODO: 必要に応じてエラー コードを変更してください
        wprintf(L"致命的なエラー: GetModuleHandle が失敗しました\n");
        nRetCode = 1;
    }

    return nRetCode;
}
