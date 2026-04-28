using namespace std;

#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <iomanip>

#include "HbiFpd.h"  
#include "HbiType.h"  
#include "HbiError.h"  

#include "./HBIDeviceProperties.h"

// HBI で通信するデバイスの動作を制御するクラス  
class CHBIDeviceCtrl  
{ 
private:
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
	int m_iCaptureFrame; // 取得するフレームの総数

	int m_iimageWidth;
	int m_iimageHeight;


public:
	// コンストラクタ  
	CHBIDeviceCtrl()  
		: m_hHBI(nullptr)  
		, m_bIsHBIInitialized(false)  
		, m_bcapturing(false)
		, m_iframeCounter(0)
		, m_iCaptureFrame(0)
		, m_iimageWidth(3072)
		, m_iimageHeight(3072)
	{
		std::memset(&m_modedata, 0, sizeof(m_modedata));
		std::memset(&m_modelist, 0, sizeof(m_modelist));
		std::memset(&m_zoomRect, 0, sizeof(m_zoomRect));
	}

	// CapturerHBIDlg.cpp からアクセスするためのゲッターメソッド
	std::vector<uint16_t> GetvecHBIimagebuffer() const { return m_vechbiimagebuffer; }
	int GetImageWidth()                          const { return m_iimageWidth; }
	int GetImageHeight()                         const { return m_iimageHeight; }

	bool UpdateProperties() {
		if (!IsHBIInitialized()) {
			std::wcout << L"HBI is not initialized.\n";
			return false;
		}
		int ret = HBI_GetImageProperty(m_hHBI, &m_imgProp);
		m_iimageWidth  = m_imgProp.nwidth;
		m_iimageHeight = m_imgProp.nheight;
		std::wcout << L"Image Properties:" << m_iimageWidth << L" x " << m_iimageHeight << std::endl;

		if (ret != 0) {
			std::wcout << L"HBI_GetImageProperty failed. ret=" << ret << L"\n";
			return false;
		}
		return true;
	}

	bool SetCaptureFrame(int iCaptureFrame) {
		m_iCaptureFrame = iCaptureFrame;
		return true;
	}

	void SetCaptureParams(
		int iGainType, 
		int iExpMili, 
		int iCaptureFrame,
		int iBinningType,
		int iZoomHeight=0
	) {
		if (!IsHBIInitialized()) {
			std::wcout << L"HBI is not initialized.\n";
			return;
		}
		int iret;
		// GainType
		iret = HBI_MSetPGALevel(m_hHBI, iGainType);
		if (iret != 0) {
			std::wcout << L"HBI_MSetPGALevel failed. iret=" << iret << L"\n";
			return;
		}
		// Binning
		iret = HBI_MSetBinning(m_hHBI, iBinningType);
		if (iret != 0) {
			std::wcout << L"HBI_SetBinning failed. iret=" << iret << L"\n";
			return;
		}
		// 取得フレーム数
		SetCaptureFrame(iCaptureFrame);

		/* ROI の設定
		* 横方向は設定ができない。縦方向は中心から等間隔にのみ広げられる。
		* uleft, utop, uright, ubottom = 0 の時はフルサイズとなる
		*/
		m_zoomRect.uleft   = 0;
		m_zoomRect.utop    = (m_iimageHeight - iZoomHeight) / 2; // 画像の中心から等間隔に広げる
		m_zoomRect.uright  = m_iimageWidth - 1;
		m_zoomRect.ubottom = m_iimageHeight - m_zoomRect.utop - 1;

		iret = HBI_MSetZoomModeRect(m_hHBI, m_zoomRect);
		if (iret != 0) {
			std::wcout << L"HBI_MSetZoomModeRect failed. iret=" << iret << L"\n";
			std::wcout << L"  Zoom Rect: " << m_zoomRect.uleft << ", " << m_zoomRect.utop << ", " << m_zoomRect.uright << ", " << m_zoomRect.ubottom << "\n";
			std::wcout << m_iimageHeight << L" x " << m_iimageWidth << L" image size, but zoom height is set to " << iZoomHeight << L".\n";
			return;
		}
		// Exposure time (= 1/fps)
		iret = HBI_SetSelfDumpingTime(m_hHBI, iExpMili);
		if (iret != 0) {
			std::wcout << L"HBI_MSetFpsOfTime failed. iret=" << iret << L"\n";
			return;
		}
		return;
	}

	void GetCaptureParams() {
		unsigned int uiBinningTypeOut;
		HBI_GetBinning(m_hHBI, &uiBinningTypeOut);
		unsigned int uiZoomLeft, uiZoomTop, uiZoomWidth, uiZoomHeight;
		HBI_GetCurZoomRect(m_hHBI, &uiZoomLeft, &uiZoomTop, &uiZoomWidth, &uiZoomHeight);
		int iExpMiliOut;
		HBI_GetSelfDumpingTime(m_hHBI, &iExpMiliOut);
		float fExpSec = static_cast<float>(iExpMiliOut) / 1000.0f;
		float ffps = 1.0f / static_cast<float>(fExpSec);

		std::wcout << L"Getting current capture parameters...\n";
		std::wcout << L"  Gain Type        : " << HBI_GetPGALevel(m_hHBI) << L"\n";
		std::wcout << L"  Exposure Time    : " << std::fixed << std::setprecision(2) << fExpSec << L" s\n";
		std::wcout << L"  Frame per Second : " << std::fixed << std::setprecision(2) << ffps << L" fps\n";
		std::wcout << L"  Binning Type     : " << uiBinningTypeOut << L"\n";
		std::wcout << L"  Zoom Rect        : " << L"(" <<  uiZoomLeft << L", " << uiZoomTop << L") , " << uiZoomWidth << L" x " << uiZoomHeight << L"\n";
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
			return false;
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
			return false;
		}
		std::wcout << L"Connecting to the device with the following settings:\n";
		std::wcout << L"  FPD IP   : " << pcFPDIP << L"\n";
		std::wcout << L"  FPD Port : " << usFPDPORT << L"\n";
		std::wcout << L"  PC IP    : " << pcPCIP << L"\n";
		std::wcout << L"  PC Port  : " << usPCPORT << L"\n";
		int ret = HBI_ConnectDetectorJumbo(  
			m_hHBI,  
			pcFPDIP,
			usFPDPORT,
			pcPCIP,
			usPCPORT,
			0  
		);
		if (ret == 0) {
			return true;
		}else{
			std::wcout << L"HBI_ConnectDetectorJumbo failed. ret=" << ret << L"\n";
			return false;
		}
	}

	void AllocateImageBuffer(int iCaptureFrame) {
		// 画像バッファを設定する
		m_vechbiimagebuffer.resize(static_cast<size_t>(m_iimageWidth) * static_cast<size_t>(m_iimageHeight) * static_cast<size_t>(iCaptureFrame));
		return;
	}

	// 画像取得開始
	bool Capture() {
		m_aqcMode.eAqccmd = LIVE_ACQ_DEFAULT_TYPE; // ライブキャプチャ用のACQMODE

		if (!IsHBIInitialized()) {
			return false;
		}
		m_bcapturing = true; // 画像取得中フラグを立てる
		HBI_LiveAcquisition(m_hHBI, m_aqcMode);
		std::wcout << L"Capture started...\n";
		return true;
	}

	// 画像取得中か判断
	bool IsCapturering() const { return m_bcapturing; }

	// 画像取得停止
	bool StopCapture() {
		if (!IsHBIInitialized()) {
			return false;
		}
		HBI_StopAcquisition(m_hHBI);
		m_bcapturing = false; // 画像取得中フラグを下げる
		std::wcout << L"Capture stopped.\n";
		return true;
	}

	// 取得した画像をバッファに保存する関数
	void SaveBuffer(void* pvParam1){
		if (m_iframeCounter >= m_iCaptureFrame) {
			m_bcapturing = false; // 指定枚数撮影したので保存はしない
			return; 
		}
		std::wcout << L"    Received multiple image data, frame id: " << m_iframeCounter << std::endl;

		int iframePixelCount = static_cast<size_t>(m_iimageWidth) * static_cast<size_t>(m_iimageHeight);

		size_t offset = m_iframeCounter * iframePixelCount;

		memcpy_s(
			&m_imgData,
			sizeof(m_imgData),
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
		int ievantParam2, // image data or detector status
		int ievantParam3, // frame rate or status
		int ievantParam4  // pcie event or Reserved or extended
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
			// std::wcout << L"           ievantParam2 (image data or detector status)      : " << ievantParam2 << L"\n";
			std::wcout << L"           ievantParam3 (frame rate or status)               : " << ievantParam3 << L"\n";
			// std::wcout << L"           ievantParam4 (pcie event or Reserved or extended) : " << ievantParam4 << L"\n";
			// std::wcout << L"    Received multiple image data, frame id: " << m_iframeCounter << std::endl;
			m_iframeCounter++;
		}
		return 1;
	}

	void SetCallBackFun() {
		HBI_RegEventCallBackFun(m_hHBI, usrHBICallback, this);
	}
};
