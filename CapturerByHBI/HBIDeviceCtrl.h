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
	FPD_MODE_LIST    m_modelist; // モードリストを保存する構造体
	FPD_MODE_DATA    m_modedata; // モードデータを保存する構造体

	void* m_hHBI;  
	bool  m_bIsHBIInitialized;  
	std::vector<uint16_t> m_vechbiImageBuffer; // 最終の画像データを保存するバッファ
	std::vector<uint16_t> m_arrImageBuffer; // 画像データを保存するバッファ

	bool m_bIsCapturing; // 画像取得中かどうかを示すフラグ
	int m_iFrame; // 取得したフレーム数をカウントする変数
	int m_iCaptureFrame; // 取得するフレームの総数

	int m_iimageWidth;
	int m_iimageHeight;

public:
	// コンストラクタ  
	CHBIDeviceCtrl()
		: m_hHBI(nullptr)
		, m_bIsHBIInitialized(false)
		, m_bIsCapturing(false)
		, m_iFrame(0)
		, m_iCaptureFrame(0)
		, m_iimageWidth(0)
		, m_iimageHeight(0)
	{
		std::memset(&m_modedata, 0, sizeof(m_modedata));
		std::memset(&m_modelist, 0, sizeof(m_modelist));
		std::memset(&m_zoomRect, 0, sizeof(m_zoomRect));
	}

	// デストラクタ
	~CHBIDeviceCtrl()
	{
		if (IsCapturering()) {
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
	std::vector<uint16_t> GetvecHBIimagebuffer() const { return m_vechbiImageBuffer; }
	int GetImageWidth()                          const { return m_iimageWidth; }
	int GetImageHeight()                         const { return m_iimageHeight; }

    bool Initialize() {  
		if (IsHBIInitialized()) {  
			std::wcout << L"HBI is already initialized.\n";
			return false;
		}  

		// 初期化処理  
		m_hHBI = HBI_Init(0);  

		if (!m_hHBI) {  
			m_bIsHBIInitialized = false;
		}else {
			m_bIsHBIInitialized = true;
		}
		return m_bIsHBIInitialized;
    }

	void SetCallBackFun() {
		HBI_RegEventCallBackFun(m_hHBI, usrHBICallback, this);
	}

	bool ConectJumbo(char* pcFPDIP, unsigned short usFPDPORT, char* pcPCIP, unsigned short usPCPORT) {
		if (!IsHBIInitialized()) { return false; }
		int iRet = HBI_ConnectDetectorJumbo(m_hHBI, pcFPDIP, usFPDPORT, pcPCIP, usPCPORT, 0);
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
		if (!m_bIsHBIInitialized) { return false; }

		char cProductCode[10]  = { 0 };
		int iRetProductCode    = HBI_GetHbiProductCode(m_hHBI, cProductCode);
		if (iRetProductCode != 0) {
			std::wcout << L"HBI_GetFPDProductCode failed. iRet=" << iRetProductCode << L"\n";
			return "";
		}

		std::wcout << L"FPD Product Code: "  << cProductCode << L"\n";
		return std::string(cProductCode);
	}

	bool GetSDKVersion() {
		// HBISDKApi.dll のバージョンを取得する
		if (!m_bIsHBIInitialized) { return false; }
		 
		char csdkVer[128] = {0};
		int iRet = HBI_GetSDKVerion(m_hHBI, csdkVer);

		if (iRet != 0) {
			std::wcout << L"HBI_GetHbiStatus failed. iRet=" << iRet << L"\n"; 
			return false;
		}
		std::wcout << L"SDK Version: " << csdkVer << L"\n";
		return true;
	}

	bool GetFPDProductCode(std::string& outProductCode) {
		outProductCode.clear();
		if (!m_bIsHBIInitialized) {
			std::wcout << L"HBI is not initialized.\n";
			return false;
		}

		char cproductcode[32] = { 0 };
		const int iRet = HBI_GetHbiProductCode(m_hHBI, cproductcode);
		if (iRet != 0) {
			std::wcout << L"HBI_GetFPDProductCode failed. iRet=" << iRet << L"\n";
			return false;
		}

		outProductCode = cproductcode;
		outProductCode.erase(
			std::remove_if(outProductCode.begin(), outProductCode.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }),
			outProductCode.end()
		);

		std::wcout << L"FPD Product Code: " << outProductCode.c_str() << L"\n";
		return !outProductCode.empty();
	}

	bool GetCaptureParams() {
		if (!m_bIsHBIInitialized) { return false; }

		int iRet;

		unsigned int uiBinningTypeOut;
		iRet = HBI_GetBinning(m_hHBI, &uiBinningTypeOut);
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
		std::wcout << L"  Binning Type     : " << uiBinningTypeOut << L"\n";
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
		* 横方向は設定ができない。縦方向は中心から等間隔にのみ広げられる。
		* uleft, utop, uright, ubottom = 0 の時はフルサイズとなる
		*/
		m_zoomRect.uleft = 0;
		m_zoomRect.uright = iOriginalWidth - 1;
		if (iZoomHeight != 0) {
			m_zoomRect.utop = iZoomTop;
			m_zoomRect.ubottom = iZoomTop + iZoomHeight - 1;
		}
		else {
			m_zoomRect.utop = 0;
			m_zoomRect.ubottom = iOriginalHeight - 1;
		}
		std::wcout << L"Zoom Rect set to: (" << m_zoomRect.uleft << L", " << m_zoomRect.utop << L") , (" << m_zoomRect.uright << L", " << m_zoomRect.ubottom << L")\n";
		iRet = HBI_MSetZoomModeRect(m_hHBI, m_zoomRect);
		if (iRet != 0) {
			std::wcout << L"HBI_MSetZoomModeRect failed. iRet=" << iRet << L"\n";
			return false;
		}
		return true;
	}

	void AllocateImageBuffer(int iCaptureFrame) {
		// 画像バッファを設定する
		m_vechbiImageBuffer.resize(static_cast<size_t>(m_iimageWidth) * static_cast<size_t>(m_iimageHeight) * static_cast<size_t>(iCaptureFrame));
	}

	bool UpdateProperties() {
		// 画像のサイズを取得する
		if (!IsHBIInitialized()) { return false; }
		IMAGE_PROPERTY   imgProp;
		int iRet = HBI_GetImageProperty(m_hHBI, &imgProp);
		m_iimageWidth = imgProp.nwidth;
		m_iimageHeight = imgProp.nheight;
		std::wcout << L"Image Properties:" << m_iimageWidth << L" x " << m_iimageHeight << std::endl;

		if (iRet != 0) {
			std::wcout << L"HBI_GetImageProperty failed. iRet=" << iRet << L"\n";
			return false;
		}
		return true;
	}

	// 画像取得開始
	bool Capture() {
		m_aqcMode.eAqccmd = LIVE_ACQ_DEFAULT_TYPE; // ライブキャプチャ用のACQMODE

		if (!IsHBIInitialized()) { return false; }
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
	bool IsCapturering() const { return m_bIsCapturing; }

	bool Release() {
		if (IsCapturering()) {
			std::wcout << L"Stopping capture before release...\n";
			StopCapture();
		}
		HBI_Destroy(m_hHBI);
		return true;
	}

private:

	void SetCaptureFrame(int iCaptureFrame) { m_iCaptureFrame = iCaptureFrame; }

	bool IsHBIInitialized() const { return m_bIsHBIInitialized; }  

	// 取得した画像をバッファに保存する関数
	void SaveBuffer(void* pvParam1){
		if (m_iFrame >= m_iCaptureFrame) {
			m_bIsCapturing = false; // 指定枚数撮影したので保存はしない
			return; 
		}
		std::wcout << L"    Received multiple image data, frame id: " << m_iFrame << std::endl;

		int iframePixelCount = static_cast<size_t>(m_iimageWidth) * static_cast<size_t>(m_iimageHeight);

		size_t offset = m_iFrame * iframePixelCount;
		const size_t uiFrameOffset = static_cast<size_t>(m_iFrame) * iframePixelCount;

		memcpy_s(
			&m_imgData,
			sizeof(m_imgData),
			pvParam1,
			sizeof(IMAGE_DATA_ST));

		memcpy_s(
			m_vechbiImageBuffer.data() + offset,
			m_vechbiImageBuffer.size() * sizeof(uint16_t) - offset * sizeof(uint16_t), // バッファの残りサイズを計算
			m_imgData.databuff,
			iframePixelCount * sizeof(uint16_t)
		);
	}

	// ユーザーコールバック関数 (SDK に渡す関数ポインタ)
	static int usrHBICallback(
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
		// printf("              Event ID: 0x%02X, Param2: %d\n", eventId, ievantParam2);
		if (eventId == ECALLBACK_TYPE_MULTIPLE_IMAGE){
			SaveBuffer(peventParam1);
			// std::wcout << L"           ievantParam2 (image data or detector status)      : " << ievantParam2 << L"\n";
			std::wcout << L"           ievantParam3 (frame rate or status)               : " << ievantParam3 << L"\n";
			// std::wcout << L"           ievantParam4 (pcie event or Reserved or extended) : " << ievantParam4 << L"\n";
			// std::wcout << L"    Received multiple image data, frame id: " << m_iFrame << std::endl;
			m_iFrame++;
		}
		return 1;
	}
};
