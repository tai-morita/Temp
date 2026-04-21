#include <windows.h>
#include <winnt.h>
PCONTEXT __pctx_probe = nullptr;

#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <cstring>
#include <tiffio.h>
#include <numeric>
#include <string>

/*
#include "../../BigTIFF/BigTIFF.h"
#include "../..\CTiff\Tiff.h"
#include "../../Common/Sprintf.h"
*/
#include "HbiFpd.h"
#include "HbiType.h"
#include "HbiError.h"

using namespace std;

// Parameter setting
constexpr int FRAMECOUNT = 100;

constexpr int GAINLEVEL = 2; // 1.2PC
constexpr int EXPTIME_milli = 33; // 33ms
constexpr int EXPTIME_micro = 333; // 333us -> 合計で33.333ms = 1/30s: 30fps

static std::atomic<bool> g_connected{ false };
static std::atomic<bool> g_imageReady{ false };
static IMAGE_PROPERTY g_imgProp;
static FPD_AQC_MODE g_aqcMode;
static COMM_CFG g_commCfg;
static std::vector<uint16_t> g_imageBuffer; // 最終の画像データを保存するグローバルバッファ

static std::int16_t g_iFrameCounter = 0;
static std::atomic<bool> g_CaptureFlag{ false };

size_t g_framePixelCount;
size_t g_totalPixelCount;


void SaveAsTiff(const std::string& filename, const std::vector<uint16_t>& buffer, unsigned int width, unsigned int height)
{
    TIFF* tif = TIFFOpen(filename.c_str(), "w");
    if (!tif) {
        std::cerr << "TIFFOpen failed\n";
        return;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    for (uint32_t row = 0; row < height; ++row) {
        const uint16_t* rowPtr = &buffer[row * width];
        TIFFWriteScanline(tif, (void*)rowPtr, row, 0);
    }

    TIFFClose(tif);
}


void SaveAsMultiFrameTiff(
    const std::string& filename,
    const std::vector<uint16_t>& buffer,
    unsigned int width,
    unsigned int height,
    unsigned int frameCount
)
{
	std::cout << "Saving multi-frame TIFF: " << filename << "\n";
    if (width == 0 || height == 0 || frameCount == 0) {
        std::cerr << "Invalid image size or frame count\n";
        return;
    }

    const size_t framePixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t requiredPixelCount = framePixelCount * static_cast<size_t>(frameCount);
    if (buffer.size() < requiredPixelCount) {
        std::cerr << "Buffer size is smaller than required for requested frame count\n";
        return;
    }

    TIFF* tif = TIFFOpen(filename.c_str(), "w");
    if (!tif) {
        std::cerr << "TIFFOpen failed\n";
        return;
    }

    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_SUBFILETYPE, 0);
        TIFFSetField(tif, TIFFTAG_PAGENUMBER, frame, frameCount);

        const size_t frameOffset = static_cast<size_t>(frame) * framePixelCount;
        for (uint32_t row = 0; row < height; ++row) {
            const uint16_t* rowPtr = &buffer[frameOffset + static_cast<size_t>(row) * width];
            if (TIFFWriteScanline(tif, (void*)rowPtr, row, 0) < 0) {
                std::cerr << "TIFFWriteScanline failed at frame " << frame << ", row " << row << "\n";
                TIFFClose(tif);
                return;
            }
        }

        if (!TIFFWriteDirectory(tif)) {
            std::cerr << "TIFFWriteDirectory failed at frame " << frame << "\n";
            TIFFClose(tif);
            return;
        }
    }

    TIFFClose(tif);
}

void AquisitionSingleFrame(void* pvParam1)
{
   static std::vector<uint16_t> vecimageBuffer;  // こいつはECALLBACK_TYPE_MULTIPLE_IMAGEで取得するときのバッファ

   IMAGE_DATA_ST img;  
   std::memcpy(&img, pvParam1, sizeof(IMAGE_DATA_ST));  

   size_t pixelCount =  
       static_cast<size_t>(g_imgProp.nwidth) *  
       static_cast<size_t>(g_imgProp.nheight);  

   g_imageBuffer.resize(pixelCount); // Ensure the buffer is resized before copying data  

   std::memcpy(  
       g_imageBuffer.data(),
       img.databuff,  
       pixelCount * sizeof(uint16_t)  
   );  

   g_imageReady = true;  
}

void AquisitionMultiFrame(void* pvParam1, int iframePixelCount)
{
    size_t offset = g_iFrameCounter * iframePixelCount;
    static std::vector<uint16_t> vecimageBuffer;  // こいつはECALLBACK_TYPE_MULTIPLE_IMAGEで取得するときのバッファ

    IMAGE_DATA_ST img;
    std::memcpy(&img, pvParam1, sizeof(IMAGE_DATA_ST));

    /*
	std::cout << "pvParam1: " << pvParam1 << std::endl;
    std::cout << "g_imageBuffer.data() + offset: " << g_imageBuffer.data() + offset << std::endl;
    std::cout << "img.databuff: " << img.databuff << std::endl;
    std::cout << "iframePixelCount * sizeof(uint16_t): " << iframePixelCount * sizeof(uint16_t) << std::endl;
    */
    std::memcpy(
        g_imageBuffer.data() + offset,
        img.databuff,
        iframePixelCount * sizeof(uint16_t)
    );

    g_imageReady = true;
}

int HBICallback(
    void*,
    int,
    unsigned char eventId,
    void* pvParam1,
    int nParam2,
    int,
    int
)
{
    //printf("    Event ID: 0x%02X, Param2: %d\n", eventId, nParam2);

    if (eventId == ECALLBACK_TYPE_FPD_STATUS)
    {
        int status = nParam2;
        if (status == 100)
        {
			//printf("        Detector connected\n");
            g_connected = true;
        }
    }

    if (eventId == ECALLBACK_TYPE_SINGLE_IMAGE)
    {
		AquisitionSingleFrame(pvParam1);
    }
    if (eventId == ECALLBACK_TYPE_MULTIPLE_IMAGE)
    {
		if (g_iFrameCounter >= FRAMECOUNT) {
            g_CaptureFlag = true;
			return 1;
		}
        AquisitionMultiFrame(pvParam1, g_framePixelCount);
        g_iFrameCounter++;
		std::cout << "    Received multiple image data, frame id: " << g_iFrameCounter << std::endl;
    }

    return 1;
}

int main()
{

    void* hFpd = HBI_Init(0);
    if (!hFpd)
    {
        std::cout << "HBI_Init failed\n";
        return -1;
    }
    HBI_RegEventCallBackFun(hFpd, HBICallback, nullptr);

	g_commCfg._type = UDP_JUMBO_COMM_TYPE;
    strcpy_s(g_commCfg._remoteip, sizeof(g_commCfg._remoteip), "192.168.10.40");
    strcpy_s(g_commCfg._localip, sizeof(g_commCfg._localip), "192.168.10.20");
    g_commCfg._remotePort = 32897;
    g_commCfg._loacalPort = 32896;

    int ret = HBI_ConnectDetectorJumbo(
        hFpd,
        (char*)"192.168.10.40", // FPD IP
        32897,                 // FPD port
        (char*)"192.168.10.20", // PC IP
        32896,                 // PC port
        0
    );

    if (ret != 0)
    {
        std::cout << "Connect failed\n";
        HBI_Destroy(hFpd);
        return -1;
    }

    while (!g_connected)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)
        );
    }


    ret = HBI_GetImageProperty(hFpd, &g_imgProp);
    if (ret != 0)
    {
        std::cout << "HBI_GetImageProperty failed\n";
        HBI_Destroy(hFpd);
        return -1;
    }


    std::cout
        << "  width=" << g_imgProp.nwidth << "\n"
        << "  height=" << g_imgProp.nheight << "\n"
        << "  databit=" << g_imgProp.ndatabit << "\n"
        << std::endl;

    // パラメータの設定
    HBI_MSetPGALevel(hFpd, GAINLEVEL);
	HBI_MSetFpsOfTime(hFpd, EXPTIME_milli, EXPTIME_micro);

	// マルチフレーム取得のためのバッファを確保
    g_framePixelCount = g_imgProp.nwidth * g_imgProp.nheight;
    g_totalPixelCount = FRAMECOUNT * g_framePixelCount;

    g_imageBuffer.resize(g_totalPixelCount);

	//CArray2D<unsigned short> a2dImage(g_imgProp.nwidth, g_imgProp.nheight);

    //std::wstring strSaveFilePath = L"D:\\github\\Temp\\CapturerByHBI\\CapturerByHBI\\data\\Test_MultiFrame.tif";
    //CBigTIFF tiffOut;
	//tiffOut.OpenFileToWrite(strSaveFilePath);


    if (false) {
        HBI_SinglePrepare(hFpd);
        HBI_SingleAcquisition(hFpd, g_aqcMode);
    }
	else {
        g_aqcMode.eAqccmd = LIVE_ACQ_DEFAULT_TYPE;
        HBI_LiveAcquisition(hFpd, g_aqcMode);

        while (!g_CaptureFlag)
        {
		    std::this_thread::sleep_for(
			    std::chrono::milliseconds(50)
		    );
        }
		bool flag_stop = false;
        while(g_iFrameCounter >= FRAMECOUNT && !flag_stop){
            HBI_StopAcquisition(hFpd);
            std::cout << "Stopped acquisition" << std::endl;
			flag_stop = true;
        }

   	}



    std::string strSaveFilePath = "D:\\github\\CapturerByHBI\\CapturerByHBI\\data\\Test_1Frame_2.tif";

    SaveAsMultiFrameTiff(
        strSaveFilePath,
        g_imageBuffer,
        g_imgProp.nwidth,
        g_imgProp.nheight,
        FRAMECOUNT
    );


    HBI_Destroy(hFpd);

    std::cout << "Finished\n";
    return 0;
}