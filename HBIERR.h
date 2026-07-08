/**
* @file  HBIERRStr.h
* @brief HBI のエラーコードをエラーメッセージに変換するクラス
*/
#pragma once
#include<string>
#include "../Common/HBI_DLL/INCLUDES/HbiError.h"

class CHBIERRStr : public std::string {
public:
	/**
	* @brief コンストラクタ
	*/
	CHBIERRStr(HBIRETCODE hbierr)
	: std::string(HBIERR2Str(hbierr))
	{}

private:
	/**
	* @brief HBI のエラーコードをエラーメッセージに変換する
	* @param hbierr: HBI のエラーコード
	* @return エラーコードに対応するエラーメッセージ
	*/
	std::string HBIERR2Str(HBIRETCODE hbierr) const {
		if (hbierr == HBI_ERR_ININT_FAILED)          { return "Init dll failed"                         ;}
		if (hbierr == HBI_ERR_OPEN_DETECTOR_FAILED)  { return "Open device driver failed"               ;}
		if (hbierr == HBI_ERR_INVALID_PARAMS)        { return "Parameter error"                         ;}
		if (hbierr == HBI_ERR_CONNECT_FAILED)        { return "Connect failed"                          ;}
		if (hbierr == HBI_ERR_MALLOC_FAILED)         { return "Malloc memory failed"                    ;}
		if (hbierr == HBI_ERR_RELIMGMEM_FAILED)      { return "Releaseimagemem fail"                    ;}
		if (hbierr == HBI_ERR_RETIMGMEM_FAILED)      { return "ReturnImageMem fail"                     ;}
		if (hbierr == HBI_ERR_NODEVICE)              { return "No Init DLL Instance"                    ;}
		if (hbierr == HBI_ERR_DISCONNECT)            { return "Is Disconnect"                           ;}
		if (hbierr == HBI_ERR_DEVICE_BUSY)           { return "Fpd is busy"                             ;}
		if (hbierr == HBI_ERR_SENDDATA_FAILED)       { return "SendData failed"                         ;}
		if (hbierr == HBI_ERR_RECEIVE_DATA_FAILED)   { return "Receive Data failed"                     ;}
		if (hbierr == HBI_ERR_COMMAND_DISMATCH)      { return "Command dismatch"                        ;}
		if (hbierr == HBI_ERR_NO_IMAGE_RAW)          { return "No Image raw"                            ;}
		if (hbierr == HBI_ERR_PTHREAD_ACTIVE_FAILED) { return "Pthread active failed"                   ;}
		if (hbierr == HBI_ERR_STOP_ACQUISITION)      { return "Pthread stop data acquisition failed"    ;}
		if (hbierr == HBI_ERR_INSERT_FAILED)         { return "insert calibrate mode failed"            ;}
		if (hbierr == HBI_ERR_GET_CFG_FAILED)        { return "get Fpd config failed"                   ;}
		if (hbierr == HBI_NOT_SUPPORT)               { return "not surport yet"                         ;}
		if (hbierr == HBI_REGISTER_CALLBACK_FAILED)  { return "failed to register callback function"    ;}
		if (hbierr == HBI_SEND_MESSAGE_FAILD)        { return "send message failed"                     ;}
		if (hbierr == HBI_ERR_WORKMODE)              { return "switch work mode failed"                 ;}
		if (hbierr == HBI_FAILED)                    { return "operation failed"                        ;}
		if (hbierr == HBI_FILE_NOT_EXISTS)           { return "file does not exist"                     ;}
		if (hbierr == HBI_COMM_TYPE_ERR)             { return "communication is not exist"              ;}
		if (hbierr == HBI_TYPE_IS_NOT_EXISTS)        { return "this type is not exists"                 ;}
		if (hbierr == HBI_SAVE_FILE_FAILED)          { return "save file failed"                        ;}
		if (hbierr == HBI_INIT_PARAM_FAILED)         { return "Init dll param failed"                   ;}
		if (hbierr == HBI_INIT_FILE_NOT_EXIST)       { return "Init dll config file is not exist"       ;}
		if (hbierr == HBI_INVALID_FLAT_PANEL)        { return "Invalid flat panel detector"             ;}
		if (hbierr == HBI_INVALID_DLL_HANDLE)        { return "Invalid dll instance handle"             ;}
		if (hbierr == HBI_FPD_IS_DISCONNECT)         { return "Fpd is disconnect"                       ;}
		if (hbierr == HBI_ERR_DETECTOR_NUMBER)       { return "Detector number error"                   ;}
		if (hbierr == HBI_ERR_DATA_CHECK_FAILED)     { return "Data Check failed"                       ;}
		if (hbierr == HBI_ERR_CFG_ISEMPTY)           { return "The configuration variable is empty"     ;}
		if (hbierr == HBI_ERR_OPEN_WIZARD_FAILED)    { return "Open template wizard failed"             ;}
		if (hbierr == HBI_ERR_WIZARD_ALREADY_EXIST)  { return "template wizard already exist"           ;}
		if (hbierr == HBI_DIRECTORY_NOT_EXISTS)      { return "File directory is not exists"            ;}
		if (hbierr == HBI_UNREGISTER_CALLBACK)       { return "Unregistered callbacke function"         ;}
		if (hbierr == HBI_ERR_IMAGE_ISEMPTY)         { return "Image is empty"                          ;}
		if (hbierr == HBI_ERR_ABNORMMAL_IMAGE)       { return "Abnormal image"                          ;}
		if (hbierr == HBI_PREPARATION_COMPLETE)      { return "Preparation complete"                    ;}
		if (hbierr == HBI_ERR_FILE_EXCEPTION)        { return "File exception"                          ;}
		if (hbierr == HBI_ERR_COMM_TYPE)             { return "Wrong communication type"                ;}
		if (hbierr == HBI_ERR_INVALID_MODE)          { return "This mode contains incorrect parameters" ;}
		if (hbierr == HBI_ERR_FIRM_CONFIG_DATA)      { return "Firmware parameter data is abnormal"     ;}
		if (hbierr == HBI_ERR_IS_NULL)               { return "The pointer is NULL"                     ;}
		if (hbierr == HBI_ERR_ROM_PARAMETER)         { return "ROM includes incorrect parameters"       ;}
		if (hbierr == HBI_ERR_FILE_TYPE)             { return "File type exception"                     ;}
		if (hbierr == HBI_ERR_PCIE_DEVICE_USED)      { return "PCIE device has been used"               ;}
		if (hbierr == HBI_END)                       { return "Exit monitoring"                         ;}
	}
};