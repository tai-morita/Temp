#pragma once

#include "resource.h"

#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <iomanip>

#include "../Common/HBI_DLL/INCLUDES/HbiError.h"
#include "../Common/HBI_DLL/INCLUDES/HbiFpd.h"
#include "../Common/HBI_DLL/INCLUDES/HbiType.h"
#include "../Common/tstring.h"
#include "../Array/Array2D.h"
#include "../Array/Array4D.h"
#include "../CSmartLog/SmartLog.h"
#include "HBICaptureConfig.h"

// HBI で通信するデバイスの動作を制御するクラス。
class CHBIDeviceCtrl
{
private:

	void* m_hHBI;                             // HBISDK のハンドル。これで SDK の関数を呼び出す。
	bool  m_bIsHBIInitialized;                // 初期化されているか示すフラグ。
	std::vector<uint16_t> m_vecuiImageBuffer; // 最終の画像データを保存するバッファ。
	uint16_t* m_pImageBuffer;                 // 画像バッファのポインタ。
	size_t m_iImageBufferSize;                // 画像バッファのサイズ。
	bool m_bIsCapturing;                      // 画像取得中かどうかを示すフラグ。
	int m_iFrameCounter;                      // 取得したフレーム数をカウントする。
	int m_iCaptureFrame;                      // 取得するフレームの総数。
	int m_iImageWidth;                        // 取得する画像サイズ(幅)。
	int m_iImageHeight;                       // 取得する画像サイズ(高さ)。
	CArray2D<unsigned short> m_a2dusImage;    // 取得した画像データを保存する 2 次元配列。
	CArray4D<uint16_t> m_a4duiImageBuffer;    // 取得した画像データを保存する 2 次元配列。


	std::string m_strProductCode;  // デバイスの製品コード。

public:
	// コンストラクタ。
	CHBIDeviceCtrl()
		: m_hHBI(nullptr)
		, m_bIsHBIInitialized(false)
		, m_bIsCapturing(false)
		, m_iFrameCounter(0)
		, m_iCaptureFrame(0)
		, m_iImageWidth(0)
		, m_iImageHeight(0)
		, m_pImageBuffer(nullptr)
		, m_iImageBufferSize(0)
		, m_strProductCode("")
	{
		LOG_BEGINF0(7, "KwCZ|  HBIDeviceCtrl::HBIDeviceCtrl()");

	}

	// デストラクタ。
	~CHBIDeviceCtrl()
	{
		LOG_BEGINF0(7, "EFg7|  HBIDeviceCtrl::~HBIDeviceCtrl()");
		if (IsCapturing()) {
			std::cout << "Stopping capture before destruction...\n";
			StopCapture();
		}
		if (IsHBIInitialized()) {
			HBI_Destroy(m_hHBI);
			m_bIsHBIInitialized = false;
		}
		if (m_pImageBuffer != nullptr) {
			// delete[] m_pImageBuffer;
			m_pImageBuffer = nullptr;
		}
	}


public:
	// CapturerHBIDlg.cpp からアクセスするためのゲッターメソッド。
	const uint16_t* GetImageBuffer() const { return m_pImageBuffer; }
	const size_t GetImageBuf() const { return m_iImageBufferSize; }
	const int GetImageWidth() const { return m_iImageWidth; }
	const int GetImageHeight() const { return m_iImageHeight; }
	const CArray4D<uint16_t>& Geta4duiImage() const { return m_a4duiImageBuffer; }

	/**
	 * @brief デバイスに接続する。関数を実行する。
	 * @note  SDK の HBI_ConnectDetectorJumbo 関数の引数が char* 型なので、string 型を受け取るオーバーロードを作成した。
	*/
	bool ConnectDevice(std::string* pstrDestIpAddr, const unsigned short kusDestPORT, std::string* pstrSrcIpAddr, const unsigned short kusSrcPort) {
		bool bIsSuccess = false;
		// const ではないため、メモリを確保して char* 型に変換する。
		size_t iDestIpAddrBuffLen = pstrDestIpAddr->length() + 1;
		size_t iSrcIpAddrBuffLen = pstrSrcIpAddr->length() + 1;

		char* pcDestIpAddr = new char[iDestIpAddrBuffLen];
		char* pcSrcIpAddr = new char[iSrcIpAddrBuffLen];

		memcpy_s(pcDestIpAddr, iDestIpAddrBuffLen, pstrDestIpAddr->c_str(), iDestIpAddrBuffLen);
		memcpy_s(pcSrcIpAddr, iSrcIpAddrBuffLen, pstrSrcIpAddr->c_str(), iSrcIpAddrBuffLen);
		bool iResult = ConnectDevice(pcDestIpAddr, kusDestPORT, pcSrcIpAddr, kusSrcPort);
		if (IsSuccess(iResult)) { bIsSuccess = true; }
		else { bIsSuccess = false; }
		// メモリを解放する。
		delete[] pcDestIpAddr;
		delete[] pcSrcIpAddr;

		return bIsSuccess;
	}

	/**
	 * @brief SDK の初期化を行う。
	 */
	bool Initialize() {
		LOG_BEGINF0(7, "HIAb|  HBIDeviceCtrl::Initialize()");

		if (IsHBIInitialized()) {
			LOG_INPROGRESSF("gJOK|  HBI is already initialized.");
			return false;
		}

		m_hHBI = HBI_Init(0);

		if (!m_hHBI) { m_bIsHBIInitialized = false; }
		else { m_bIsHBIInitialized = true; }
		return m_bIsHBIInitialized;
	}

	/**
	 * @brief SDK のイベントコールバック関数を設定する。
	 * @note イベントが起こった時、SDK が UserHBICallback を呼び出す。
	 */
	void SetCallbackFunction() { HBI_RegEventCallBackFun(m_hHBI, UserHBICallback, this); }

	/**
	 * @brief  Jumbo Packet でデバイスに接続する。
	 * @param  pcFPDIP   デバイスの IP アドレス。
	 * @param  kusFPDPORT デバイスのポート番号。
	 * @param  pcPCIP    PC の IP アドレス。
	 * @param  kusPCPORT  PC のポート番号。
	 * @return 接続に成功した場合は true、失敗した場合は false。
	 */
	bool ConnectDevice(char* pcDestIpAddr, const unsigned short kusDestPORT, char* pcSrcIpAddr, const unsigned short kusSrcPort) {
		LOG_BEGINF0(7, "MHyd| HBIDeviceCtrl::ConnectDevice()");

		if (!IsHBIInitialized()) { return false; }
		int iResult = HBI_ConnectDetectorJumbo(m_hHBI, pcDestIpAddr, kusDestPORT, pcSrcIpAddr, kusSrcPort, 0);
		if (!IsSuccess(iResult)) {
			return false;
		}
		LOG_INPROGRESSF("ybDD| Connected to the device successfully.");

		return true;
	}

	/**
	 * @brief デバイスが接続されているか確認する。
	 * @return 接続されている場合は true、接続されていない場合は false。
	 */
	bool IsConnected() {
		LOG_BEGINF0(7, "RB58|  HBIDeviceCtrl::IsConnected()");
		if (!IsHBIInitialized()) { return false; }
		int iResult = HBI_IsConnect(m_hHBI);
		if (!iResult) {
			LOG_INPROGRESSF("5Iva|  Device is not connected.");
			return false;
		}
		LOG_INPROGRESSF("FKXJ|  Device is connected.");
		return true;
	}

	/**
	 * @brief デバイスのシリアル番号を取得する。
	 * @return 取得に成功した場合はシリアル番号の文字列、失敗した場合は空文字列。
	 */
	std::string GetFPDSerialNumber() {
		LOG_BEGINF0(7, "uHRU|  HBIDeviceCtrl::GetFPDSerialNumber()");
		// パネルの情報を取得する。
		if (!m_bIsHBIInitialized) { return ""; }

		char cSerialNumber[14] = { 0 };

		int iResult = HBI_GetFPDSerialNumber(m_hHBI, cSerialNumber);
		if (!IsSuccess(iResult)) {
			return "";
		}
		LOG_INPROGRESSF("X383|  FPD Serial Number: %s", cSerialNumber);
		return std::string(cSerialNumber);
	}

	/**
	 * @brief デバイスの製品コードを取得する。
	 * @return 取得に成功した場合は製品コードの文字列、失敗した場合は空文字列。
	 */
	std::string  GetFPDProductCode() {
		LOG_BEGINF0(7, "gYAK|  HBIDeviceCtrl::GetFPDProductCode()");
		if (!m_bIsHBIInitialized) { return ""; }

		char pcProductCode[128] = { 0 };
		int iResult = HBI_GetHbiProductCode(m_hHBI, pcProductCode);
		if (!IsSuccess(iResult)) {
			return "";
		}
		m_strProductCode = std::string(pcProductCode);
		LOG_INPROGRESSF("9ia7|  FPD Product Code: %s", m_strProductCode.c_str());
		return m_strProductCode;
	}

	/**
	 * @brief SDK のバージョンを取得する。
	 * @return 取得に成功した場合は SDK のバージョンの文字列、失敗した場合は空文字列。
	 */
	std::string GetSDKVersion() {
		LOG_BEGINF0(7, "Clfj|  HBIDeviceCtrl::GetSDKVersion()");
		if (!m_bIsHBIInitialized) { return ""; }

		char pcSDKVersion[128] = { 0 };
		int iResult = HBI_GetSDKVerion(m_hHBI, pcSDKVersion);

		if (!IsSuccess(iResult)) {
			return "";
		}
		LOG_INPROGRESSF("5HdC|  SDK Version: %s", pcSDKVersion);
		return std::string(pcSDKVersion);
	}

	/**
	 * @brief  現在のキャプチャパラメータをログに出力する。
	 * @return すべてのパラメータの取得に成功した場合は true、どこかで取得に失敗した場合は false。
	 * @note   デバッグとして使用する。
	 */
	bool PrintCaptureParams() {
		LOG_BEGINF0(7, "KaJ2|  HBIDeviceCtrl::PrintCaptureParams()");
		if (!m_bIsHBIInitialized) { return false; }

		int iResult;

		// GainType: HBI_GetPGALevel の戻り値は GainType (1 ～ 11) が成功、失敗は -1。
		int iGainType = HBI_GetPGALevel(m_hHBI);
		if (iGainType < 1 || iGainType > 11) {
			LOG_INPROGRESSF("lMOT|  HBI_GetPGALevel failed. iGainType=%d", iGainType);
			return false;
		}

		// iBinningType の値は 1: 1x1, 2: 2x2, 3: 3x3, 4: 4x4。
		unsigned int uiBinningType;
		iResult = HBI_GetBinning(m_hHBI, &uiBinningType);
		if (!IsSuccess(iResult)) {
			return false;
		}

		// ihbiCaptureOffsetLeft, ihbiCaptureOffsetTop, ihbiCaptureOffsetWidth, ihbiCaptureOffsetHeight はそれぞれ左上の座標と幅、高さを表し。
		// それぞれの変数に値が格納される。
		unsigned int uiCaptureOffsetLeft, uiCaptureOffsetTop, uiCaptureOffsetWidth, uiCaptureOffsetHeight;
		iResult = HBI_GetCurZoomRect(m_hHBI, &uiCaptureOffsetLeft, &uiCaptureOffsetTop, &uiCaptureOffsetWidth, &uiCaptureOffsetHeight);
		if (!IsSuccess(iResult)) {
			return false;
		}

		// imsExposureTime は Exposure time をミリ秒単位で表す。
		int imsExposureTime;
		iResult = HBI_GetSelfDumpingTime(m_hHBI, &imsExposureTime);
		if (!IsSuccess(iResult)) {
			return false;
		}
		// 出力用に fps を計算する。
		float fExpSec = static_cast<float>(imsExposureTime) / 1000.0f;
		float ffps = 1.0f / static_cast<float>(fExpSec);

		// 取得したパラメータをログに出力する。
		LOG_INPROGRESSF("7Lb7|  Current capture parameters:");
		LOG_INPROGRESSF("md4X|    Binning Type     : %u", uiBinningType);
		LOG_INPROGRESSF("eRFD|    Gain Type        : %d", iGainType);
		LOG_INPROGRESSF("1UKJ|    Exposure Time (s): %.2f sec", fExpSec);
		LOG_INPROGRESSF("6aqO|    Frame per Second : %.2f fps", ffps);
		LOG_INPROGRESSF("1YIe|    Capture Area     : (Left, Top) = (%u, %u) , Width x Height = %u x %u", uiCaptureOffsetLeft, uiCaptureOffsetTop, uiCaptureOffsetWidth, uiCaptureOffsetHeight);
		return true;
	}

	/**
	 * @brief  キャプチャパラメータを設定する。
	 * @param  iGainType            Gain Type (1 ~ 11)。
	 * @param  imsExposureTime      Exposure time (ms)。
	 * @param  iCaptureFrame        取得するフレーム数。
	 * @param  iBinningType         Binning (1: 1x1, 2: 2x2, 3: 3x3, 4: 4x4)。
	 * @param  iOriginalSizeWidth   フルエリアの幅(3030z のキャプチャ領域の指定で必要)。
	 * @param  iOriginalSizeHeight  フルエリアの高さ(3030z のキャプチャ領域の指定で必要)。
	 * @param  iCaptureOffsetLeft   キャプチャ領域の左上の x ()。
	 * @param  iCaptureOffsetTop    キャプチャ領域の左上の y ()。
	 * @param  iCaptureOffsetWidth  キャプチャ領域の幅。
	 * @param  iCaptureOffsetHeight キャプチャ領域の高さ。
	 * @return すべてのパラメータの設定に成功した場合は true、どこかで設定に失敗した場合は false。
	 * @note   3030z デュアル読出しのため、Width はフルエリア、Height は中心から等間隔にオフセットする必要がある。
	 *         現在 3030z の採用予定はない。
	 */
	bool SetCaptureParams(struct CaptureConfig& captureConfig) {
		LOG_BEGINF0(7, "t4Jj|  HBIDeviceCtrl::SetCaptureParams()");
		if (!IsHBIInitialized()) { return false; }
		// 取得フレーム数。
		SetCaptureFrame(captureConfig.m_iCaptureFrame);

		// GainType。
		int iResult = HBI_MSetPGALevel(m_hHBI, captureConfig.m_iGainType);
		if (!IsSuccess(iResult)) {
			return false;
		}

		// Binning。
		iResult = HBI_MSetBinning(m_hHBI, captureConfig.m_iBinningType);
		if (!IsSuccess(iResult)) {
			return false;
		}

		// Exposure time (= 1/fps)。
		iResult = HBI_MSetSelfDumpingTime(m_hHBI, captureConfig.m_imillisecExposureTime);
		if (!IsSuccess(iResult)) {
			return false;
		}

		/* ROI の設定。
		* 横方向は設定ができない。
		* 3030zはデュアル読出しのため、中央から等間隔にオフセットする必要がある。そのため縦方向のサイズは偶数である必要がある。
		* 2520zはシングル読出しのため、縦方向のオフセットは自由にできる。
		* uleft, utop, uright, ubottom = 0 の時はフルサイズとなる。
		*/
		CMOS_ZOOM_RECT hbiCaptureOffset;
		if (m_strProductCode == "X-Panel3030zFDM") {
			hbiCaptureOffset.utop = (captureConfig.m_iOriginalHeight - captureConfig.m_iCaptureAreaHeight) / 2;
			hbiCaptureOffset.ubottom = hbiCaptureOffset.utop + captureConfig.m_iCaptureAreaTop - 1;
			hbiCaptureOffset.uleft = 0;
			hbiCaptureOffset.uright = 0;
		}
		else {
			hbiCaptureOffset.utop = captureConfig.m_iCaptureAreaTop;
			hbiCaptureOffset.ubottom = captureConfig.m_iCaptureAreaTop + captureConfig.m_iCaptureAreaHeight - 1;
			hbiCaptureOffset.uleft = captureConfig.m_iCaptureAreaLeft;
			hbiCaptureOffset.uright = captureConfig.m_iCaptureAreaLeft + captureConfig.m_iCaptureAreaWidth - 1;
		}
		// ZoomWidth, ZoomHeight が 0 の時はフルサイズになるようにする。
		if (captureConfig.m_iCaptureAreaWidth == 0) {
			hbiCaptureOffset.uleft = 0;
			hbiCaptureOffset.uright = 0;
		}
		if (captureConfig.m_iCaptureAreaHeight == 0) {
			hbiCaptureOffset.utop = 0;
			hbiCaptureOffset.ubottom = 0;
		}
		iResult = HBI_MSetZoomModeRect(m_hHBI, hbiCaptureOffset);
		if (!IsSuccess(iResult)) {
			return false;
		}
		return true;
	}

	/**
	 * @brief  取得するフレーム数に対して画像バッファを確保する。
	 * @param  iCaptureFrame 取得するフレーム数。
	 * @return 画像バッファの確保に成功した場合は true、失敗した場合は false。
	 * @note   画像バッファは m_vecuiImageBuffer に確保される。バッファのサイズは m_iImageWidth * m_iImageHeight * iCaptureFrame。
	 */
	bool AllocateImageBuffer(int iCaptureFrame) {
		// 未初期化、未接続、未撮影の場合はバッファを確保しない。
		LOG_BEGINF0(7, "Hef4|  HBIDeviceCtrl::AllocateImageBuffer()");
		if (!IsHBIInitialized()
			|| !IsConnected()
			|| IsCapturing()) {
			LOG_INPROGRESSF("qvrL|  Cannot allocate image buffer. Invalid state: IsHBIInitialized=%d, IsConnected=%d, IsCapturing=%d",
				IsHBIInitialized(), IsConnected(), IsCapturing());
			return false;
		}

		// すでにバッファが確保されている場合は、一度解放してから再度確保する。
		if (m_pImageBuffer != nullptr) {
			// delete[] m_pImageBuffer;
			m_pImageBuffer = nullptr;
		}

		// 画像バッファを確保する。
		// どれかの値が 0 の場合はバッファを確保しない。
		if (!m_iImageWidth || !m_iImageHeight || !iCaptureFrame) {
			LOG_INPROGRESSF("BmI3|  Cannot allocate image buffer. Invalid parameters: Width=%d, Height=%d, CaptureFrame=%d",
				m_iImageWidth, m_iImageHeight, iCaptureFrame);
			return false;
		}

		m_a4duiImageBuffer.Resize(0, m_iImageWidth - 1, 0, m_iImageHeight - 1, 0, 0, 0, iCaptureFrame - 1, CArray4D<uint16_t>::R_CLEAR);
		m_iImageBufferSize = m_a4duiImageBuffer.BufferLen();
		m_pImageBuffer = m_a4duiImageBuffer.PTable();

		// m_iImageBufferSize = static_cast<size_t>(m_iImageWidth) * static_cast<size_t>(m_iImageHeight) * static_cast<size_t>(iCaptureFrame);
		// m_pImageBuffer = new uint16_t[m_iImageBufferSize];
		LOG_INPROGRESSF("HS4r|  Allocated image buffer of size: %zu (Width=%d, Height=%d, CaptureFrame=%d)",
			m_iImageBufferSize, m_iImageWidth, m_iImageHeight, iCaptureFrame);
		return true;
	}

	/**
	 * @brief  画像のサイズを取得してメンバ変数に
	 * @return 画像のサイズの取得に成功した場合は true、失敗した場合は false。
	 * @note   画像のサイズは m_iImageWidth, m_iImageHeight に格納される。
	 */
	bool UpdateImageProperties() {
		LOG_BEGINF0(7, "GI8J|  HBIDeviceCtrl::UpdateImageProperties()");
		if (!IsHBIInitialized()) { return false; }

		IMAGE_PROPERTY hbiImageProperty; // FPD プロパティの構造体
		int iResult = HBI_GetImageProperty(m_hHBI, &hbiImageProperty);
		m_iImageWidth = hbiImageProperty.nwidth;
		m_iImageHeight = hbiImageProperty.nheight;
		if (!IsSuccess(iResult)) {
			return false;
		}
		LOG_INPROGRESSF("NaxT|  Image Properties: Width=%d, Height=%d", m_iImageWidth, m_iImageHeight);
		return true;
	}

	/**
	 * @brief  画像取得を開始する。
	 * @return 画像取得の開始に成功した場合は true、失敗した場合は false。
	 * @note   LIVE_ACQ_DEFAULT_TYPE は HBIASDK のライブキャプチャモード。
	 */
	bool StartCapture() {
		LOG_BEGINF0(7, "2Fbj|  HBIDeviceCtrl::StartCapture()");
		FPD_AQC_MODE hbi_aqc_mode;
		hbi_aqc_mode.eAqccmd = LIVE_ACQ_DEFAULT_TYPE;

		if (!IsHBIInitialized()) { return false; }
		m_iFrameCounter = 0;
		int iResult = HBI_LiveAcquisition(m_hHBI, hbi_aqc_mode);
		if (!IsSuccess(iResult)) {
			return false;
		}
		m_bIsCapturing = true; // 画像取得中フラグを立てる。
		LOG_INPROGRESSF("s6ER|  Capture started successfully.");
		return true;
	}

	/**
	 * @brief  画像取得を停止する。
	 * @return 画像取得の停止に成功した場合は true、失敗した場合は false。
	 */
	bool StopCapture() {
		LOG_BEGINF0(7, "iyUw|  HBIDeviceCtrl::StopCapture()");
		if (!IsHBIInitialized()) { return false; }
		HBI_StopAcquisition(m_hHBI);
		m_bIsCapturing = false; // 画像取得中フラグを下げる。
		LOG_INPROGRESSF("WxWg|  Capture stopped successfully.");
		return true;
	}

	// 画像取得中か判断する。
	bool IsCapturing() const { return m_bIsCapturing; }

	/**
	 * @brief  デバイスとの接続を切断し、SDK を終了する。
	 * @return 接続の切断と SDK の終了に成功した場合は true、失敗した場合は false。
	 */
	bool DisconnectDevice() {
		// 画像取得中の場合は停止する。
		LOG_BEGINF0(7, "Kx1G|  HBIDeviceCtrl::DisconnectDevice()");
		if (IsCapturing()) {
			LOG_INPROGRESSF("RSti|  Stopping capture before disconnecting device.");
			StopCapture();
		}
		// 画像バッファを解放する。
		if (m_pImageBuffer != nullptr) {
			// delete[] m_pImageBuffer;
			m_pImageBuffer = nullptr;
		}

		if (IsHBIInitialized()) {
			HBI_Destroy(m_hHBI);
			m_hHBI = nullptr;
			m_bIsHBIInitialized = false;
		}
		return true;
	}

private:

	/**
	 * @brief HBI の関数の処理が成功したか否かを判定する。
	 * @param  iResult HBI の関数の戻り値。
	 * @return 成功した場合は true、失敗した場合は false。
	 */
	bool IsSuccess(int iResult) const {
		LOG_BEGINF0(7, "UCQS|  HBIDeviceCtrl::IsSuccess()");
		if (iResult == HBI_SUCCSS) {
			return true;
		}
		int iErrorCode = static_cast<int>(sizeof(CrErrStrList) / sizeof(CrErrStrList[0]));
		LOG_INPROGRESSF("GFXI|  HBI function failed. iErrorCode=%d, iResult=%d", iErrorCode, iResult);
		const char* pcErrorMessage = HBI_GetError(CrErrStrList, iErrorCode, iResult);
		LOG_INPROGRESSF("RJVh|  Error message: %s", pcErrorMessage);
		return false;
	}

	// 取得するフレーム数を設定する。
	void SetCaptureFrame(int iCaptureFrame) { m_iCaptureFrame = iCaptureFrame; }

	// HBI が初期化されているか判断する。
	bool IsHBIInitialized() const { return m_bIsHBIInitialized; }

	/**
	 * @brief  画像データをバッファに保存する。
	 * @param  pvParam1 SDK のイベントコールバック関数から渡される画像データのポインタ。
	 * @return なし。
	 * @note   コールバック関数から呼び出される。
	 *         pvParam1 は IMAGE_DATA_ST 構造体のポインタ。
	 *         databuff メンバに画像データが格納されている。
	 */
	void SaveImageBuffer(void* pvParam1) {
		LOG_BEGINF0(7, "KiH8|  HBIDeviceCtrl::SaveImageBuffer()");
		if (m_iFrameCounter >= m_iCaptureFrame) {
			// 指定枚数撮影したので保存はしない。
			LOG_INPROGRESSF("7wM9| Frame counter %d reached capture frame %d. Stopping capture.", m_iFrameCounter, m_iCaptureFrame);
			m_bIsCapturing = false;
			return;
		}

		// 取得した画像をバッファに保存するため、フレーム数だけオフセットする。
		const size_t kiFramePixelCount = static_cast<size_t>(m_iImageWidth) * static_cast<size_t>(m_iImageHeight);
		const size_t kiOffset = static_cast<size_t>(m_iFrameCounter) * kiFramePixelCount;

		// IMAGE_DATA_ST構造体内のdatabuffに画像データがあるので、それをバッファにコピーする。
		memcpy_s(
			m_pImageBuffer + kiOffset,
			m_iImageBufferSize * sizeof(uint16_t) - kiOffset * sizeof(uint16_t), // バッファの残りサイズを計算する。
			static_cast<IMAGE_DATA_ST*>(pvParam1)->databuff,
			kiFramePixelCount * sizeof(uint16_t)
		);

		// 保存に成功したらフレーム数をカウントする。
		LOG_INPROGRESSF("779v|  Saved image data for frame %d", m_iFrameCounter);
		m_iFrameCounter++;

	}

	/**
	 * @brief  SDK のイベントコールバック関数。SDK がイベントを検知した時に呼び出される。
	 * @param  pContext       オブジェクトのポインタ。
	 * @param  ifpdId         デバイス ID。
	 * @param  uceventId      イベント ID。
	 * @param  peventParam1   イベントに関するパラメータ。
	 * @param  ievantParam2   イベントに関するパラメータ。
	 * @param  ievantParam3   イベントに関するパラメータ。
	 * @param  ievantParam4   イベントに関するパラメータ。
	 */

	static int UserHBICallback(
		void* pContext,
		int           ifpdId,
		unsigned char uceventId,
		void* peventParam1,
		int           ievantParam2,
		int           ievantParam3,
		int           ievantParam4
	)
	{
		// SDK側で取得したポインタを CHBIDeviceCtrl クラスのオブジェクトのポインタとしてキャストする。
		CHBIDeviceCtrl* pCHBIDeviceCtrl = static_cast<CHBIDeviceCtrl*>(pContext);

		if (!pCHBIDeviceCtrl) { return 0; }
		return pCHBIDeviceCtrl->OnHBICallback(
			ifpdId,
			uceventId,
			peventParam1,
			ievantParam2,
			ievantParam3,
			ievantParam4
		);
	}

	/**
	 * @brief  SDK のイベントコールバック関数から呼び出される実装関数。イベントに応じて処理を行う。
	 * @param  ifpdId         デバイス ID。
	 * @param  uceventId        イベント ID。
	 * @param  peventParam1   イベントに関するパラメータ
	 * @param  ievantParam2   イベントに関するパラメータ
	 * @param  ievantParam3   イベントに関するパラメータ
	 * @param  ievantParam4   イベントに関するパラメータ
	 * @return 1 を返す。
	 */
	int OnHBICallback(
		int           ifpdId,
		unsigned char uceventId,
		void* peventParam1,
		int           ievantParam2,
		int           ievantParam3,
		int           ievantParam4
	)
	{
		// LOG_BEGINF0(7, "6P6w|  CHBIDeviceCtrl::OnHBICallback()");
		// peventParam1 以外は使っていないが、 SDK の仕様上、引数として受け取る必要がある。
		(void)ifpdId;
		(void)ievantParam2;
		(void)ievantParam3;
		(void)ievantParam4;
		if (uceventId == ECALLBACK_TYPE_MULTIPLE_IMAGE) {
			SaveImageBuffer(peventParam1);
		}
		if (uceventId == ECALLBACK_TYPE_SET_CFG_OK) {
			// LOG_INPROGRESSF("fDCh|  Set parameters successfully.");
		}
		if (uceventId == ECALLBACK_TYPE_PACKET_MISS) {
			// LOG_INPROGRESSF("J0JU|  Packet lost.");
		}
		if (uceventId == ECALLBACK_TYPE_BUFFER_WARNING) {
			// LOG_INPROGRESSF("Sj3Y|  Buffer warning.");
		}
		return 1;
	}
};
