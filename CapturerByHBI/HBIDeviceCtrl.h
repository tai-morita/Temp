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
	static IMAGE_PROPERTY m_imgProp;  
	static COMM_CFG       m_commCfg; // 通信の設定を保持する構造体  
	static FPD_AQC_MODE m_aqcMode;
	static IMAGE_DATA_ST m_imgData; // 画像取得時のデータを保存する構造体

	void* m_hHBI;  
	bool  m_bIsHBIInitialized;  
	static std::vector<uint16_t> m_vechbiimagebuffer; // 最終の画像データを保存するバッファ

	char* m_kcFPDIP = "192.168.10.40";
	char* m_kcPCIP  = "192.168.10.20";
	const unsigned short FPDPORT = 32897;
	const unsigned short PCPORT  = 32896;

	bool m_bcapturing; // 画像取得中かどうかを示すフラグ
	int m_iframeCounter; // 取得したフレーム数をカウントする変数

	const int kiFRAMECOUNT = 100;
	const int kiGAINLEVEL = 2; // 1.2PC
	const int kiEXPTIME_milli = 33; // 33ms
	const int kiEXPTIME_micro = 333; // 333us -> ���v��33.333ms = 1/30s: 30fps

	int m_itotalCaptureFrame = kiFRAMECOUNT; // 取得するフレームの総数
	int m_kiGAINLEVEL = kiGAINLEVEL; // 1.2PC
	int m_kiEXPTIME_milli = kiEXPTIME_milli; // 33ms
	int m_kiEXPTIME_micro = kiEXPTIME_micro; // 333us -> ���v��33.333ms = 1/30s: 30fps

	// コンストラクタ  
	CHBIDeviceCtrl()  
		: m_hHBI(nullptr)  
		, m_bIsHBIInitialized(false)  
		, m_bcapturing(false)
		, m_iframeCounter(0)
	{}  

	bool UpdateProperties() {
		if (!IsHBIInitialized()) {
			std::wcout << L"HBI is not initialized.\n";
			return false;
		}

		int ret = HBI_GetImageProperty(m_hHBI, &m_imgProp);
		if (ret != 0) {
			std::wcout << L"HBI_GetImageProperty failed. ret=" << ret << L"\n";
			return false;
		}

		// Keep property view class in sync with the latest device state.
		HBIDeviceProperties::m_imgProp = m_imgProp;
		return true;
	}

	void SetCaptureParams(int iGainType, int iExpMili, int iExpMicro) {
		if (!IsHBIInitialized()) {
			std::wcout << L"HBI is not initialized.\n";
			return;
		}
		int ret = HBI_MSetPGALevel(m_hHBI, iGainType);
		if (ret != 0) {
			std::wcout << L"HBI_MSetPGALevel failed. ret=" << ret << L"\n";
			return;
		}
		ret = HBI_MSetFpsOfTime(m_hHBI, iExpMili, iExpMicro);
		if (ret != 0) {
			std::wcout << L"HBI_MSetFpsOfTime failed. ret=" << ret << L"\n";
			return;
		}
		double dExposureMs = static_cast<double>(iExpMili) + static_cast<double>(iExpMicro) / 1000.0;
		double dFps = dExposureMs > 0.0 ? 1000.0 / dExposureMs : 0.0;
		std::wcout << L"Capture parameters set: GainType =" << iGainType << L"\n";
		std::wcout << L"Exposure Time =" << dExposureMs << L" ms\n";
		std::wcout << L"FPS =" << dFps << L"\n";
		return;
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

	bool ConectJumbo() {  
		if (!IsHBIInitialized()) {  
			return false; // HBI が初期化されていない場合は、接続できない  
		}
		std::wcout << L"Connecting to the device with the following settings:\n";
		std::wcout << L"  FPD IP: " << m_kcFPDIP << L"\n";
		std::wcout << L"  FPD Port: " << FPDPORT << L"\n";
		std::wcout << L"  PC IP: " << m_kcPCIP << L"\n";
		std::wcout << L"  PC Port: " << PCPORT << L"\n";
		int ret = HBI_ConnectDetectorJumbo(  
			m_hHBI,  
			m_kcFPDIP, // FPD IP  
			FPDPORT,   // FPD port  
			m_kcPCIP,  // PC IP  
			PCPORT,    // PC port  
			0  
		);  
		return ret == 0; // 接続に成功した場合は true を返す  
	}

	static std::vector<uint16_t> SetImageBuffer(int iframeCount) {
		// 画像バッファを設定する
		/*
		if (!IsHBIInitialized()) {
			return {}; // HBI が初期化されていない場合は、空のベクターを返す
		}
		*/
		int iImageWidth     = m_imgProp.nwidth;
		int iImageHeight    = m_imgProp.nheight;
		int totalPixelCount = static_cast<size_t>(iImageWidth) * static_cast<size_t>(iImageHeight) * static_cast<size_t>(iframeCount);
		m_vechbiimagebuffer.resize(totalPixelCount); // バッファのサイズをフレーム数に応じて調整
		return m_vechbiimagebuffer; // 画像バッファを返す
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

		int iframePixelCount = static_cast<size_t>(m_imgProp.nwidth) * static_cast<size_t>(m_imgProp.nheight);

		size_t offset = m_iframeCounter * iframePixelCount;

		std::memcpy(&m_imgData, pvParam1, sizeof(IMAGE_DATA_ST));

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
		//printf("              Event ID: 0x%02X, Param2: %d\n", eventId, ievantParam2);
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

	bool Release() {
		if (IsCapturering()) {
			std::wcout << L"Stopping capture before release...\n";
			StopCapture();
		}
		HBI_Destroy(m_hHBI);
		return true;
	}

};