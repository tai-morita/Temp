using namespace std;

#include <vector>
#include <string>
#include <iostream>


#include "HbiFpd.h"  
#include "HbiType.h"  
#include "HbiError.h"  

#include "./HBIDeviceProperties.h"

// HBI で通信するデバイスの動作を制御するクラス  
class CHBIDeviceCtrl  
{  
public:
	IMAGE_PROPERTY m_imgProp;  
	COMM_CFG       m_commCfg; // 通信の設定を保持する構造体  
	FPD_AQC_MODE m_aqcMode;
	IMAGE_DATA_ST m_imgData; // 画像取得時のデータを保存する構造体
	CMOS_ZOOM_RECT m_zoomRect; // ズームモードの矩形を保存する構造体
	FPD_MODE_LIST m_modelist; // モードリストを保存する構造体
	FPD_MODE_DATA m_modedata; // モードデータを保存する構造体

	void* m_hHBI;  
	bool  m_bIsHBIInitialized;  
	std::vector<uint16_t> m_vechbiimagebuffer; // 最終の画像データを保存するバッファ

	bool m_bcapturing; // 画像取得中かどうかを示すフラグ
	int m_iframeCounter; // 取得したフレーム数をカウントする変数
	int m_itotalCaptureFrame; // 取得するフレームの総数

	int m_iimageWidth;
	int m_iimageHeight;

	// コンストラクタ  
	CHBIDeviceCtrl()  
		: m_hHBI(nullptr)  
		, m_bIsHBIInitialized(false)  
		, m_bcapturing(false)
		, m_iframeCounter(0)
		, m_itotalCaptureFrame(0)
		, m_iimageWidth(2560)
		, m_iimageHeight(2048)
	{}  

	bool UpdateProperties() {
		if (!IsHBIInitialized()) {
			std::wcout << L"HBI is not initialized.\n";
			return false;
		}
		int ret = HBI_GetImageProperty(m_hHBI, &m_imgProp);
		m_iimageWidth = m_imgProp.nwidth;
		m_iimageHeight = m_imgProp.nheight;
		std::wcout << L"Image Properties:" << m_iimageWidth << L" x " << m_iimageHeight << std::endl;

		if (ret != 0) {
			std::wcout << L"HBI_GetImageProperty failed. ret=" << ret << L"\n";
			return false;
		}
		return true;
	}

	void SetCaptureParams(
		int iGainType, 
		int iExpMili, 
		int iExpMicro, 
		int itotalCaptureFrame,
		int iBinningType,
		int iZoomLeft=0,
		int iZoomTop=0,
		int iZoomWidth=0,
		int iZoomHeight=0
	) {
		std::wcout << L"Setting capture parameters...\n";
		std::wcout << L"  GainType: " << iGainType << L"\n";
		std::wcout << L"  Binning Type: " << iBinningType << L"\n";
		std::wcout << L"  Zoom Rect: Left=" << iZoomLeft << L", Top=" << iZoomTop << L", Width=" << iZoomWidth << L", Height=" << iZoomHeight << L"\n";
		std::wcout << L"  Exposure Time: " << iExpMili << L" ms + " << iExpMicro << L" us\n";
		if (!IsHBIInitialized()) {
			std::wcout << L"HBI is not initialized.\n";
			return;
		}
		int iret;
		iret = HBI_MSetPGALevel(m_hHBI, iGainType);
		if (iret != 0) {
			std::wcout << L"HBI_MSetPGALevel failed. iret=" << iret << L"\n";
			return;
		}
		iret = HBI_MSetBinning(m_hHBI, iBinningType);
		if (iret != 0) {
			std::wcout << L"HBI_SetBinning failed. iret=" << iret << L"\n";
			return;
		}

		m_zoomRect.uleft   = iZoomLeft;
		m_zoomRect.utop    = iZoomTop;
		m_zoomRect.uright  = m_zoomRect.uleft + iZoomWidth-1;
		m_zoomRect.ubottom = m_zoomRect.utop + iZoomHeight-1;

		std::wcout << L"  Zoom Rect Set To: Left=" << m_zoomRect.uleft << L", Top=" << m_zoomRect.utop << L", Right=" << m_zoomRect.uright << L", Bottom=" << m_zoomRect.ubottom << L"\n";
		iret = HBI_MSetZoomModeRect(m_hHBI, m_zoomRect); // ズームモードの矩形を設定（例: 画像全体）
		if (iret != 0) {
			std::wcout << L"HBI_MSetZoomModeRect failed. iret=" << iret << L"\n";
			return;
		}
		unsigned int uiZoomLeft, uiZoomTop, uiZoomWidth, uiZoomHeight;
		HBI_GetCurZoomRect(m_hHBI, &uiZoomLeft, &uiZoomTop, &uiZoomWidth, &uiZoomHeight);
		std::wcout << L"  Zoom Rect    : " << uiZoomLeft << L", " << uiZoomTop << L", " << uiZoomWidth << L", " << uiZoomHeight << L"\n";
		iret = HBI_MSetFpsOfTime(m_hHBI, iExpMili, iExpMicro);
		if (iret != 0) {
			std::wcout << L"HBI_MSetFpsOfTime failed. iret=" << iret << L"\n";
			return;
		}

		m_itotalCaptureFrame = itotalCaptureFrame; // 取得するフレームの総数を設定
		double dExposureMs = static_cast<double>(iExpMili) + static_cast<double>(iExpMicro) / 1000.0;
		double dFps = dExposureMs > 0.0 ? 1000.0 / dExposureMs : 0.0;
		/*
		std::wcout << L"Capture parameters set:\n  Capture Frames = " << itotalCaptureFrame << L"\n";
		std::wcout << L"  GainType = " << iGainType << L"\n";
		std::wcout << L"  Exposure Time =" << dExposureMs << L" ms\n";
		*/
		std::wcout << L"  FPS =" << dFps << L"\n";
		return;
	}

	void GetCaptureParams() {
		std::wcout << L"Getting current capture parameters...\n";
		std::wcout << L"  Gain Type    : " << HBI_GetPGALevel(m_hHBI) << L"\n";
		std::wcout << L"  Exposure Time: " << L" " << L" ms\n";
		unsigned int uiBinningTypeOut;
		HBI_GetBinning(m_hHBI, &uiBinningTypeOut);
		std::wcout << L"  Binning Type : " << uiBinningTypeOut << L"\n";
		unsigned int uiZoomLeft, uiZoomTop, uiZoomWidth, uiZoomHeight;
		HBI_GetCurZoomRect(m_hHBI, &uiZoomLeft, &uiZoomTop, &uiZoomWidth, &uiZoomHeight);
		std::wcout << L"  Zoom Rect    : " << uiZoomLeft << L", " << uiZoomTop << L", " << uiZoomWidth << L", " << uiZoomHeight << L"\n";
	}

	void GetFPDStatus() {
		// パネルの情報を取得する
		if (m_bIsHBIInitialized) {
			char cserialnumber[14] = { 0 };
			int iret;
			iret = HBI_GetFPDSerialNumber(m_hHBI, cserialnumber); // 配列のポインタを渡す
			if (iret != 0) {
				std::wcout << L"HBI_GetFPDSerialNumber failed. iret=" << iret << L"\n";
			}
			std::wcout << L"FPD Serial Number: " << cserialnumber << L"\n";
		}
	}

	bool IsHBIInitialized() const {  
		return m_bIsHBIInitialized;  
	}  

    bool Initialize() {  
		if (IsHBIInitialized()) {  
			std::wcout << L"HBI is already initialized.\n";
			return false; // すでに初期化されている場合は、再度初期化しない  
		}  

		// 初期化処理  
		m_hHBI = HBI_Init(0);  

		if (!m_hHBI) {  
			m_bIsHBIInitialized = false;
		}
		else {
			m_bIsHBIInitialized = true;
		}
		return m_bIsHBIInitialized;
    }

	bool ConectJumbo(char* pcFPDIP, unsigned short usFPDPORT, char* pcPCIP, unsigned short usPCPORT) {  
		if (!IsHBIInitialized()) {  
			return false; // HBI が初期化されていない場合は、接続できない  
		}
		std::wcout << L"Connecting to the device with the following settings:\n";
		std::wcout << L"  FPD IP: " << pcFPDIP << L"\n";
		std::wcout << L"  FPD Port: " << usFPDPORT << L"\n";
		std::wcout << L"  PC IP: " << pcPCIP << L"\n";
		std::wcout << L"  PC Port: " << usPCPORT << L"\n";
		int ret = HBI_ConnectDetectorJumbo(  
			m_hHBI,  
			pcFPDIP, // FPD IP  
			usFPDPORT,   // FPD port  
			pcPCIP,  // PC IP  
			usPCPORT,    // PC port  
			0  
		);  

		return ret == 0; // 接続に成功した場合は true を返す  
	}

	void AllocateImageBuffer(int iframeCount) {
		// 画像バッファを設定する
		int totalPixelCount = static_cast<size_t>(m_iimageWidth) * static_cast<size_t>(m_iimageHeight) * static_cast<size_t>(iframeCount);
		m_vechbiimagebuffer.resize(totalPixelCount); // バッファのサイズをフレーム数に応じて調整
		return;
	}

	// 画像取得開始
	bool Capture() {
		if (!IsHBIInitialized()) {
			return false; // HBI が初期化されていない場合は、キャプチャできない  
		}
		m_aqcMode.eAqccmd = LIVE_ACQ_DEFAULT_TYPE;
		m_bcapturing = true; // 画像取得中フラグを立てる
		HBI_LiveAcquisition(m_hHBI, m_aqcMode);
		std::wcout << L"Capture started...\n";
		return true;
	}

	// 画像取得中か判断
	bool IsCapturering() const {
		return m_bcapturing;
	}

	// 画像取得停止
	bool StopCapture() {
		if (!IsHBIInitialized()) {
			return false; // HBI が初期化されていない場合は、キャプチャを停止できない  
		}
		HBI_StopAcquisition(m_hHBI);
		m_bcapturing = false; // 画像取得中フラグを下げる
		std::wcout << L"Capture stopped.\n";
		return true;
	}

	// 取得した画像をバッファに保存する関数
	void SaveBuffer(void* pvParam1){
		if (m_iframeCounter >= m_itotalCaptureFrame) {
			m_bcapturing = false; // 指定枚数撮影したので、画像取得中フラグを下げる
			return; 
		}
		std::wcout << L"    Received multiple image data, frame id: " << m_iframeCounter << std::endl;

		int iframePixelCount = static_cast<size_t>(m_iimageWidth) * static_cast<size_t>(m_iimageHeight);

		size_t offset = m_iframeCounter * iframePixelCount;

		std::memcpy(
			&m_imgData,
			pvParam1,
			sizeof(IMAGE_DATA_ST));

		memcpy_s(
			m_vechbiimagebuffer.data() + offset,
			m_vechbiimagebuffer.size() * sizeof(uint16_t) - offset * sizeof(uint16_t), // バッファの残りサイズを計算
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
		int ievantParam2,
		int ievantParam3,
		int ievantParam4
	)
	{
 		CHBIDeviceCtrl* self = static_cast<CHBIDeviceCtrl*>(pContext);
		if (!self) {
			return 0;
		}
		return self->OnHBICallback(
			ifpdId,
			eventId,
			peventParam1,
			ievantParam2,
			ievantParam3,
			ievantParam4
		);
	}

	bool Release() {
		if (IsCapturering()) {
			std::wcout << L"Stopping capture before release...\n";
			StopCapture();
		}
		HBI_Destroy(m_hHBI);
		return true;
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
		if (eventId == ECALLBACK_TYPE_MULTIPLE_IMAGE)
		{
			SaveBuffer(peventParam1);
			// std::wcout << L"    Received multiple image data, frame id: " << m_iframeCounter << std::endl;
			m_iframeCounter++;
		}
		return 1;
	}

	void SetCallBackFun() {
		HBI_RegEventCallBackFun(m_hHBI, usrHBICallback, this);
	}
};