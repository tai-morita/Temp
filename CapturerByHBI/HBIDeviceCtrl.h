#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <iomanip>

#include "HbiFpd.h"  
#include "HbiType.h"  
#include "HbiError.h"  

// HBI で通信するデバイスの動作を制御するクラス  
class CHBIDeviceCtrl  
{ 
private:
	COMM_CFG         m_commCfg; // 通信の設定を保持する構造体  
	FPD_AQC_MODE     m_aqcMode;
	IMAGE_DATA_ST    m_imgData; // 画像取得時のデータを保存する構造体
	CMOS_ZOOM_RECT   m_zoomRect; // ズームモードの矩形を保存する構造体
	FPD_MODE_LIST    m_modeList; // モードリストを保存する構造体
	FPD_MODE_DATA    m_modeData; // モードデータを保存する構造体

	void* m_hHBI;  
	bool  m_bIsHBIInitialized;  
	std::vector<uint16_t> m_vecuiImageBuffer; // 最終の画像データを保存するバッファ

	bool m_bIsCapturing; // 画像取得中かどうかを示すフラグ
	int m_iFrame; // 取得したフレーム数をカウントする変数
	int m_iCaptureFrame; // 取得するフレームの総数

	int m_iImageWidth;
	int m_iImageHeight;

public:
	// コンストラクタ  
	CHBIDeviceCtrl()
		: m_hHBI(nullptr)
		, m_bIsHBIInitialized(false)
		, m_bIsCapturing(false)
		, m_iFrame(0)
		, m_iCaptureFrame(0)
		, m_iImageWidth(0)
		, m_iImageHeight(0)
	{
		std::memset(&m_modeData, 0, sizeof(m_modeData));
		std::memset(&m_modeList, 0, sizeof(m_modeList));
		std::memset(&m_zoomRect, 0, sizeof(m_zoomRect));
	}

	// デストラクタ
	~CHBIDeviceCtrl()
	{
		if (IsCapturing()) {
			std::wcout << L"Stopping capture before destruction...\n";
			StopCapture();
		}
		if (IsHBIInitialized()) {
			HBI_Destroy(m_hHBI);
			m_bIsHBIInitialized = false;
		}
	}


public:
	// CapturerHBIDlg.cpp からアクセスするためのゲッターメソッド
	const std::vector<uint16_t>& GetImageBuffer() const { return m_vecuiImageBuffer; }
	int GetImageWidth()  const { return m_iImageWidth; }
	int GetImageHeight() const { return m_iImageHeight; }

    bool Initialize() {  
		if (IsHBIInitialized()) {  
			std::wcout << L"HBI is already initialized.\n";
			return false;
		}  

		// 初期化処理  
		m_hHBI = HBI_Init(0);  

		if (!m_hHBI) { m_bIsHBIInitialized = false; }
		else         { m_bIsHBIInitialized = true; }
		return m_bIsHBIInitialized;
    }

	void SetCallbackFunction() { HBI_RegEventCallBackFun(m_hHBI, UserHBICallback, this); }

	bool ConnectJumbo(const char* pcFPDIP, unsigned short usFPDPORT, const char* pcPCIP, unsigned short usPCPORT) {
		if (!IsHBIInitialized()) { return false; }
		int iRet = HBI_ConnectDetectorJumbo(
			m_hHBI,
			const_cast<char*>(pcFPDIP),
			usFPDPORT,
			const_cast<char*>(pcPCIP),
			usPCPORT,
			0);
		if (iRet) {
			std::wcout << L"HBI_ConnectDetectorJumbo failed. iRet=" << iRet << L"\n";
			return false;
		}
		std::wcout << L"Connected to the device successfully.\n";
		return true;
	}

	bool IsConnected() {
		if (!IsHBIInitialized()) { return false; }
		int iRet = HBI_IsConnect(m_hHBI);
		if (!iRet) {
			std::wcout << L"Device is not connected.\n";
			return false;
		}
		return true;
	}

	bool GetFPDSerialNumber() {
		// パネルの情報を取得する
		if (!m_bIsHBIInitialized) { return false; }

		char cSerialNumber[14] = { 0 };

		int iRetSerial = HBI_GetFPDSerialNumber(m_hHBI, cSerialNumber);

		if (iRetSerial != 0) {
			std::wcout << L"HBI_GetFPDSerialNumber failed. iRet=" << iRetSerial << L"\n";
			return false;
		}
		std::wcout << L"FPD Serial Number: " << cSerialNumber << L"\n";
		return true;
	}

	std::string  GetFPDProductCode(){
		if (!m_bIsHBIInitialized) { return ""; }

		char pcProductCode[10]  = { 0 };
		int iRetProductCode     = HBI_GetHbiProductCode(m_hHBI, pcProductCode);
		if (iRetProductCode != 0) {
			std::wcout << L"HBI_GetFPDProductCode failed. iRet=" << iRetProductCode << L"\n";
			return "";
		}

		std::wcout << L"FPD Product Code: " << pcProductCode << L"\n";
		return std::string(pcProductCode);
	}

	bool GetSDKVersion() {
		// HBISDKApi.dll のバージョンを取得する
		if (!m_bIsHBIInitialized) { return false; }
		 
		char pcSDKVersion[128] = {0};
		int iRet = HBI_GetSDKVerion(m_hHBI, pcSDKVersion);

		if (iRet != 0) {
			std::wcout << L"HBI_GetHbiStatus failed. iRet=" << iRet << L"\n"; 
			return false;
		}
		std::wcout << L"SDK Version: " << pcSDKVersion << L"\n";
		return true;
	}

	bool GetCaptureParams() {
		if (!m_bIsHBIInitialized) { return false; }

		int iRet;

		unsigned int uiBinningType;
		iRet = HBI_GetBinning(m_hHBI, &uiBinningType);
		if (iRet!=0) { 
			std::wcout << L"HBI_GetPGALevel failed. iRet=" << iRet << L"\n"; 
			return false;
		}
		
		unsigned int uiZoomLeft, uiZoomTop, uiZoomWidth, uiZoomHeight;
		iRet =	HBI_GetCurZoomRect(m_hHBI, &uiZoomLeft, &uiZoomTop, &uiZoomWidth, &uiZoomHeight);
		if (iRet!=0) { 
			std::wcout << L"HBI_GetCurZoomRect failed. iRet=" << iRet << L"\n"; 
			return false;
		}

		int iExpMiliOut;
		iRet = HBI_GetSelfDumpingTime(m_hHBI, &iExpMiliOut);
		if (iRet!=0) { 
			std::wcout << L"HBI_GetSelfDumpingTime failed. iRet=" << iRet << L"\n"; 
			return false;
		}
		float fExpSec = static_cast<float>(iExpMiliOut) / 1000.0f;
		float ffps = 1.0f / static_cast<float>(fExpSec);

		std::wcout << L"Getting current capture parameters...\n";
		std::wcout << L"  Gain Type        : " << HBI_GetPGALevel(m_hHBI) << L"\n";
		std::wcout << L"  Exposure Time    : " << std::fixed << std::setprecision(2) << fExpSec << L" s\n";
		std::wcout << L"  Frame per Second : " << std::fixed << std::setprecision(2) << ffps << L" fps\n";
		std::wcout << L"  Binning Type     : " << uiBinningType << L"\n";
		std::wcout << L"  Zoom Rect        : " << L"(" << uiZoomLeft << L", " << uiZoomTop << L") , " << uiZoomWidth << L" x " << uiZoomHeight << L"\n";
		return true;
	}

	bool SetCaptureParams(
		int iGainType       = 0,
		int iExpMili        = 0,
		int iCaptureFrame   = 0,
		int iBinningType    = 0,
		int iOriginalWidth  = 0,
		int iOriginalHeight = 0,
		int iZoomLeft       = 0,
		int iZoomTop        = 0,
		int iZoomWidth      = 0,
		int iZoomHeight     = 0
	) {
		if (!IsHBIInitialized()) { return false; }
		// 取得フレーム数
		SetCaptureFrame(iCaptureFrame);

		// GainType
		int iRet = HBI_MSetPGALevel(m_hHBI, iGainType);
		if (iRet != 0) {
			std::wcout << L"Gain Type: " << iGainType << L"\n";
			std::wcout << L"HBI_MSetPGALevel failed. iRet=" << iRet << L"\n";
			return false;
		}
		// Binning
		iRet = HBI_MSetBinning(m_hHBI, iBinningType);
		if (iRet != 0) {
			std::wcout << L"Binning Type: " << iBinningType << L"\n";
			std::wcout << L"HBI_SetBinning failed. iRet=" << iRet << L"\n";
			return false;
		}

		// Exposure time (= 1/fps)
		iRet = HBI_SetSelfDumpingTime(m_hHBI, iExpMili);
		if (iRet != 0) {
			std::wcout << L"Exposure Time (ms): " << iExpMili << L"\n";
			std::wcout << L"HBI_MSetFpsOfTime failed. iRet=" << iRet << L"\n";
			return false;
		}

		/* ROI の設定
		* 横方向は設定ができない。
		* 3030zはデュアル読出しのため、中央から等間隔にオフセットする必要がある。そのため縦方向のサイズは偶数である必要がある。
		* 2520zはシングル読出しのため、縦方向のオフセットは自由にできる。
		* uleft, utop, uright, ubottom = 0 の時はフルサイズとなる
		*/
		if (iZoomHeight != 0) {
			m_zoomRect.utop = iZoomTop;
			m_zoomRect.ubottom = iZoomTop + iZoomHeight - 1;
		}
		else {
			m_zoomRect.utop = 0;
			m_zoomRect.ubottom = iOriginalHeight - 1;
		}
		if (iZoomWidth != 0) {
			m_zoomRect.uleft = iZoomLeft;
			m_zoomRect.uright = iZoomLeft + iZoomWidth -1;
		}
		else {
			m_zoomRect.uleft = 0;
			m_zoomRect.uright = iOriginalWidth - 1;
		}
		iRet = HBI_MSetZoomModeRect(m_hHBI, m_zoomRect);
		if (iRet != 0) {
			std::wcout << L"HBI_MSetZoomModeRect failed. iRet=" << iRet << L"\n";
			return false;
		}
		return true;
	}

	void AllocateImageBuffer(int iCaptureFrame) {
		// 画像バッファを設定する
		m_iFrame = 0;
		m_vecuiImageBuffer.resize(static_cast<size_t>(m_iImageWidth) * static_cast<size_t>(m_iImageHeight) * static_cast<size_t>(iCaptureFrame));
	}

	bool UpdateImageProperties() {
		// 画像のサイズを取得する
		if (!IsHBIInitialized()) { return false; }
		IMAGE_PROPERTY   imgProp;
		int iRet = HBI_GetImageProperty(m_hHBI, &imgProp);
		m_iImageWidth = imgProp.nwidth;
		m_iImageHeight = imgProp.nheight;
		std::wcout << L"Image Properties:" << m_iImageWidth << L" x " << m_iImageHeight << std::endl;

		if (iRet != 0) {
			std::wcout << L"HBI_GetImageProperty failed. iRet=" << iRet << L"\n";
			return false;
		}
		return true;
	}

	// 画像取得開始
	bool StartCapture() {
		m_aqcMode.eAqccmd = LIVE_ACQ_DEFAULT_TYPE; // ライブキャプチャ用のACQMODE

		if (!IsHBIInitialized()) { return false; }
		m_iFrame = 0;
		m_bIsCapturing = true; // 画像取得中フラグを立てる
		HBI_LiveAcquisition(m_hHBI, m_aqcMode);
		std::wcout << L"Capture started...\n";
		return true;
	}

	// 画像取得停止
	bool StopCapture() {
		if (!IsHBIInitialized()) { return false; }
		HBI_StopAcquisition(m_hHBI);
		m_bIsCapturing = false; // 画像取得中フラグを下げる
		std::wcout << L"Capture stopped.\n";
		return true;
	}

	// 画像取得中か判断
	bool IsCapturing() const { return m_bIsCapturing; }

	bool ReleaseDevice() {
		if (IsCapturing()) {
			std::wcout << L"Stopping capture before release...\n";
			StopCapture();
		}
		if (IsHBIInitialized()) {
			HBI_Destroy(m_hHBI);
			m_hHBI = nullptr;
			m_bIsHBIInitialized = false;
		}
		return true;
	}

private:

	void SetCaptureFrame(int iCaptureFrame) { m_iCaptureFrame = iCaptureFrame; }

	bool IsHBIInitialized() const { return m_bIsHBIInitialized; }  

	// 取得した画像をバッファに保存する関数
	void SaveImageBuffer(void* pvParam1){
		if (m_iFrame >= m_iCaptureFrame) {
			m_bIsCapturing = false; // 指定枚数撮影したので保存はしない
			return; 
		}

		// 取得した画像をバッファに保存するため、フレーム数分オフセットする
		const size_t kiFramePixelCount = static_cast<size_t>(m_iImageWidth) * static_cast<size_t>(m_iImageHeight);
		const size_t kiOffset = static_cast<size_t>(m_iFrame) * kiFramePixelCount;

		// IMAGE_DATA_ST構造体内のdatabuffに画像データがあるので、それをバッファにコピーする
		memcpy_s(
			m_vecuiImageBuffer.data() + kiOffset,
			m_vecuiImageBuffer.size() * sizeof(uint16_t) - kiOffset * sizeof(uint16_t), // バッファの残りサイズを計算
			static_cast<IMAGE_DATA_ST*>(pvParam1)->databuff,
			kiFramePixelCount * sizeof(uint16_t)
		);
		// 保存に成功したらフレーム数をカウントする
		std::wcout << L"    Received multiple image data, frame id: " << m_iFrame << std::endl;
		m_iFrame++;

	}

	// ユーザーコールバック関数 (SDK に渡す関数ポインタ)
	static int UserHBICallback(
		void *pContext,
		int ifpdId,
		unsigned char eventId,
		void* peventParam1,
		int ievantParam2, // image data or detector status
		int ievantParam3, // frame rate or status
		int ievantParam4  // pcie event or Reserved or extended
	)
	{
 		CHBIDeviceCtrl* self = static_cast<CHBIDeviceCtrl*>(pContext);
		if (!self) { return 0; }
		return self->OnHBICallback(
			ifpdId,
			eventId,
			peventParam1,
			ievantParam2,
			ievantParam3,
			ievantParam4
		);
	}

	// インスタンス側の実処理
	int OnHBICallback(
		int ifpdId,
		unsigned char eventId,
		void* peventParam1,
		int ievantParam2,
		int ievantParam3,
		int ievantParam4
	)
	{
		(void)ifpdId;
		(void)ievantParam2;
		(void)ievantParam3;
		(void)ievantParam4;
		if (eventId == ECALLBACK_TYPE_MULTIPLE_IMAGE){
			SaveImageBuffer(peventParam1);
		}
		if (eventId == ECALLBACK_TYPE_SET_CFG_OK) {
			std::wcout << L"Set parameters successfully." << std::endl;
		}
		if (eventId == ECALLBACK_TYPE_PACKET_MISS) {
			std::wcout << L"Packet lost." << std::endl;
		}
		if (eventId == ECALLBACK_TYPE_BUFFER_WARNING) {
			std::wcout << L"Buffer warning." << std::endl;
		}
		return 1;
	}
};
