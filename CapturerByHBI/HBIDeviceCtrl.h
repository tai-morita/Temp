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

	void* m_hHBI;                             // HBISDK のハンドル。これで SDK の関数を呼び出す。
	bool  m_bIsHBIInitialized;                // 初期化されているか示すフラグ
	std::vector<uint16_t> m_vecuiImageBuffer; // 最終の画像データを保存するバッファ

	bool m_bIsCapturing; // 画像取得中かどうかを示すフラグ
	int m_iFrameCounter; // 取得したフレーム数をカウントする
	int m_iCaptureFrame; // 取得するフレームの総数

	int m_iImageWidth;   // 取得する画像サイズ(幅)
	int m_iImageHeight;  // 取得する画像サイズ(高さ)

public:
	// コンストラクタ  
	CHBIDeviceCtrl()
		: m_hHBI(nullptr)
		, m_bIsHBIInitialized(false)
		, m_bIsCapturing(false)
		, m_iFrameCounter(0)
		, m_iCaptureFrame(0)
		, m_iImageWidth(0)
		, m_iImageHeight(0)

	// デストラクタ
	~CHBIDeviceCtrl()
	{
		if (IsCapturing()) {
			std::cout << "Stopping capture before destruction...\n";
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
	const int GetImageWidth()  const { return m_iImageWidth; }
	const int GetImageHeight() const { return m_iImageHeight; }

	/**
	 * @brief SDK の関数を呼び出す前の初期化処理
	 */
    bool Initialize() {  
		if (IsHBIInitialized()) {  
			std::cout << "HBI is already initialized.\n";
			return false;
		}  

		m_hHBI = HBI_Init(0);  

		if (!m_hHBI) { m_bIsHBIInitialized = false; }
		else         { m_bIsHBIInitialized = true; }
		return m_bIsHBIInitialized;
    }

	/**
	 * @brief SDK のイベントコールバック関数を設定する
	 * イベントが起こった時、SDK が UsetHBICallBack を呼び出す。
	 */
	void SetCallbackFunction() { HBI_RegEventCallBackFun(m_hHBI, UserHBICallback, this); }

	/**
	 * @brief Jumbo Packet でデバイスに接続する
	 * @param pcFPDIP   デバイスの IP アドレス
	 * @param usFPDPORT デバイスのポート番号
	 * @param pcPCIP    PC の IP アドレス
	 * @param usPCPORT  PC のポート番号
	 * @return 接続に成功した場合は true、失敗した場合は false
	 * @note   HBI_ConnectDetectorJumbo の戻り値は 0 が成功、0 以外が失敗
	 */
	bool ConnectJumbo(const char* kpcFPDIP, const unsigned short kusFPDPORT, const char* kpcPCIP, const unsigned short kusPCPORT) {
		if (!IsHBIInitialized()) { return false; }
		int iRet = HBI_ConnectDetectorJumbo(
			m_hHBI,
			const_cast<char*>(kpcFPDIP),
			kusFPDPORT,
			const_cast<char*>(kpcPCIP),
			kusPCPORT,
			0);
		if (iRet) {
			std::cout << "HBI_ConnectDetectorJumbo failed. iRet=" << iRet << "\n";
			return false;
		}
		std::cout << "Connected to the device successfully.\n";
		return true;
	}

	/**
	 * @brief デバイスが接続されているか確認する
	 * @return 接続されている場合は true、接続されていない場合は false
	 * @note   HBI_IsConnect の戻り値は 1 が接続、0 が未接続
	 */
	bool IsConnected() {
		if (!IsHBIInitialized()) { return false; }
		int iRet = HBI_IsConnect(m_hHBI);
		if (!iRet) {
			std::cout << "Device is not connected.\n";
			return false;
		}
		std::cout << "Device is connected.\n";
		return true;
	}

	/**
	 * @brief デバイスのシリアル番号を取得する
	 * @return 取得に成功した場合はシリアル番号の文字列、失敗した場合は空文字列
	 * @note   HBI_GetFPDSerialNumber の戻り値は 0 が成功、0 以外が失敗
	 */
	std::string GetFPDSerialNumber() {
		// パネルの情報を取得する
		if (!m_bIsHBIInitialized) { return ""; }

		char cSerialNumber[14] = { 0 };

		int iRetSerial = HBI_GetFPDSerialNumber(m_hHBI, cSerialNumber);

		if (iRetSerial != 0) {
			std::cout << "HBI_GetFPDSerialNumber failed. iRet=" << iRetSerial << "\n";
			return "";
		}
		std::cout << "FPD Serial Number: " << cSerialNumber << "\n";
		return std::string(cSerialNumber);
	}

	/**
	 * @brief デバイスの製品コードを取得する
	 * @return 取得に成功した場合は製品コードの文字列、失敗した場合は空文字列
	 * @note   HBI_GetHbiProductCode の戻り値は 0 が成功、0 以外が失敗
	 */
	std::string  GetFPDProductCode(){
		if (!m_bIsHBIInitialized) { return ""; }

		char pcProductCode[10]  = { 0 };
		int iRetProductCode     = HBI_GetHbiProductCode(m_hHBI, pcProductCode);
		if (iRetProductCode != 0) {
			std::cout << "HBI_GetFPDProductCode failed. iRet=" << iRetProductCode << "\n";
			return "";
		}

		std::cout << "FPD Product Code: " << pcProductCode << "\n";
		return std::string(pcProductCode);
	}
	
	/**
	 * @brief SDK のバージョンを取得する
	 * @return 取得に成功した場合は SDK のバージョンの文字列、失敗した場合は空文字列
	 * @note   HBI_GetSDKVerion の戻り値は 0 が成功、0 以外が失敗
	 */
	std::string GetSDKVersion() {
		// HBISDKApi.dll のバージョンを取得する
		if (!m_bIsHBIInitialized) { return ""; }
		 
		char pcSDKVersion[128] = {0};
		int iRet = HBI_GetSDKVerion(m_hHBI, pcSDKVersion);

		if (iRet != 0) {
			std::cout << "HBI_GetSDKVersion failed. iRet=" << iRet << "\n"; 
			return "";
		}
		std::cout << "SDK Version: " << pcSDKVersion << "\n";
		return std::string(pcSDKVersion);
	}

	/**
	 * @brief  現在のキャプチャパラメータをコンソール上に出力する
	 * @return すべてのパラメータの取得に成功した場合は true、どこかで取得に失敗した場合は false
	 * @note   デバッグとして使用
	 */
	bool PrintCaptureParams() {
		if (!m_bIsHBIInitialized) { return false; }

		int iRet;

		// GainType: HBI_GetPGALevel の戻り値は GainType (1 ~ 11) が成功、失敗は -1
		int iGainType = HBI_GetPGALevel(m_hHBI);
		 if (!iGainType) { 
			std::cout << "HBI_GetPGALevel failed."<< "\n"; 
			return false;
		}

		// Binning: HBI_GetBinning の戻り値は 0が成功、失敗は 0以外
		// iBinningType の値は 1: 1x1, 2: 2x2, 3: 3x3, 4: 4x4
		int iBinningType;
		iRet = HBI_GetBinning(m_hHBI, &iBinningType);
		if (iRet!=0) { 
			std::cout << "HBI_GetBinning failed. iRet=" << iRet << "\n"; 
			return false;
		}
		
		// Capture 領域: HBI_GetCurZoomRect の戻り値は 0が成功、失敗は 0以外
		// iZoomLeft, iZoomTop, iZoomWidth, iZoomHeight はそれぞれ左上の座標と幅、高さを表し
		// それぞれの変数に値が格納される
		// 0 が成功、0 以外が失敗
		int iZoomLeft, iZoomTop, iZoomWidth, iZoomHeight;
		iRet =	HBI_GetCurZoomRect(m_hHBI, &iZoomLeft, &iZoomTop, &iZoomWidth, &iZoomHeight);
		if (iRet!=0) { 
			std::cout << "HBI_GetCurZoomRect failed. iRet=" << iRet << "\n"; 
			return false;
		}

		// Exposure time: HBI_GetSelfDumpingTime の戻り値は 0が成功、失敗は 0以外
		// imsExposureTime は Exposure time をミリ秒単位で表す
		// 0 が成功、0 以外が失敗
		int imsExposureTime;
		iRet = HBI_GetSelfDumpingTime(m_hHBI, &imsExposureTime);
		if (iRet!=0) { 
			std::cout << "HBI_GetSelfDumpingTime failed. iRet=" << iRet << "\n"; 
			return false;
		}
		// 出力用に fps を計算
		float fExpSec = static_cast<float>(imsExposureTime) / 1000.0f;
		float ffps = 1.0f / static_cast<float>(fExpSec);

		// 取得したパラメータをコンソール上に出力する
		std::cout << "Getting current capture parameters...\n";
		std::cout << "  Binning Type     : " << iBinningType << "\n";
		std::cout << "  Gain Type        : " << HBI_GetPGALevel(m_hHBI) << "\n";
		std::cout << "  Exposure Time (s): " << std::fixed << std::setprecision(2) << fExpSec << " s\n";
		std::cout << "  Frame per Second : " << std::fixed << std::setprecision(2) << ffps << " fps\n";
		std::cout << "  Capture Area     : " << 
		  "(Left, Top) = (" << iZoomLeft << ", " << iZoomTop << ") , Width x Height = " << iZoomWidth << " x " << iZoomHeight << "\n";

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
			std::cout << "Gain Type: " << iGainType << "\n";
			std::cout << "HBI_MSetPGALevel failed. iRet=" << iRet << "\n";
			return false;
		}
		// Binning
		iRet = HBI_MSetBinning(m_hHBI, iBinningType);
		if (iRet != 0) {
			std::cout << "Binning Type: " << iBinningType << "\n";
			std::cout << "HBI_MSetBinning failed. iRet=" << iRet << "\n";
			return false;
		}

		// Exposure time (= 1/fps)
		iRet = HBI_MSetSelfDumpingTime(m_hHBI, iExpMili);
		if (iRet != 0) {
			std::cout << "Exposure Time (ms): " << iExpMili << "\n";
			std::cout << "HBI_MSetSelfDumpingTime failed. iRet=" << iRet << "\n";
			return false;
		}

		/* ROI の設定
		* 横方向は設定ができない。
		* 3030zはデュアル読出しのため、中央から等間隔にオフセットする必要がある。そのため縦方向のサイズは偶数である必要がある。
		* 2520zはシングル読出しのため、縦方向のオフセットは自由にできる。
		* uleft, utop, uright, ubottom = 0 の時はフルサイズとなる
		*/

		if (iZoomHeight != 0) {
			CMOS_ZOOM_RECT.utop = iZoomTop;
			CMOS_ZOOM_RECT.ubottom = iZoomTop + iZoomHeight - 1;
		}
		else {
			CMOS_ZOOM_RECT.utop = 0;
			CMOS_ZOOM_RECT.ubottom = iOriginalHeight - 1;
		}
		if (iZoomWidth != 0) {
			CMOS_ZOOM_RECT.uleft = iZoomLeft;
			CMOS_ZOOM_RECT.uright = iZoomLeft + iZoomWidth -1;
		}
		else {
			CMOS_ZOOM_RECT.uleft = 0;
			CMOS_ZOOM_RECT.uright = iOriginalWidth - 1;
		}
		iRet = HBI_MSetZoomModeRect(m_hHBI, CMOS_ZOOM_RECT);
		if (iRet != 0) {
			std::cout << "HBI_MSetZoomModeRect failed. iRet=" << iRet << "\n";
			return false;
		}
		return true;
	}

	void AllocateImageBuffer(int iCaptureFrame) {
		// 画像バッファを設定する
		m_iFrameCounter = 0;
		m_vecuiImageBuffer.resize(static_cast<size_t>(m_iImageWidth) * static_cast<size_t>(m_iImageHeight) * static_cast<size_t>(iCaptureFrame));
	}

	bool UpdateImageProperties() {
		// 画像のサイズを取得する
		if (!IsHBIInitialized()) { return false; }
		IMAGE_PROPERTY   imgProp;
		int iRet = HBI_GetImageProperty(m_hHBI, &imgProp);
		m_iImageWidth = imgProp.nwidth;
		m_iImageHeight = imgProp.nheight;
		std::cout << "Image Properties:" << m_iImageWidth << " x " << m_iImageHeight << std::endl;

		if (iRet != 0) {
			std::cout << "HBI_GetImageProperty failed. iRet=" << iRet << "\n";
			return false;
		}
		return true;
	}

	// 画像取得開始
	bool StartCapture() {
		FPD_AQC_MODE.eAqccmd = LIVE_ACQ_DEFAULT_TYPE; // ライブキャプチャ用のACQMODE

		if (!IsHBIInitialized()) { return false; }
		m_iFrameCounter = 0;
		m_bIsCapturing = true; // 画像取得中フラグを立てる
		HBI_LiveAcquisition(m_hHBI, m_aqcMode);
		std::cout << "Capture started...\n";
		return true;
	}

	// 画像取得停止
	bool StopCapture() {
		if (!IsHBIInitialized()) { return false; }
		HBI_StopAcquisition(m_hHBI);
		m_bIsCapturing = false; // 画像取得中フラグを下げる
		std::cout << "Capture stopped.\n";
		return true;
	}

	// 画像取得中か判断
	bool IsCapturing() const { return m_bIsCapturing; }

	bool ReleaseDevice() {
		if (IsCapturing()) {
			std::cout << "Stopping capture before release...\n";
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
		if (m_iFrameCounter >= m_iCaptureFrame) {
			m_bIsCapturing = false; // 指定枚数撮影したので保存はしない
			return; 
		}

		// 取得した画像をバッファに保存するため、フレーム数分オフセットする
		const size_t kiFramePixelCount = static_cast<size_t>(m_iImageWidth) * static_cast<size_t>(m_iImageHeight);
		const size_t kiOffset = static_cast<size_t>(m_iFrameCounter) * kiFramePixelCount;

		// IMAGE_DATA_ST構造体内のdatabuffに画像データがあるので、それをバッファにコピーする
		memcpy_s(
			m_vecuiImageBuffer.data() + kiOffset,
			m_vecuiImageBuffer.size() * sizeof(uint16_t) - kiOffset * sizeof(uint16_t), // バッファの残りサイズを計算
			static_cast<IMAGE_DATA_ST*>(pvParam1)->databuff,
			kiFramePixelCount * sizeof(uint16_t)
		);
		// 保存に成功したらフレーム数をカウントする
		std::cout << "    Received multiple image data, frame id: " << m_iFrameCounter << std::endl;
		m_iFrameCounter++;

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
			std::cout << "Set parameters successfully." << std::endl;
		}
		if (eventId == ECALLBACK_TYPE_PACKET_MISS) {
			std::cout << "Packet lost." << std::endl;
		}
		if (eventId == ECALLBACK_TYPE_BUFFER_WARNING) {
			std::cout << "Buffer warning." << std::endl;
		}
		return 1;
	}
};
