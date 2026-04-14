#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <cstring>
#include <tiffio.h>

#include "HbiFpd.h"
#include "HbiType.h"
#include "HbiError.h"
#include <string>

static std::atomic<bool> g_connected{ false };
static std::atomic<bool> g_imageReady{ false };

static IMAGE_PROPERTY g_imgProp;
static std::vector<uint16_t> g_imageBuffer;
static FPD_AQC_MODE g_aqcMode;

static COMM_CFG g_commCfg;

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

// コールバック関数
int HBICallback(
    void*,
    int,
    unsigned char eventId,
    void* pvParam1,
    int,
    int,
    int
)
{
        // statusの初期値
    int status = 0;
    if (pvParam1 != nullptr) {
        status = reinterpret_cast<intptr_t>(pvParam1);
    }
    std::cout << "[Callback] eventId=" << static_cast<int>(eventId)
        << ", status=" << status << std::endl;
    if (eventId == ECALLBACK_TYPE_FPD_STATUS)
    {
        if (pvParam1 == nullptr && g_connected == false)
        {
            int status = *reinterpret_cast<int*>(&eventId);
        }
    }

    if (eventId == ECALLBACK_TYPE_FPD_STATUS)
    {
        int status = reinterpret_cast<int>(pvParam1);
        if (status == 100)
        {
			printf("Detector connected\n");
            g_connected = true;
        }
    }

    if (eventId == ECALLBACK_TYPE_SINGLE_IMAGE)
    {
        // 実際の構造体名は HbiType.h に定義されているものを使用
        IMAGE_DATA_ST img;
		std::memcpy(&img, pvParam1, sizeof(IMAGE_DATA_ST));

        size_t pixelCount =
            static_cast<size_t>(g_imgProp.nwidth) *
            static_cast<size_t>(g_imgProp.nheight);

        std::memcpy(
            g_imageBuffer.data(),
            img.databuff,
            pixelCount * sizeof(uint16_t)
        );

        g_imageReady = true;
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


    // 画像プロパティ取得
    ret = HBI_GetImageProperty(hFpd, &g_imgProp);
    if (ret != 0)
    {
        std::cout << "HBI_GetImageProperty failed\n";
        HBI_Destroy(hFpd);
        return -1;
    }


    std::cout
        << "width=" << g_imgProp.nwidth << "\n"
        << "height=" << g_imgProp.nheight << "\n"
        << "databit=" << g_imgProp.ndatabit << "\n"
        << std::endl;

    size_t pixelCount =
        static_cast<size_t>(g_imgProp.nwidth) *
        static_cast<size_t>(g_imgProp.nheight);

    g_imageBuffer.resize(pixelCount);

    // 単フレーム撮影
    HBI_SinglePrepare(hFpd);
    HBI_SingleAcquisition(hFpd, g_aqcMode);


    while (!g_imageReady)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(10)
        );
    }

    // 16bit RAW 保存
    std::string strSaveFilePath = "D:\\__SVN\\taimlab\\trunk\\CapturerByHBI\\data\\Test_SingleImage.tif";
    SaveAsTiff(strSaveFilePath, g_imageBuffer, g_imgProp.nwidth, g_imgProp.nheight);

    HBI_Destroy(hFpd);

    std::cout << "Finished\n";
    return 0;
}