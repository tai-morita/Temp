// CpturerByHBI_rev2.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <thread>
#include <fstream>
#include <chrono>
#include "pch.h"
#include "framework.h"
#include "HBIDeviceCtrl.h"
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

/**
 * @brief  画像を保存する関数
 * @detail 4 次元配列は(X, Y, Z, T) で格納されており、フレームは T 軸で指定されている。
 * @param  a2dusImage 保存する画像データの2次元配列
 * @param  iSaveFrame 保存するフレーム数
 * @param  krwstrSaveFilePath 保存先のファイルパス
 */
void SaveImage(const CArray4D<uint16_t> a4duiImage, const int kiImageWidth, const int kiImageHeight, const wstring& krwstrSaveFilePath) {
    LOG_BEGINF0(7, "HzZX| SaveImage()");
    // データに不具合があった場合は保存せずに終了する。
    // 高さ、幅が一致しない or 入力画像のバッファ長が 0 の場合
    if ((kiImageWidth != a4duiImage.XLen() || kiImageHeight != a4duiImage.YLen()) || a4duiImage.BufferLen() == 0) {
        LOG_INPROGRESSF("NrwT| Something problem occured.");
        LOG_INPROGRESSF("Mu7z|  kiImageWidth           = %d", kiImageWidth);
        LOG_INPROGRESSF("4ZzF|  kiImageHeight          = %d", kiImageHeight);
        LOG_INPROGRESSF("JsUK|  a4duiImage.XLen()      = %d", a4duiImage.XLen());
        LOG_INPROGRESSF("MYnw|  a4duiImage.YLen()      = %d", a4duiImage.YLen());
        LOG_INPROGRESSF("VNmm|  a4duiImage.BufferLen() = %d", a4duiImage.BufferLen());
        return;
    }
    LOG_INPROGRESSF("Oqj2| Saving image data: TotalFrame = %d, Height = %d, Width = %d", a4duiImage.TLen(), kiImageHeight, kiImageWidth);
    LOG_INPROGRESSF("QZrw| Saving image to  : %s", std::string(krwstrSaveFilePath.begin(), krwstrSaveFilePath.end()).c_str());
    CBigTIFF tiffOut;
    // BigTIFF 形式で保存する。
    tiffOut.OpenFileToWrite(krwstrSaveFilePath, CBigTIFF::EWriteFormat::TIFF8);
    for (int iFrame = a4duiImage.TMin(); iFrame <= a4duiImage.TMax(); ++iFrame) {
        CArray2D<uint16_t> a2duiTemp(0, kiImageWidth - 1, 0, kiImageHeight - 1);
        a2duiTemp    = a4duiImage.Geta2dPlane(0, iFrame);
        double dMean = a2duiTemp.Mean(double());
        LOG_INPROGRESSF("kB9F| Frame %d: Mean pixel value = %f", iFrame, dMean);
        tiffOut.WriteFrame(a2duiTemp);
    }
    tiffOut.CloseFile();
}

/**
* @brief  HBI SDK を使用して、デバイスの接続、切断、画像取得などの操作を行う関数
 *@detail CapturerByHBI 開発の練習として、 SDK を使用して画像取得できる関数を作成した。
*/
void CapturerByHBIDlg() {
    LOG_BEGINF0(7, "3HGr| MAIN: CapturerByHBIDlg()");
    wstring wstrPARAMSJSONPATH    = LR"(D:\_2026\CapturerByHBI\CapturerByHBI_rev2\DeviceParams.json)";              // パラメータを読む JSON ファイル
    wstring wstrSaveFilePath  = LR"(D:\_2026\CapturerByHBI\CapturerByHBI_rev2\CaptureData\CapturedImage.tif)";  // 保存する画像ファイルのパス
    const std::string kstrDestIpAddr     = "192.168.10.40"; // FPD の IP アドレス
    const std::string kstrSrcIpAddr      = "192.168.10.20"; // PC の IP アドレス
    constexpr unsigned short kusDestPort = 32897;           // FPD のポート番号
    constexpr unsigned short kusSrcPort  = 32896;           // PC のポート番号

    CHBIDeviceCtrl hbiDeviceCtrl;

    // HBI の Initialize
    if (!hbiDeviceCtrl.Initialize()) {
        LOG_INPROGRESSF("47WE| HBI Initialize failed.");
        return;
    }

    // SDK のイベントコールバック関数を設定する。イベントが発生したとき、 SDK が UserHBICallback を呼び出す。
    if (!hbiDeviceCtrl.SetCallbackFunction()) {
        return;
    }

    // Device の接続
    if (!hbiDeviceCtrl.ConnectDevice(&kstrDestIpAddr, kusDestPort, &kstrSrcIpAddr, kusSrcPort)) {
        return;
    }

    // 接続後、安定するまで少し待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 接続状態の確認。
    if (!hbiDeviceCtrl.IsDeviceConnected()) {
        return;
    }

    // HBI SDK のバージョンを取得する。
    std::string strSDKVersion = hbiDeviceCtrl.GetSDKVersion();
    if (strSDKVersion.empty()) {
        LOG_INPROGRESSF("BhLx| Failed to get SDK version.");
    }

    // デバイスのシリアルナンバーを取得する。
    std::string strSerialNumber = hbiDeviceCtrl.GetFPDSerialNumber();
    if (strSerialNumber.empty()) {
        LOG_INPROGRESSF("GZMP| Failed to get FPD Serial Number.");
        return;
    }
    // デバイスの製品コードを取得する。
    std::string strProductCode = hbiDeviceCtrl.GetFPDProductCode();
    if (strProductCode.empty()) {
        LOG_INPROGRESSF("uBK0| Failed to get FPD Product Code.");
        return;
    }

    // ProductCode をもとに、 JSON ファイルから撮影パラメータを読み込む。
    // JSON ファイルの内容は、 CaptureConfig クラスのコンストラクタで読み取り、撮影パラメータが設定される。
    CCaptureConfig captureConfig(wstrPARAMSJSONPATH, strProductCode);

    // 撮影枚数が 0 の場合はエラーとして終了する。
    if (captureConfig.m_iCaptureFrame == 0) {
        LOG_INPROGRESSF("x1Vf| Failed to load CaptureConfig for Product Code: %s", strProductCode.c_str());
        return;
    }

    if (strProductCode == ("X-Panel3030zFDM") && (captureConfig.m_iCaptureAreaHeight % 2 != 0)) {
        // 3030z の場合は、デュアル読出しのため、中央から等間隔にオフセットする。そのため高さ方向のサイズは偶数である必要がある
        LOG_INPROGRESSF("6Ysy| Zoom height must be a multiple of 2 for Product Code: %s. Adjusting to nearest even number.", strProductCode.c_str());
        return;
    }

    // 撮影パラメータをデバイスに設定する前に、安定するまで少し待つ。
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // デバイスに撮影パラメータを設定する。
    if (!hbiDeviceCtrl.SetCaptureParams(captureConfig)) {
        LOG_INPROGRESSF("lKkL| Failed to set CaptureConfig for Product Code: %s", strProductCode.c_str());
        return;
    }

    // 撮影パラメータをデバイスに設定した後、確認のためにログに出力する。
    if (!hbiDeviceCtrl.PrintCaptureParams()) {
        LOG_INPROGRESSF("IaqJ| Failed to print CaptureConfig for Product Code: %s", strProductCode.c_str());
        return;
    }

    // 画像プロパティを更新する。
    if (!hbiDeviceCtrl.UpdateImageProperties()) {
        LOG_INPROGRESSF("mPM3| Failed to update image properties for Product Code: %s", strProductCode.c_str());
        return;
    }

    // 画像バッファを確保する。
    if (!hbiDeviceCtrl.AllocateImageBuffer(captureConfig.m_iCaptureFrame)) {
        LOG_INPROGRESSF("Failed to allocate image buffer");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // キャプチャ開始
    if (hbiDeviceCtrl.StartCapture()) {
        // 50 ms ごとにキャプチャ中かどうかを確認する。
        while (hbiDeviceCtrl.IsCapturing()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        // キャプチャが終了したら、StopCapture() を呼び出してキャプチャを停止する。
        hbiDeviceCtrl.StopCapture();
    }

    // デバイスの接続を切断する。
    hbiDeviceCtrl.DisconnectDevice();

    // 画像を保存する。
    const int kiImageHeight = hbiDeviceCtrl.GetImageHeight();
    const int kiImageWidth  = hbiDeviceCtrl.GetImageWidth();
    const CArray4D<uint16_t> ka4duiImage = hbiDeviceCtrl.GetImageBuffer();

    SaveImage(ka4duiImage, kiImageWidth, kiImageHeight, wstrSaveFilePath);

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
        } else
        {
            // TODO: アプリケーションの動作を記述するコードをここに挿入してください。
            CSmartLog::GetiLogFileAccessKeyW(L"CapturerByHBILog.log");
            LOG_BEGINF0(7, "iX1b| MAIN");
            CapturerByHBIDlg();
        }
    } else
    {
        // TODO: 必要に応じてエラー コードを変更してください
        wprintf(L"致命的なエラー: GetModuleHandle が失敗しました\n");
        nRetCode = 1;
    }

    return nRetCode;
}
