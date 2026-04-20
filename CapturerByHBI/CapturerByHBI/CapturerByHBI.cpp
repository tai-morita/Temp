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

#include "../BigTIFF/BigTIFF.h"
#include "..\CTiff\Tiff.h"
#include "../Common/Sprintf.h"

#include "HbiFpd.h"
#include "HbiType.h"
#include "HbiError.h"

using namespace std;

static std::atomic<bool> g_connected{ false };
static std::atomic<bool> g_imageReady{ false };
static IMAGE_PROPERTY g_imgProp;
static FPD_AQC_MODE g_aqcMode;
static COMM_CFG g_commCfg;

/*
void SaveAsTiff(const std::wstring& filename, const std::vector<uint16_t>& buffer, unsigned int width, unsigned int height)
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
}*/

//static std::vector<uint16_t> AquisitionSingleFrame(void* pvParam1)  
void AquisitionSingleFrame(void* pvParam1)
{  
   static std::vector<uint16_t> vecimageBuffer;  

   std::cout << "    Received single image data" << std::endl;  
   IMAGE_DATA_ST img;  
   std::memcpy(&img, pvParam1, sizeof(IMAGE_DATA_ST));  

   size_t pixelCount =  
       static_cast<size_t>(g_imgProp.nwidth) *  
       static_cast<size_t>(g_imgProp.nheight);  

   vecimageBuffer.resize(pixelCount); // Ensure the buffer is resized before copying data  

   std::memcpy(  
       vecimageBuffer.data(),  
       img.databuff,  
       pixelCount * sizeof(uint16_t)  
   );  

   std::cout << "Mean pixel value: " << std::accumulate(vecimageBuffer.begin(), vecimageBuffer.end(), 0.0) / pixelCount << std::endl;  

   g_imageReady = true;  

   //return vecimageBuffer; // Return the vector directly  
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
    printf("    Event ID: 0x%02X, Param2: %d\n", eventId, nParam2);

    if (eventId == ECALLBACK_TYPE_FPD_STATUS)
    {
        int status = nParam2;
        if (status == 100)
        {
			printf("        Detector connected\n");
            g_connected = true;
        }
    }

    if (eventId == ECALLBACK_TYPE_SINGLE_IMAGE)
    {
		AquisitionSingleFrame(pvParam1);
    }
    if (eventId == ECALLBACK_TYPE_MULTIPLE_IMAGE)
    {
		std::cout << "    Received multiple image data, frame id: " << nParam2 << std::endl;
        AquisitionSingleFrame(pvParam1);
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



    int nGainLevel = 2; // 1.2PC
    HBI_SetPGALevel(hFpd, nGainLevel);

	CArray2D<unsigned short> a2dImage(g_imgProp.nwidth, g_imgProp.nheight);

    std::wstring strSaveFilePath = L"D:\\github\\Temp\\CapturerByHBI\\CapturerByHBI\\data\\Test_MultiFrame.tif";
    CBigTIFF tiffOut;
	tiffOut.OpenFileToWrite(strSaveFilePath);


    if (false) {
        HBI_SinglePrepare(hFpd);
        HBI_SingleAcquisition(hFpd, g_aqcMode);
    }
	else {
        g_aqcMode.eAqccmd = LIVE_ACQ_DEFAULT_TYPE;
        HBI_LiveAcquisition(hFpd, g_aqcMode);

        /*
        std::this_thread::sleep_for(
            std::chrono::seconds(1)
        );
        */

        HBI_StopAcquisition(hFpd);
		std::cout << "Stopped acquisition\n";
		return 0;

	}


    while (!g_imageReady)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(10)
        );
    }


    //SaveAsTiff(strSaveFilePath, g_imageBuffer, g_imgProp.nwidth, g_imgProp.nheight);

    HBI_Destroy(hFpd);

    std::cout << "Finished\n";
    return 0;
}