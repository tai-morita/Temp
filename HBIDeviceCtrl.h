#pragma once

#include "resource.h"

#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <iomanip>

#include "HBICaptureConfig.h"
#include "HBIERR.h"
#include "../Common/HBI_DLL/INCLUDES/HbiError.h"
#include "../Common/HBI_DLL/INCLUDES/HbiFpd.h"
#include "../Common/HBI_DLL/INCLUDES/HbiType.h"
#include "../Common/tstring.h"
#include "../Array/Array2D.h"
#include "../Array/Array4D.h"
#include "../CSmartLog/SmartLog.h"

// HBI で通信するデバイスの動作を制御するクラス。
/**
 * @brief   HBI SDK を使用して、デバイスの接続、切断、画像取得などの操作を行うクラス。
 * @details CapturerByHBI 開発の練習として、 SDK を使用して画像取得できるクラスを作成した。
 */
class CHBIDeviceCtrl
{
private:

	void*                    m_hHBI;              // HBISDK のハンドル。これで SDK の関数を呼び出す。
	bool                     m_bIsInitialized;    // 初期化されているか示すフラグ。
	uint16_t*                m_pImageBuffer;      // 画像バッファの先頭アドレスを指すポインタ。
	size_t                   m_szImageBufferSize; // 画像バッファのサイズ。
	bool                     m_bIsCapturing;      // 画像取得中かどうかを示すフラグ。
	int                      m_iFrameCounter;     // 取得したフレーム数をカウントする。
	int                      m_iCaptureFrame;     // 取得するフレームの総数。
	int                      m_iImageWidth;       // 取得する画像サイズ(幅)。
	int                      m_iImageHeight;      // 取得する画像サイズ(高さ)。
	CArray2D<unsigned short> m_a2dusImage;        // 1 フレーム分の画像データを保存する 2 次元配列。
	CArray4D<uint16_t>       m_a4duiImageBuffer;  // 取得した画像データを保存する 4 次元配列。

	std::string m_strProductCode;  // デバイスの製品コード。

public:
	// コンストラクタ。
	CHBIDeviceCtrl()
		: m_hHBI             (nullptr)
		, m_pImageBuffer     (nullptr)
		, m_bIsInitialized   (false)
		, m_bIsCapturing     (false)
		, m_iFrameCounter    (0)
		, m_iCaptureFrame    (0)
		, m_iImageWidth      (0)
		, m_iImageHeight     (0)
		, m_szImageBufferSize(0)
		, m_strProductCode   ("")
	{
		LOG_BEGINF0(7, "GOzh| HBIDeviceCtrl::HBIDeviceCtrl()");
	}

	// デストラクタ。
	~CHBIDeviceCtrl()
	{
		LOG_BEGINF0(7, "EFg7| HBIDeviceCtrl::~HBIDeviceCtrl()");
		if (IsCapturing()) {
			LOG_INPROGRESSF("2DfC| Stopping capture before destruction.");
			StopCapture();
		}
		// SDK の初期化が完了している場合は、 SDK を終了する。
		if (IsInitialized()) {
			HBI_Destroy(m_hHBI);
			m_bIsInitialized = false;
		}
		// 画像バッファが確保されている場合は、解放する。
		if (m_pImageBuffer != nullptr) {
			m_pImageBuffer = nullptr;
		}
	}


public:
	// CapturerHBIDlg.cpp からアクセスするためのゲッターメソッド。
	const CArray4D<uint16_t>& GetImageBuffer() const { return m_a4duiImageBuffer; }  // 取得した画像データを返す。
	const int                 GetImageWidth () const { return m_iImageWidth     ; }  // 画像の幅を返す。
	const int                 GetImageHeight() const { return m_iImageHeight    ; }  // 画像の高さを返す。
	/**
	 * @brief   デバイスのシリアル番号を取得する。
	 * @details シリアル番号は 14 bytes で取得される。
	 * @return  取得に成功した場合はシリアル番号の文字列、失敗した場合は空文字列。
	 */
	std::string GetFPDSerialNumber() const {
		LOG_BEGINF0(7, "uHRU| HBIDeviceCtrl::GetFPDSerialNumber()");
		// パネルの情報を取得する。
		if (!m_bIsInitialized) { return ""; }
		// シリアルは 13 文字 + NULL 文字の 14 bytes で取得する。
		char cSerialNumber[14] = { 0 };
		int iResult = HBI_GetFPDSerialNumber(m_hHBI, cSerialNumber);
		if (!IsSuccess(iResult)) {
			return "";
		}
		LOG_INPROGRESSF("X383| FPD Serial Number: %s", cSerialNumber);
		return std::string(cSerialNumber);
	}
	/**
	 * @brief   デバイスの製品コードを取得する。
	 * @details 製品コードは 17 bytes 以上で取得される。
	 * @return  取得に成功した場合は製品コードの文字列、失敗した場合は空文字列。
	 */
	std::string GetFPDProductCode() {
		LOG_BEGINF0(7, "gYAK| HBIDeviceCtrl::GetFPDProductCode()");
		if (!m_bIsInitialized) { return ""; }
		// 16 文字 + NULL 文字の 17 bytes 以上確保する必要がある。
		// 最大サイズはDTに確認中。
		char cProductCode[128] = { 0 };

		int iResult = HBI_GetHbiProductCode(m_hHBI, cProductCode);
		if (!IsSuccess(iResult)) {
			return "";
		}
		m_strProductCode = std::string(cProductCode);
		LOG_INPROGRESSF("9ia7| FPD Product Code: %s", m_strProductCode.c_str());
		return m_strProductCode;
	}
	/**
	 * @brief   SDK のバージョンを取得する。
	 * @details SDK のバージョンは 64 bytes 以上で取得される。
	 * @return  true: 取得に成功, false: 取得に失敗
	 */
	std::string GetSDKVersion() const {
		LOG_BEGINF0(7, "Clfj| HBIDeviceCtrl::GetSDKVersion()");
		if (!m_bIsInitialized) { return ""; }
		// SDK バージョンの取得は 64 bytes 以上確保する必要がある。
		char cSDKVersion[128] = { 0 };

		int iResult = HBI_GetSDKVerion(m_hHBI, cSDKVersion);
		if (!IsSuccess(iResult)) {
			return "";
		}
		LOG_INPROGRESSF("5HdC| SDK Version: %s", cSDKVersion);
		return std::string(cSDKVersion);
	}

public:
	/**
	 * @brief   デバイスに接続する。関数を実行する。
	 * @details IPアドレスを char* 型に変換して、ConnectDevice(char*, unsigned short, char*, unsigned short) を呼び出す。
	 *          HBI_ConnectDetectorJumbo() が char* 型を要求するため、string* 型の文字列を char* 型に変換する必要がある。
	 * @param   kpstrDestIpAddr デバイスの IP アドレス。
	 * @param   kusDestPORT     デバイスのポート番号。
	 * @param   kpstrSrcIpAddr  PC の IP アドレス。
	 * @param   kusSrcPort      PC のポート番号。
	*/
	bool ConnectDevice(const std::string* kpstrDestIpAddr, const unsigned short kusDestPORT, const std::string* kpstrSrcIpAddr, const unsigned short kusSrcPort) {
		LOG_BEGINF0(7, "GUGw| HBIDeviceCtrl::ConnectDevice()");
		bool bIsSuccess = false;
		// SDK に渡すIPアドレスは const ではないため、メモリを確保して char* 型に変換する。
		const size_t szDestIpAddrBuffLen = kpstrDestIpAddr->length() + 1;
		const size_t szSrcIpAddrBuffLen  = kpstrSrcIpAddr->length() + 1;
		char* pcDestIpAddr = new char[szDestIpAddrBuffLen];
		char* pcSrcIpAddr  = new char[szSrcIpAddrBuffLen];
		memcpy_s(pcDestIpAddr, szDestIpAddrBuffLen, kpstrDestIpAddr->c_str(), szDestIpAddrBuffLen);
		memcpy_s(pcSrcIpAddr , szSrcIpAddrBuffLen , kpstrSrcIpAddr ->c_str(), szSrcIpAddrBuffLen);

		bool iResult = ConnectDevice(pcDestIpAddr, kusDestPORT, pcSrcIpAddr, kusSrcPort);
		if (iResult) { bIsSuccess = true ; }
		else         { bIsSuccess = false; }
		// メモリを解放する。
		delete[] pcDestIpAddr;
		delete[] pcSrcIpAddr;
		return bIsSuccess;
	}
	/**
	 * @brief   デバイスの接続を切断する。
	 * @details m_hHBI が持っているハンドルはここで解放される。
	 */
	bool DisconnectDevice() {
		LOG_BEGINF0(7, "gN20| HBIDeviceCtrl::DisconnectDevice()");

		// キャプチャ中の場合は、キャプチャを停止する。
		if (m_bIsCapturing) {
			LOG_INPROGRESSF("2ycH| Stopping capture before disconnecting device.");
			StopCapture();
		}

		// 画像バッファを解放する。
		if (m_pImageBuffer != nullptr) {
			LOG_INPROGRESSF("9NVM| Releasing image buffer.");
			m_pImageBuffer = nullptr;
		}

		// すでに接続されていない場合は切断する必要はない。
		if (!IsDeviceConnected()) {
			return true;
		}

		if (IsInitialized()) {
			LOG_INPROGRESSF("OtZV| Release HBI handle.");
			HBI_Destroy(m_hHBI);
			// 未初期化状態にする
			m_hHBI           = nullptr;
			m_bIsInitialized = false;
		}
		return true;
	}

	/**
	 * @brief  SDK の初期化を行う。
	 * @return true: 成功, false: 失敗
	 */
	bool Initialize() {
		LOG_BEGINF0(7, "HIAb| HBIDeviceCtrl::Initialize()");
		if (IsInitialized()) {
			LOG_INPROGRESSF("gJOK| HBI is already initialized.");
			return false;
		}
		// 初期化処理
		m_hHBI = HBI_Init(0);
		// 初期化フラグの更新
		if (m_hHBI) { m_bIsInitialized = true ; }
		else        { m_bIsInitialized = false; }
		return m_bIsInitialized;
	}
	/**
	 * @brief   SDK のイベントコールバック関数を設定する。
	 * @details イベントが起こった時、SDK が this ポインタを引数として UserHBICallback を呼び出す。
	 * @return  true: 成功, false: 失敗
	 */
	bool SetCallbackFunction() {
		LOG_BEGINF0(7, "dXzg| HBIDeviceCtrl::SetCallbackFunction()");
		int iResult = HBI_RegEventCallBackFun(m_hHBI, UserHBICallback, this);
		if (!IsSuccess(iResult)) {
			LOG_INPROGRESSF("rirp| HBI_RegEventCallBackFun failed.");
			return false;
		}
		return true;
	}

	/**
	 * @brief  デバイスが接続されているか確認する。
	 * @return true: 接続されている, false: 接続されていない
	 */
	bool IsDeviceConnected() const {
		LOG_BEGINF0(2, "RB58| HBIDeviceCtrl::IsConnected()");
		if (!IsInitialized()) { return false; }
		int iResult = HBI_IsConnect(m_hHBI);
		if (!iResult) {
			LOG_INPROGRESSF("5Iva| Device is not connected.");
			return false;
		}
		LOG_INPROGRESSF("FKXJ| Device is connected.");
		return true;
	}

	/**
	 * @brief  現在のキャプチャパラメータをログに出力する。
	 * @note   デバッグとして使用する。
	 * @return true: 出力に成功, false: 出力に失敗
	 */
	bool PrintCaptureParams() const {
		LOG_BEGINF0(7, "KaJ2| HBIDeviceCtrl::PrintCaptureParams()");
		if (!m_bIsInitialized) { return false; }
		LOG_INPROGRESSF("7Lb7| Current capture parameters:");
		int iResult;
		{
			// GainType: 1 ～ 11 の値を返す。
			int iGainType = HBI_GetPGALevel(m_hHBI);
			if (!iGainType) {
				LOG_INPROGRESSF("lMOT| HBI_GetPGALevel failed.");
				return false;
			}
			LOG_INPROGRESSF("eRFD|   Gain Type         : %d", iGainType);
		}
		{
			// iBinningType の値は 1: 1x1, 2: 2x2, 3: 3x3, 4: 4x4。
			unsigned int uiBinningType;
			iResult = HBI_GetBinning(m_hHBI, &uiBinningType);
			if (!IsSuccess(iResult)) {
				return false;
			}
			LOG_INPROGRESSF("md4X|   Binning Type      : %u", uiBinningType);
		}
		{
			// uiCaptureAreaLeft, uiCaptureAreaTop, uiCaptureAreaWidth, uiCaptureAreaHeight はそれぞれ左上の座標と幅、高さを表し。
			// それぞれの変数に値が格納される。
			unsigned int uiCaptureAreaLeft, uiCaptureAreaTop, uiCaptureAreaWidth, uiCaptureAreaHeight;
			iResult = HBI_GetCurZoomRect(m_hHBI, &uiCaptureAreaLeft, &uiCaptureAreaTop, &uiCaptureAreaWidth, &uiCaptureAreaHeight);
			if (!IsSuccess(iResult)) {
				return false;
			}
			LOG_INPROGRESSF("1YIe|   Capture Area      : (Left, Top) = (%u, %u) , Width x Height = %u x %u", uiCaptureAreaLeft, uiCaptureAreaTop, uiCaptureAreaWidth, uiCaptureAreaHeight);
		}
		{
			// imsExpTime は Exposure time をミリ秒単位で表す。
			int imsExpTime;
			{
				iResult = HBI_GetSelfDumpingTime(m_hHBI, &imsExpTime);
				if (!IsSuccess(iResult)) {
					return false;
				}
			}
			// 出力用に fps を計算する。
			float ffps = 1.0f / static_cast<float>(imsExpTime) * 1000.0f;
			LOG_INPROGRESSF("1UKJ|   Exposure Time (ms): %d ms"   , imsExpTime);
			LOG_INPROGRESSF("6aqO|   Frame Rate (fps)  : %.2f fps", ffps);
		}
		return true;
	}

	/**
	 * @brief  キャプチャパラメータを設定する。
	 * @param  krcaptureConfig: 設定するキャプチャパラメータを保持する構造体。
	 * @return true: 設定に成功, false: 設定に失敗
	 * @note   3030Z デュアル読出しのため、Width はフルエリア、Height は中心から等間隔にオフセットする必要がある。
	 *         現在 3030Z の採用予定はない。
	 *         2520Z は Height 方向のみオフセットが可能。
	 */
	bool SetCaptureParams(const struct CCaptureConfig& krcaptureConfig) {
		LOG_BEGINF0(7, "t4Jj| HBIDeviceCtrl::SetCaptureParams()");
		if (!IsInitialized()) { return false; }
		// 取得フレーム数
		SetCaptureFrame(krcaptureConfig.m_iCaptureFrame);

		int iResult;
		{
			// GainType
			LOG_INPROGRESSF("oWAx| Setting GainType     to %d", krcaptureConfig.m_iGainType);
			iResult = HBI_MSetPGALevel(m_hHBI, krcaptureConfig.m_iGainType);
			if (!IsSuccess(iResult)) {
				return false;
			}
		}
		{
			// Binning
			LOG_INPROGRESSF("cpe2| Setting BinningType  to %d", krcaptureConfig.m_iBinningType);
			iResult = HBI_MSetBinning(m_hHBI, krcaptureConfig.m_iBinningType);
			if (!IsSuccess(iResult)) {
				return false;
			}
		}
		{
			// Exposure time (= 1/fps)
			LOG_INPROGRESSF("VJPA| Setting ExposureTime to %d ms", krcaptureConfig.m_imsExpTime);
			iResult = HBI_MSetSelfDumpingTime(m_hHBI, krcaptureConfig.m_imsExpTime);
			if (!IsSuccess(iResult)) {
				return false;
			}
		}
		{
			/* ROI の設定
			 * 横方向は設定ができない。
			 * 3030zはデュアル読出しのため、中央から等間隔にオフセットする必要がある。そのため縦方向のサイズは偶数である必要がある。
			 * 2520zはシングル読出しのため、縦方向のオフセットは自由にできる。
			 * uleft, utop, uright, ubottom = 0 の時はフルサイズとなる。
			 */
			CMOS_ZOOM_RECT hbiCaptureArea;
			if (m_strProductCode == "X-Panel3030zFDM") {
				hbiCaptureArea.utop    = (krcaptureConfig.m_iOriginalHeight - krcaptureConfig.m_iCaptureAreaHeight) / 2;
				hbiCaptureArea.ubottom = hbiCaptureArea.utop + krcaptureConfig.m_iCaptureAreaTop - 1;
				hbiCaptureArea.uleft   = 0;
				hbiCaptureArea.uright  = 0;
			} else {
				hbiCaptureArea.utop    = krcaptureConfig.m_iCaptureAreaTop;
				hbiCaptureArea.ubottom = krcaptureConfig.m_iCaptureAreaTop + krcaptureConfig.m_iCaptureAreaHeight - 1;
				hbiCaptureArea.uleft   = 0;
				hbiCaptureArea.uright  = 0;
			}
			// ZoomWidth, ZoomHeight が 0 の時はフルサイズになるようにする。
			if (krcaptureConfig.m_iCaptureAreaWidth == 0) {
				hbiCaptureArea.uleft  = 0;
				hbiCaptureArea.uright = 0;
			}
			if (krcaptureConfig.m_iCaptureAreaHeight == 0) {
				hbiCaptureArea.utop    = 0;
				hbiCaptureArea.ubottom = 0;
			}
			LOG_INPROGRESSF("TKz2| Setting CaptureArea  to (Left, Top) = (%d, %d), (Right, Bottom) = (%d, %d)",
				hbiCaptureArea.uleft, hbiCaptureArea.utop, hbiCaptureArea.uright, hbiCaptureArea.ubottom);
			iResult = HBI_MSetZoomModeRect(m_hHBI, hbiCaptureArea);
			if (!IsSuccess(iResult)) {
				return false;
			}
		}
		return true;
	}

	/**
	 * @brief   取得するフレーム数に応じて画像バッファを確保する。
	 * @param   iCaptureFrame: 取得するフレーム数。
	 * @return  true: バッファの確保に成功, false: バッファの確保に失敗
	 * @details 画像バッファは m_a4duiImageBuffer に確保される。バッファのサイズは m_iImageWidth * m_iImageHeight * iCaptureFrame。
	 */
	bool AllocateImageBuffer(const int kiCaptureFrame) {
		// 未初期化、未接続、撮影中の場合はバッファを確保しない。
		LOG_BEGINF0(7, "Hef4| HBIDeviceCtrl::AllocateImageBuffer()");
		if (!(IsInitialized() && IsDeviceConnected() && !IsCapturing())) {
			LOG_INPROGRESSF("qvrL| Cannot allocate image buffer. Invalid state: IsInitialized=%d, IsConnected=%d, IsCapturing=%d",
				IsInitialized(), IsDeviceConnected(), IsCapturing());
			return false;
		}

		// 画像バッファを確保する。
		// どれかの値が 0 の場合はバッファを確保しない。
		if (!(m_iImageWidth && m_iImageHeight && kiCaptureFrame)) {
			LOG_INPROGRESSF("BmI3| Cannot allocate image buffer. Invalid parameters: Width=%d, Height=%d, CaptureFrame=%d",
				m_iImageWidth, m_iImageHeight, kiCaptureFrame);
			return false;
		}

		// 画像バッファを確保する。
		// 画像は4次元配列(X, Y, Z, T)で、フレーム方向はT次元で表す。
		m_a4duiImageBuffer.Resize(0, m_iImageWidth - 1, 0, m_iImageHeight - 1, 0, 0, 0, kiCaptureFrame - 1, CArray4D<uint16_t>::R_CLEAR);
		m_szImageBufferSize = m_a4duiImageBuffer.BufferLen();
		m_pImageBuffer      = m_a4duiImageBuffer.PTable();

		LOG_INPROGRESSF("HS4r| Allocated image buffer of size: %zu (Width=%d, Height=%d, CaptureFrame=%d)",
			m_szImageBufferSize, m_iImageWidth, m_iImageHeight, kiCaptureFrame);
		return true;
	}

	/**
	 * @brief   画像のサイズを取得してメンバ変数を更新する。
	 * @return  true: 取得に成功, false: 取得に失敗
	 * @details 画像のサイズは m_iImageWidth, m_iImageHeight に格納される。
	 *          取得に失敗した場合は m_iImageWidth, m_iImageHeight を 0 に設定される。
	 */
	bool UpdateImageProperties() {
		LOG_BEGINF0(7, "GI8J| HBIDeviceCtrl::UpdateImageProperties()");
		if (!IsInitialized()) { return false; }

		IMAGE_PROPERTY hbiImageProperty; // FPD プロパティの構造体
		int iResult = HBI_GetImageProperty(m_hHBI, &hbiImageProperty);
		if (!IsSuccess(iResult)) {
			// 取得に失敗した場合は、画像サイズを 0 に設定する。
			m_iImageWidth  = 0;
			m_iImageHeight = 0;
			return false;
		}
		m_iImageWidth  = hbiImageProperty.nwidth;
		m_iImageHeight = hbiImageProperty.nheight;
		LOG_INPROGRESSF("NaxT| Image Properties: Width=%d, Height=%d", m_iImageWidth, m_iImageHeight);
		return true;
	}

	/**
	 * @brief  画像取得を開始する。
	 * @return true: 取得の開始に成功, false: 取得の開始に失敗
	 * @note   LIVE_ACQ_DEFAULT_TYPE は HBIASDK のライブキャプチャモード。
	 */
	bool StartCapture() {
		LOG_BEGINF0(7, "2Fbj| HBIDeviceCtrl::StartCapture()");
		FPD_AQC_MODE hbiAqcMode;
		hbiAqcMode.eAqccmd = LIVE_ACQ_DEFAULT_TYPE;

		if (!IsInitialized()) { return false; }
		m_iFrameCounter = 0; // フレームカウンタをリセットする。
		int iResult = HBI_LiveAcquisition(m_hHBI, hbiAqcMode);
		if (!IsSuccess(iResult)) {
			return false;
		}
		m_bIsCapturing = true; // 画像取得中フラグを立てる。
		LOG_INPROGRESSF("s6ER| Capture started successfully.");
		return true;
	}

	/**
	 * @brief  画像取得を停止する。
	 * @return true: 停止に成功, false: 停止に失敗
	 */
	bool StopCapture() {
		LOG_BEGINF0(7, "iyUw| HBIDeviceCtrl::StopCapture()");
		if (!IsInitialized()) { return false; }
		HBI_StopAcquisition(m_hHBI);
		m_bIsCapturing = false; // 画像取得中フラグを下げる。
		LOG_INPROGRESSF("WxWg| Capture stopped successfully.");
		return true;
	}

	/**
	 * @brief  画像取得中か判断する。
	 */
	bool IsCapturing() const { return m_bIsCapturing; }

private:
	/**
	 * @brief  Jumbo Packet でデバイスに接続する。
	 * @param  pcDestIPAddr: FPD の IP アドレス。
	 * @param  kusDestPort : FPD のポート番号。
	 * @param  pcSrcIPAddr : PC の IP アドレス。
	 * @param  kusSrcPort  : PC のポート番号。
	 * @return true: 接続に成功, false: 接続に失敗
	 */
	bool ConnectDevice(char* pcDestIpAddr, const unsigned short kusDestPORT, char* pcSrcIpAddr, const unsigned short kusSrcPort) {
		LOG_BEGINF0(7, "MHyd| HBIDeviceCtrl::ConnectDevice()");
		if (!IsInitialized()) { return false; }
		int iResult = HBI_ConnectDetectorJumbo(m_hHBI, pcDestIpAddr, kusDestPORT, pcSrcIpAddr, kusSrcPort, 0);
		if (!IsSuccess(iResult)) {
			return false;
		}
		LOG_INPROGRESSF("ybDD| Connected to the device successfully.");
		return true;
	}
	/**
	 * @brief   HBI の関数の処理が成功したか否かを判定する。
	 * @details 失敗した場合は、エラーコードをログに出力する。
	 * @param   iResult: HBI の関数の戻り値。
	 * @return  true: 成功, false: 失敗
	 */
	bool IsSuccess(const int kiResult) const {
		LOG_BEGINF0(2, "UCQS| HBIDeviceCtrl::IsSuccess(kiResult = %d)", kiResult);
		HBIRETCODE hbierr = static_cast<HBIRETCODE>(kiResult);
		if (kiResult == HBI_SUCCSS) {
			return true;
		} else {
			LOG_INPROGRESSF("9y9z| Error: %s", CHBIERRStr(hbierr).c_str());
			return false;
		}
	}

	/**
	 * @brief 取得するフレーム数を設定する。
	 */
	void SetCaptureFrame(int iCaptureFrame) { m_iCaptureFrame = iCaptureFrame; }

	/**
	 * @brief SDK が初期化されているか判断する。
	 */
	bool IsInitialized() const { return m_bIsInitialized; }

	/**
	 * @brief   画像データをバッファに保存する。
	 * @param   pImageData: 画像データのポインタ。
	 * @return  なし。
	 * @details 画像取得後、コールバック関数から呼び出される。
	 *          指定枚数に達するまで、取得した画像データを m_pImageBuffer に保存する。
	 */
	bool SaveImageBuffer(const void* pImageData) {
		LOG_BEGINF0(7, "KiH8| HBIDeviceCtrl::SaveImageBuffer()");
		if (m_iFrameCounter >= m_iCaptureFrame) {
			// 指定枚数撮影したので保存はしない。
			LOG_INPROGRESSF("7wM9| Frame counter %d reached capture frame %d. Stopping capture.", m_iFrameCounter, m_iCaptureFrame);
			m_bIsCapturing = false;
			return false;
		}

		// 取得したフレーム数だけオフセットする。
		const int    kiFramePixelCount = m_iImageWidth * m_iImageHeight;
		const size_t kszOffsetBuffSize = static_cast<size_t>(m_iFrameCounter * kiFramePixelCount);

		try {
			memcpy_s(
				m_pImageBuffer + kszOffsetBuffSize,                           // コピー先のバッファの先頭アドレス
				(m_szImageBufferSize - kszOffsetBuffSize) * sizeof(uint16_t), // バッファの残りサイズ
				pImageData,                                                   // コピー元のバッファの先頭アドレス
				kiFramePixelCount * sizeof(uint16_t)                          // コピーするバイト数
			);
		}
		catch (const std::exception& eError) {
			LOG_INPROGRESSF("RLIT| Exception occurred while saving image data: %s", eError.what());
			return false;
		}

		// 保存に成功したらフレーム数をカウントする。
		LOG_INPROGRESSF("779v| Saved image data for frame %d", m_iFrameCounter);
		m_iFrameCounter++;
		return true;
	}

	/**
	 * @brief   SDK のイベントコールバック関数。SDK がイベントを検知した時に呼び出される。
	 * @param   pContext       SDK 側で取得したオブジェクトのポインタ。
	 * @param   ifpdId         デバイス ID。
	 * @param   uceventId      イベント ID。
	 * @param   peventParam1   イベントに関するパラメータ。
	 * @param   ieventParam2   イベントに関するパラメータ。
	 * @param   ieventParam3   イベントに関するパラメータ。
	 * @param   ieventParam4   イベントに関するパラメータ。
	 * @return  1 を返す。
	 * @details SDK の仕様上、コールバック関数は static メソッドかつ int 型の関数である必要がある。
	 */
	static int UserHBICallback(void* pContext, int ifpdId, unsigned char uceventId, void* peventParam1, int ieventParam2, int ieventParam3, int ieventParam4) {
		// SDK側で取得したポインタを CHBIDeviceCtrl クラスのオブジェクトのポインタとしてキャストする。
		CHBIDeviceCtrl* pCHBIDeviceCtrl = static_cast<CHBIDeviceCtrl*>(pContext);
		if (!pCHBIDeviceCtrl) { return 0; }
		pCHBIDeviceCtrl->OnHBICallback(ifpdId, uceventId, peventParam1, ieventParam2, ieventParam3, ieventParam4);
		return 1;
	}

	/**
	 * @brief  SDK のイベントコールバック関数から呼び出される実装関数。イベントに応じて処理を行う。
	 * @param  ifpdId         デバイス ID。
	 * @param  uceventId      イベント ID。
	 * @param  peventParam1   イベントに関するパラメータ
	 * @param  ieventParam2   イベントに関するパラメータ
	 * @param  ieventParam3   イベントに関するパラメータ
	 * @param  ieventParam4   イベントに関するパラメータ
	 */
	void OnHBICallback(int ifpdId, unsigned char uceventId, void* peventParam1, int ieventParam2, int ieventParam3, int ieventParam4) {
		LOG_BEGINF0(2, "6P6w| CHBIDeviceCtrl::OnHBICallback()");
		// peventParam1 以外は使っていないが、 SDK の仕様上、引数として受け取る必要がある。
		(void)ifpdId;
		(void)ieventParam2;
		(void)ieventParam3;
		(void)ieventParam4;
		if (uceventId == ECALLBACK_TYPE_MULTIPLE_IMAGE) {
			// SDK から画像データが送られてきた場合、peventParam1 に IMAGE_DATA_ST 構造体のポインタが渡される。
			const void* pImageData = static_cast<IMAGE_DATA_ST*>(peventParam1)->databuff;
			if (!pImageData) {
				LOG_INPROGRESSF("cDgc| Received null image data pointer.");
				return;
			}
			SaveImageBuffer(pImageData);
		}
		return;
	}
};
