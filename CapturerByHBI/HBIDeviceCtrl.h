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
	{}

	// デストラクタ。
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
		if (m_pImageBuffer != nullptr) {
			delete[] m_pImageBuffer;
			m_pImageBuffer = nullptr;
		}
	}


public:
	// CapturerHBIDlg.cpp からアクセスするためのゲッターメソッド。
	const uint16_t* GetImageBuffer () const { return m_pImageBuffer; }
	const size_t GetImageBufferSize() const { return m_iImageBufferSize; }
	const int GetImageWidth        () const { return m_iImageWidth; }
	const int GetImageHeight       () const { return m_iImageHeight; }

	/**
	 * @brief SDK の関数を呼び出す前の初期化処理。
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
	 * @brief SDK のイベントコールバック関数を設定する。
	 * イベントが起こった時、SDK が UsetHBICallBack を呼び出す。
	 */
	void SetCallbackFunction() { HBI_RegEventCallBackFun(m_hHBI, UserHBICallback, this); }

	/**
	 * @brief Jumbo Packet でデバイスに接続する。
	 * @param pcFPDIP   デバイスの IP アドレス。
	 * @param usFPDPORT デバイスのポート番号。
	 * @param pcPCIP    PC の IP アドレス。
	 * @param usPCPORT  PC のポート番号。
	 * @return 接続に成功した場合は true、失敗した場合は false。
	 * @note   HBI_ConnectDetectorJumbo の戻り値は 0 が成功、0 以外が失敗。
	 */
	bool ConnectJumbo(const char* kpcFPDIP, const unsigned short kusFPDPORT, const char* kpcPCIP, const unsigned short kusPCPORT) {
		if (!IsHBIInitialized()) { return false; }
		int iRet = HBI_ConnectDetectorJumbo(m_hHBI, const_cast<char*>(kpcFPDIP), kusFPDPORT, const_cast<char*>(kpcPCIP), kusPCPORT, 0);
		if (iRet) {
			std::cout << "HBI_ConnectDetectorJumbo failed. iRet=" << iRet << "\n";
			return false;
		}
		std::cout << "Connected to the device successfully.\n";
		return true;
	}

	/**
	 * @brief デバイスが接続されているか確認する。
	 * @return 接続されている場合は true、接続されていない場合は false。
	 * @note   HBI_IsConnect の戻り値は 1 が接続、0 が未接続。
	 */
	bool IsConnected() {
		if (!IsHBIInitialized()) { return false; }
		if (!HBI_IsConnect(m_hHBI)) {
			std::cout << "Device is not connected.\n";
			return false;
		}
		return true;
	}

	/**
	 * @brief デバイスのシリアル番号を取得する。
	 * @return 取得に成功した場合はシリアル番号の文字列、失敗した場合は空文字列。
	 * @note   HBI_GetFPDSerialNumber の戻り値は 0 が成功、0 以外が失敗。
	 */
	std::string GetFPDSerialNumber() {
		// パネルの情報を取得する。
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
	 * @brief デバイスの製品コードを取得する。
	 * @return 取得に成功した場合は製品コードの文字列、失敗した場合は空文字列。
	 * @note   HBI_GetHbiProductCode の戻り値は 0 が成功、0 以外が失敗。
	 */
	std::string  GetFPDProductCode(){
		if (!m_bIsHBIInitialized) { return ""; }

		char pcProductCode[128]  = { 0 };
		int iRetProductCode      = HBI_GetHbiProductCode(m_hHBI, pcProductCode);
		if (iRetProductCode != 0) {
			std::cout << "HBI_GetFPDProductCode failed. iRet=" << iRetProductCode << "\n";
			return "";
		}
		m_strProductCode = std::string(pcProductCode);

		std::cout << "FPD Product Code: " << m_strProductCode << "\n";
		return m_strProductCode;
	}
	
	/**
	 * @brief SDK のバージョンを取得する。
	 * @return 取得に成功した場合は SDK のバージョンの文字列、失敗した場合は空文字列。
	 * @note   HBI_GetSDKVerion の戻り値は 0 が成功、0 以外が失敗。
	 */
	std::string GetSDKVersion() {
		// HBISDKApi.dll のバージョンを取得する。
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
	 * @brief  現在のキャプチャパラメータをコンソール上に出力する。
	 * @return すべてのパラメータの取得に成功した場合は true、どこかで取得に失敗した場合は false。
	 * @note   デバッグとして使用する。
	 */
	bool PrintCaptureParams() {
		if (!m_bIsHBIInitialized) { return false; }

		int iRet;

		// GainType: HBI_GetPGALevel の戻り値は GainType (1 ～ 11) が成功、失敗は -1。
		int iGainType = HBI_GetPGALevel(m_hHBI);
		 if (iGainType < 1 || iGainType > 11) { 
			std::cout << "HBI_GetPGALevel failed."<< "\n"; 
			return false;
		 }

		// Binning: HBI_GetBinning の戻り値は 0が成功、失敗は 0以外。
		// iBinningType の値は 1: 1x1, 2: 2x2, 3: 3x3, 4: 4x4。
		unsigned int uiBinningType;
		iRet = HBI_GetBinning(m_hHBI, &uiBinningType);
		if (iRet!=0) { 
			std::cout << "HBI_GetBinning failed. iRet=" << iRet << "\n"; 
			return false;
		}
		
		// Capture 領域: HBI_GetCurCaptureOffsetRect の戻り値は 0が成功、失敗は 0以外。
		// ihbiCaptureOffsetLeft, ihbiCaptureOffsetTop, ihbiCaptureOffsetWidth, ihbiCaptureOffsetHeight はそれぞれ左上の座標と幅、高さを表し。
		// それぞれの変数に値が格納される。
		// 0 が成功、0 以外が失敗。
		unsigned int uiCaptureOffsetLeft, uiCaptureOffsetTop, uiCaptureOffsetWidth, uiCaptureOffsetHeight;
		iRet =	HBI_GetCurZoomRect(m_hHBI, &uiCaptureOffsetLeft, &uiCaptureOffsetTop, &uiCaptureOffsetWidth, &uiCaptureOffsetHeight);
		if (iRet!=0) { 
			std::cout << "HBI_GetCurZoomRect failed. iRet=" << iRet << "\n"; 
			return false;
		}

		// Exposure time: HBI_GetSelfDumpingTime の戻り値は 0が成功、失敗は 0以外。
		// imsExposureTime は Exposure time をミリ秒単位で表す。
		// 0 が成功、0 以外が失敗。
		int imsExposureTime;
		iRet = HBI_GetSelfDumpingTime(m_hHBI, &imsExposureTime);
		if (iRet!=0) { 
			std::cout << "HBI_GetSelfDumpingTime failed. iRet=" << iRet << "\n"; 
			return false;
		}
		// 出力用に fps を計算する。
		float fExpSec = static_cast<float>(imsExposureTime) / 1000.0f;
		float ffps = 1.0f / static_cast<float>(fExpSec);

		// 取得したパラメータをコンソール上に出力する。
		std::cout << "Getting current capture parameters...\n";
		std::cout << "  Binning Type     : " << uiBinningType                                 << "\n";
		std::cout << "  Gain Type        : " << HBI_GetPGALevel(m_hHBI)                       << "\n";
		std::cout << "  Exposure Time (s): " << std::fixed << std::setprecision(2) << fExpSec << " sec\n";
		std::cout << "  Frame per Second : " << std::fixed << std::setprecision(2) << ffps    << " fps\n";
		std::cout << "  Capture Area     : " << 
		  "(Left, Top) = (" << uiCaptureOffsetLeft << ", " << uiCaptureOffsetTop << ") , Width x Height = " << uiCaptureOffsetWidth << " x " << uiCaptureOffsetHeight << "\n";

		  return true;
	}

	/**
	 * @brief  キャプチャパラメータを設定する。
	 * @param  iGainType           Gain Type (1 ~ 11)。
	 * @param  iExpMilli           Exposure time (ms)。
	 * @param  iCaptureFrame       取得するフレーム数。
	 * @param  iBinningType        Binning (1: 1x1, 2: 2x2, 3: 3x3, 4: 4x4)。
	 * @param  iOriginalSizeWidth  フルエリアの幅(3030z のキャプチャ領域の指定で必要)。
	 * @param  iOriginalSizeHeight フルエリアの高さ(3030z のキャプチャ領域の指定で必要)。
	 * @param  iCaptureOffsetLeft           キャプチャ領域の左上の x ()。
	 * @param  iCaptureOffsetTop            キャプチャ領域の左上の y ()。
	 * @param  iCaptureOffsetWidth          キャプチャ領域の幅。
	 * @param  iCaptureOffsetHeight         キャプチャ領域の高さ。
	 * @return すべてのパラメータの設定に成功した場合は true、どこかで設定に失敗した場合は false。
	 * @note   3030z デュアル読出しのため、Width はフルエリア、Height は中心から等間隔にオフセットする必要がある。
	 *         現在 3030z の採用予定はない。
	 */
	bool SetCaptureParams(
		int iGainType            = 0,
		int imillisecExopureTime = 0,
		int iCaptureFrame        = 0,
		int iBinningType         = 0,
		int iOriginalSizeWidth   = 0,
		int iOriginalSizeHeight  = 0,
		int iCaptureOffsetLeft   = 0,
		int iCaptureOffsetTop    = 0,
		int iCaptureOffsetWidth  = 0,
		int iCaptureOffsetHeight = 0
	) {
		if (!IsHBIInitialized()) { return false; }
		// 取得フレーム数。
		SetCaptureFrame(iCaptureFrame);

		// GainType。
		int iRet = HBI_MSetPGALevel(m_hHBI, iGainType);
		if (iRet != 0) {
			std::cout << "HBI_MSetPGALevel failed. input iGainType=" << iGainType 
			          << ", iRet=" << iRet << "\n";
			return false;
		}

		// Binning。
		iRet = HBI_MSetBinning(m_hHBI, iBinningType);
		if (iRet != 0) {
			std::cout << "HBI_MSetBinning failed. input iBinningType=" << iBinningType 
			          << ", iRet=" << iRet << "\n";
			return false;
		}

		// Exposure time (= 1/fps)。
		iRet = HBI_MSetSelfDumpingTime(m_hHBI, imillisecExopureTime);
		if (iRet != 0) {
			std::cout << "HBI_MSetSelfDumpingTime failed. input imillisecExopureTime=" << imillisecExopureTime
			          << ", iRet=" << iRet << "\n";
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
			hbiCaptureOffset.utop    = (iOriginalSizeHeight - iCaptureOffsetHeight) / 2;
			hbiCaptureOffset.ubottom = hbiCaptureOffset.utop + iCaptureOffsetHeight - 1;
			hbiCaptureOffset.uleft   = 0;
			hbiCaptureOffset.uright  = 0;			
		} else {
			hbiCaptureOffset.utop    = iCaptureOffsetTop;
			hbiCaptureOffset.ubottom = iCaptureOffsetTop + iCaptureOffsetHeight - 1;
			hbiCaptureOffset.uleft   = iCaptureOffsetLeft;
			hbiCaptureOffset.uright  = iCaptureOffsetLeft + iCaptureOffsetWidth -1;
		}
		// ZoomWidth, ZoomHeight が 0 の時はフルサイズになるようにする。
		if (iCaptureOffsetWidth == 0) {
			hbiCaptureOffset.uleft  = 0;
			hbiCaptureOffset.uright = 0;
		}
		if (iCaptureOffsetHeight == 0) {
			hbiCaptureOffset.utop    = 0;
			hbiCaptureOffset.ubottom = 0;
		}
		iRet = HBI_MSetZoomModeRect(m_hHBI, hbiCaptureOffset);
		if (iRet != 0) {
			std::cout << "HBI_MSetZoomModeRect failed. \n";
			std::cout << "input Parameters : ihbiCaptureOffsetLeft = " << iCaptureOffsetLeft 
				<< ", ihbiCaptureOffsetTop="    << iCaptureOffsetTop 
				<< ", ihbiCaptureOffsetWidth="  << iCaptureOffsetWidth 
				<< ", ihbiCaptureOffsetHeight=" << iCaptureOffsetHeight
				<< "\n iRet=" << iRet << "\n";
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
		if (   !IsHBIInitialized()
		    || !IsConnected()
		    || IsCapturing()) {
			std::cout << "Cannot allocate image buffer. Invalid state: "
				<< "IsHBIInitialized=" << IsHBIInitialized()
				<< ", IsConnected="    << IsConnected()
				<< ", IsCapturing="    << IsCapturing() << "\n";
			return false; 
		}

		// すでにバッファが確保されている場合は、一度解放してから再度確保する。
		if (m_pImageBuffer != nullptr) {
			delete[] m_pImageBuffer;
			m_pImageBuffer = nullptr;
		}

		// 画像バッファを確保する。
		// どれかの値が 0 の場合はバッファを確保しない。
		if (!m_iImageWidth || !m_iImageHeight || !iCaptureFrame) {
			std::cout << "Image buffer allocation failed. Invalid parameters: "
			          << "Width=" << m_iImageWidth << ", Height=" << m_iImageHeight 
			          << ", CaptureFrame=" << iCaptureFrame << "\n";
			return false;
		}
		   
		m_iImageBufferSize = static_cast<size_t>(m_iImageWidth) * static_cast<size_t>(m_iImageHeight) * static_cast<size_t>(iCaptureFrame);
		m_pImageBuffer = new uint16_t[m_iImageBufferSize];
		std::cout << "Allocated image buffer of size: " << m_iImageBufferSize << " (Width=" << m_iImageWidth
			<< ", Height=" << m_iImageHeight << ", CaptureFrame=" << iCaptureFrame << ")\n";
		return true;
	}

	/**
	 * @brief 画像のサイズを取得する。
	 * @return 画像のサイズの取得に成功した場合は true、失敗した場合は false。
	 * @note   画像のサイズは m_iImageWidth, m_iImageHeight に格納される。
	 */
	bool UpdateImageProperties() {
		// 画像のサイズを取得する。
		if (!IsHBIInitialized()) { return false; }

		IMAGE_PROPERTY hbiImageProperty;
		int iRet       = HBI_GetImageProperty(m_hHBI, &hbiImageProperty);
		m_iImageWidth  = hbiImageProperty.nwidth;
		m_iImageHeight = hbiImageProperty.nheight;
		std::cout << "Image Properties:" << m_iImageWidth << " x " << m_iImageHeight << std::endl;

		if (iRet != 0) {
			std::cout << "HBI_GetImageProperty failed. iRet=" << iRet << "\n";
			return false;
		}
		return true;
	}

	/**
	 * @brief  画像取得を開始する。
	 * @return 画像取得の開始に成功した場合は true、失敗した場合は false。
	 * @note   HBI_LiveAcquisition の戻り値は 0 が成功、0 以外が失敗。
	 *         LIVE_ACQ_DEFAULT_TYPE は HBIASDK のライブキャプチャモード。
	 */
	bool StartCapture() {
		FPD_AQC_MODE hbi_aqc_mode;
		hbi_aqc_mode.eAqccmd = LIVE_ACQ_DEFAULT_TYPE;

		if (!IsHBIInitialized()) { return false; }
		m_iFrameCounter = 0;
		int iRet = HBI_LiveAcquisition(m_hHBI, hbi_aqc_mode);
		if (iRet != 0) {
			std::cout << "HBI_LiveAcquisition failed. iRet=" << iRet << "\n";
			return false;
		}
		m_bIsCapturing = true; // 画像取得中フラグを立てる。
		std::cout << "Capture started...\n";
		return true;
	}

	/**
	 * @brief  画像取得を停止する。
	 * @return 画像取得の停止に成功した場合は true、失敗した場合は false。
	 * @note   HBI_StopAcquisition の戻り値は 0 が成功、0 以外が失敗。
	 */
	bool StopCapture() {
		if (!IsHBIInitialized()) { return false; }
		HBI_StopAcquisition(m_hHBI);
		m_bIsCapturing = false; // 画像取得中フラグを下げる。
		std::cout << "Capture stopped.\n";
		return true;
	}

	// 画像取得中か判断する。
	bool IsCapturing() const { return m_bIsCapturing; }

	/**
	 * @brief  デバイスとの接続を切断し、SDK を終了する。
	 * @return 接続の切断と SDK の終了に成功した場合は true、失敗した場合は false。
	 * @note   HBI_Destroy の戻り値は 0 が成功、0 以外が失敗。
	 */
	bool ReleaseDevice() {
		// 画像取得中の場合は停止する。
		if (IsCapturing()) {
			std::cout << "Stopping capture before release...\n";
			StopCapture();
		}
		// 画像バッファを解放する。
		if (m_pImageBuffer != nullptr) {
			delete[] m_pImageBuffer;
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
	void SaveImageBuffer(void* pvParam1){
		if (m_iFrameCounter >= m_iCaptureFrame) {
			// 指定枚数撮影したので保存はしない。
			m_bIsCapturing = false;
			return; 
		}

		// 取得した画像をバッファに保存するため、フレーム数だけオフセットする。
		const size_t kiFramePixelCount = static_cast<size_t>(m_iImageWidth) * static_cast<size_t>(m_iImageHeight);
		const size_t kiOffset          = static_cast<size_t>(m_iFrameCounter) * kiFramePixelCount;

		// IMAGE_DATA_ST構造体内のdatabuffに画像データがあるので、それをバッファにコピーする。
		memcpy_s(
			m_pImageBuffer + kiOffset,
			m_iImageBufferSize * sizeof(uint16_t) - kiOffset * sizeof(uint16_t), // バッファの残りサイズを計算する。
			static_cast<IMAGE_DATA_ST*>(pvParam1)->databuff,
			kiFramePixelCount * sizeof(uint16_t)
		);

		// 保存に成功したらフレーム数をカウントする。
		std::cout << "    Received multiple image data, frame id: " << m_iFrameCounter << std::endl;
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
		void*         pContext,
		int           ifpdId,
		unsigned char uceventId,
		void*         peventParam1,
		int           ievantParam2,
		int           ievantParam3,
		int           ievantParam4
	)
	{
		// SDK側で取得したポインタを CHBIDeviceCtrl クラスのオブジェクトのポインタとしてキャストする。
		// 木村さんへ: ここのポインタの変数名が決めきれないです。
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
		void*         peventParam1,
		int           ievantParam2,
		int           ievantParam3,
		int           ievantParam4
	)
	{
		// peventParam1 以外は使っていないが、 SDK の仕様上、引数として受け取る必要がある。
		(void)ifpdId;
		(void)ievantParam2;
		(void)ievantParam3;
		(void)ievantParam4;
		if (uceventId == ECALLBACK_TYPE_MULTIPLE_IMAGE){
			SaveImageBuffer(peventParam1);
		}
		if (uceventId == ECALLBACK_TYPE_SET_CFG_OK) {
			std::cout << "Set parameters successfully." << std::endl;
		}
		if (uceventId == ECALLBACK_TYPE_PACKET_MISS) {
			std::cout << "Packet lost." << std::endl;
		}
		if (uceventId == ECALLBACK_TYPE_BUFFER_WARNING) {
			std::cout << "Buffer warning." << std::endl;
		}
		return 1;
	}
};
