

#include "HbiFpd.h"
#include "HbiType.h"
#include "HbiError.h"

//  デバイスの情報を取得するクラス
class HBIDeviceProperties
{
	static IMAGE_PROPERTY m_imgProp;
	const void* m_hFpd;

	// FPD の ID? を取得?
	int GetFpdNumber() const { return m_imgProp.nFpdNum ? m_imgProp.nFpdNum : 0; }

	// FPD の縦方向の画素数を取得
	int GetImageHeight() const { return m_imgProp.nheight ? m_imgProp.nheight : 0; }

	// FPD の横方向の画素数を取得
	int GetImageWidth() const { return m_imgProp.nwidth ? m_imgProp.nwidth : 0; }

	// FPD のデータタイプを取得
	// 0: unsigned char, 1: unsigned short, 2: unsigned int
	int GetDataType() const { return m_imgProp.datatype ? m_imgProp.ndatabit : 0; }

	// FPD のデータビット数を取得
	// 0: 16bit, 1: 14bit, 2: 12bit, 3: 8bit
	int GetDataBit() const { return m_imgProp.ndatabit ? m_imgProp.ndatabit : 0; }

	// FPD のエンディアンを取得
	// 0: little endian, 1: big endian
	int GetEndian() const { return m_imgProp.nendian ? m_imgProp.nendian : 0; }

	// FPD のパケットサイズを取得
	int GetPacketSize() const { return m_imgProp.packet_size ? m_imgProp.packet_size : 0; }

	// FPD のフレームサイズを取得
	int GetFrameSize() const { return m_imgProp.frame_size ? m_imgProp.frame_size : 0; }

	// FPD の Tail パケットサイズ? を取得
	int GetTailPacketSize() const { return m_imgProp.tailPacketSize ? m_imgProp.tailPacketSize : 0; }

	// FPD バッファのサイズを取得
	//The number of buffer capacity,Integer,[2~32],buff_szie=frame_size * ncapacity
	int GetBufferCapacity() const { return m_imgProp.frame_number ? m_imgProp.frame_number : 0; }


};