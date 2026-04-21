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
	void* m_hHBI;  
	bool  m_bIsHBIInitialized;  
	static std::vector<uint16_t> m_vechbiimagebuffer; // 最終の画像データを保存するバッファ

	static COMM_CFG       m_commCfg; // 通信の設定を保持する構造体  
	char* m_kcFPDIP = "192.168.10.40";
	char* m_kcPCIP  = "192.168.10.20";
	const unsigned short FPDPORT = 32897;
	const unsigned short PCPORT  = 32896;

	static FPD_AQC_MODE m_aqcMode;

	bool m_bcapturing; // 画像取得中かどうかを示すフラグ
	int m_iframeCounter; // 取得したフレーム数をカウントする変数

	IMAGE_DATA_ST m_imgData; // 画像取得時のデータを保存する構造体

	int m_itotalCaptureFrame; // 取得するフレームの総数


	// コンストラクタ  
	CHBIDeviceCtrl()  
		: m_hHBI(nullptr)  
		, m_bIsHBIInitialized(false)  
		, m_bcapturing(false)
		, m_iframeCounter(0)
	{}  

	bool IsHBIInitialized() const {  
		return m_bIsHBIInitialized;  
	}  

	bool Initialize() {  
		if (IsHBIInitialized()) {  
			return false; // すでに初期化されている場合は、再度初期化しない  
		}  

		// 初期化処理  
		m_hHBI = HBI_Init(0);  

		if (!m_hHBI) {  
			return false; // 初期化に失敗した場合は、false を返す  
		}  
		return true;  
	}  

	bool ConectJumbo() {  
		if (!IsHBIInitialized()) {  
			return false; // HBI が初期化されていない場合は、接続できない  
		}  
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
		HBI_LiveAcquisition(m_hHBI, m_aqcMode);
		m_bcapturing = true; // 画像取得中フラグを立てる
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
		return true;
	}

	// 取得した画像をバッファに保存する関数
	void SaveBuffer(void* pvParam1, int iframePixelCount){
		if (m_iframeCounter >= m_itotalCaptureFrame) {
			m_bcapturing = false; // 指定枚数撮影したので、画像取得中フラグを下げる
			return; 
		}

		size_t offset = m_iframeCounter * iframePixelCount;

		std::memcpy(&m_imgData, pvParam1, sizeof(IMAGE_DATA_ST));

		std::memcpy(
			m_vechbiimagebuffer.data() + offset,
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

		if (eventId == ECALLBACK_TYPE_MULTIPLE_IMAGE)
		{
			SaveBuffer(peventParam1, m_iframeCounter);
			std::wcout << L"    Received multiple image data, frame id: " << m_iframeCounter << std::endl;
		}
		return 1;
	}

	void SetCallBackFun() {
		HBI_RegEventCallBackFun(m_hHBI, usrHBICallback, nullptr);
	}


};