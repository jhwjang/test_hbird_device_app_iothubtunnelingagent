﻿#pragma once

#include <string>
#include <mutex>
#include <time.h>
#include <vector>

#include "curl/curl.h"

#include "jansson.h"

typedef struct MemoryStruct {
	char* memory;
	size_t size;
} ChunkStruct;


/////////////////////////////////////////////////////////////////////////////
// int -> -1 : unknown ,  0 : false , 1 : true , 2 : timeout , 3 : authfail
// 
//////////////////////////////////////////////////////////////////////////////
// for firmware info
// 
// https://update.websamsung.net/FW/Update_FW_Info3.txt
//
// QND-6010R,NW_Camera,img,QND6010R_Series,qnd6010r_Series_4.01_210706.img,4.01_210706,NO,NO,NO,,9187880702fc44f5341978a0f0580aa7
// ① 모델명
// ② 제품군
// ③ Firmware 이미지 확장자
// ④ 대표 모델
// ⑤ Firmware 를 포함하는 ZIP (또는 img) 파일명
// ⑥ Full 버전 정보
typedef struct firmware_update_Info {
	bool update_check;
	int curl_responseCode;
	std::string model;
	std::string product;
	std::string extension;
	std::string representative_model;
	std::string fileName;
	std::string version_info;
} Firmware_Infos;


//////////////////////////////////////////////////////////////////////////////
// for 1. dashboard view  - need to bypass
// 
// http://<ip>/stw-cgi/eventstatus.cgi?msubmenu=eventstatus&action=check&SystemEvent=SDInsert
//  - das_presence
// http://<ip>/stw-cgi/eventstatus.cgi?msubmenu=eventstatus&action=check&SystemEvent=NASConnect
//  - nas_presence
//  
// http://<ip>/stw-cgi/system.cgi?msubmenu=storageinfo&action=view
//  - das_enable , das_recording_status , nas_enable , nas_recording_status
// 
// http://<ip>/stw-cgi/recording.cgi?msubmenu=storage&action=view (not use)
//  - das_enable , das_overwrite

typedef struct storage_Info {
	bool update_check;
	bool das_presence;
	bool sdfail;
	bool nas_presence;
	bool nasfail;
	bool das_enable;			// on or off of DAS recording
	bool nas_enable;			// on or off of NAS recording
	int curl_responseCode;
	std::string das_status;  // normal, Active, SDfailure, ... , timeout, authError
	std::string nas_status;  // normal, Active, failure, timeout, authError
} Storage_Infos;


//////////////////////////////////////////////////////////////////////////////
// for 2. deviceinfo view  - need to bypass
// 
// http://<IP>/stw-cgi/media.cgi?msubmenu=cameraregister&action=view
//  - Model, UserID , HTTPPort
// http://<IP>/stw-cgi/network.cgi?msubmenu=interface&action=view  : need to bypass
//  - LinkStatus , IPv4Address , MACAddess

typedef struct gateway_Info {
	int curl_responseCode;
	int WebPort;
	std::string Model;
	std::string IPv4Address;
	std::string MACAddress;
	std::string FirmwareVersion;
	std::string ConnectionStatus;
} GatewayInfo;

typedef struct channel_Info {
	bool update_check_deviceinfo;
	bool update_check_networkinterface;	
	bool IsBypassSupported;		
	int curl_responseCode;
	int ConnectionStatus;		// 0 : Disconnected , 1 : Success , 2 : ConnectFail , 
	int HTTPPort;	
	std::string ChannelStatus; // Disconnected , Success , ConnectFail
	std::string Model;			// camera model
	std::string DeviceName;		// 21.09.06 add - sub device name
	std::string LinkStatus;		// Connected , Disconnected
	std::string IPv4Address;
	std::string MACAddress;
	std::string FirmwareVersion;
	std::string UserID;			// admin
} Channel_Infos;


typedef struct firmware_version_Info {
	bool update_check;
	int curl_responseCode;
	std::string FirmwareVersion;
	std::string LatestFirmwareVersion;
} Firmware_Version_Infos;


struct ISUNAPIManagerObserver {
	virtual ~ISUNAPIManagerObserver() {};

	virtual void SendResponseToPeer(const std::string& topic, const std::string& message) = 0;
	virtual void SendResponseForDashboard(const std::string& topic, const std::string& message) = 0;
};

class sunapi_manager
{
public:
	sunapi_manager(const std::string& strDeviceID, int nWebPort);
	~sunapi_manager();

	void RegisterObserverForHbirdManager(ISUNAPIManagerObserver* callback);
	void SunapiManagerInit();

	bool GetNetworkInterfaceOfGateway();

	void ResetGatewayInfo();
	void ResetNetworkInterfaceOfGateway();
	void ResetDeviceInfoOfGateway();

	int GetMaxChannel();
	int GetConnectionCount();

	// For Test
	void TestDashboardView();
	void TestDeviceInfoView();
	void TestFirmwareVersionInfoView();

	void GetDataForDashboardAPI(const std::string& strTopic, const std::string& strPayload);

	// interface
	// command - checkPassword
	void CommandCheckPassword(const std::string& strTopic, const std::string& strPayload);

	// command - dashboard
	void GetDashboardView(const std::string& strTopic, json_t* json_strRoot);
	void GetDeviceInfoView(const std::string& strTopic, json_t* json_strRoot);
	void GetFirmwareVersionInfoView(const std::string& strTopic, json_t* json_strRoot);

	void UpdateFirmwareOfSubdevice(const std::string& strTopic, json_t* json_strRoot);

protected:
	// reset
	void ResetStorageInfos();
	void ResetStorageInfoForChannel(int channel);
	void ResetSubdeviceInfos();
	void ResetSubdeviceInfoForChannel(int channel);
	void ResetFirmwareVersionInfos();
	void ResetFirmwareVersionInfoForChannel(int channel);

	//update
	void UpdateStorageInfos();
	void UpdateSubdeviceInfos();
	void UpdateFirmwareVersionInfos();

	int GetDeviceIP_PW(std::string* strIP, std::string* strPW);

	static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp);
#if 0
	CURLcode CURL_Process(bool json_mode, bool ssl_opt, std::string strRequset, std::string strPW, std::string* strResult);
#else  // add timeout option
	CURLcode CURL_Process(bool json_mode, bool ssl_opt, int timeout, std::string strRequset, std::string strPW, std::string* strResult);
#endif

	bool ByPassSUNAPI(int channel, bool json_mode, const std::string IPAddress, const std::string devicePW, const std::string bypassURI, std::string* strResult, CURLcode* resCode);

	int GetMaxChannelByAttribute(std::string strID_PW);
	bool GetRegiesteredCameraStatus(const std::string deviceIP, const std::string devicePW, CURLcode* resCode);
	void SendErrorResponseForGateway(const std::string& strTopic, json_t* json_strRoot, std::string strError);

	/////////////////////////////////////////////////////////////////////////////////////////////
	// 1. dashboard view

	void GetDASPresenceOfSubdevice(int channel, const std::string deviceIP, const std::string devicePW);
	void GetNASPresenceOfSubdevice(int channel, const std::string deviceIP, const std::string devicePW);
	
	void GetStoragePresenceOfSubdevices();
	int ThreadStartForStoragePresence(int channel, const std::string deviceIP, const std::string devicePW);
	void thread_function_for_storage_presence(int channel, const std::string deviceIP, const std::string devicePW);

	void GetStorageStatusOfSubdevices();
	int ThreadStartForStorageStatus(int channel, const std::string deviceIP, const std::string devicePW);
	void thread_function_for_storage_status(int channel, const std::string deviceIP, const std::string devicePW);

	void GetDataForStorageInfo();


	void SendResponseForDashboardView(const std::string& strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid);
	
	void Set_update_checkForStorageInfo();
	void Reset_update_checkForStorageInfo();
	int ThreadStartSendResponseForDashboardView(const std::string strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid);

	void thread_function_for_send_response_for_dashboard(int maxChannel, const std::string strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid);


	/////////////////////////////////////////////////////////////////////////////////////////////
	// 2. deviceinfo view	

	// 21.09.06 add - sub device name
	bool GetDeviceNameOfSubdevices();
	int ThreadStartForSubDeviceInfo(int index, const std::string deviceIP, const std::string devicePW);
	void thread_function_for_subdevice_info(int index, const std::string deviceIP, const std::string devicePW);

	bool GetMacAddressOfSubdevices();
	int ThreadStartForNetworkInterface(int index, const std::string deviceIP, const std::string devicePW);
	void thread_function_for_network_interface(int index, const std::string deviceIP, const std::string devicePW);

	void GetDataForSubdeviceInfo();
	void SendResponseForDeviceInfoView(const std::string& strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid);

	void Set_update_checkForDeviceInfo();
	void Reset_update_checkForDeviceInfo();
	void Set_update_checkForNetworkInterface();
	void Reset_update_checkForNetworkInterface();
	int ThreadStartSendResponseForDeviceInfoView(const std::string strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid);
	void thread_function_for_send_response_for_deviceInfo(int maxChannel, const std::string strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid);


	/////////////////////////////////////////////////////////////////////////////////////////////
	// 3. firmware info view

	bool GetDeviceInfoOfGateway();

	bool GetFirmwareVersionFromText(std::string strText, std::string* strResult);

	int GetFirmwareVersionOfSubdevices();

	int ThreadStartForFirmwareVersion(int channel, const std::string deviceIP, const std::string devicePW);
	void thread_function_for_firmware_version(int channel, const std::string deviceIP, const std::string devicePW);

	bool GetLatestFirmwareVersionFromFile(std::string file_name);
	bool GetLatestFirmwareVersionFromURL(std::string update_FW_Info_url);

	void GetDataForFirmwareVersionInfo();
		
	int ThreadStartGetLatestFirmwareVersionFromURL();
	void thread_function_for_get_latestFirmwareVersion();

	void SendResponseForFirmwareView(const std::string& strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid);

	void Set_update_check_Firmware_Ver_info_ForFirmwareVersion();
	void Reset_update_check_Firmware_Ver_info_ForFirmwareVersion();

	int ThreadStartSendResponseForFirmwareView(const std::string strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid);
	void thread_function_for_send_response_for_firmwareVersion(int maxChannel, const std::string strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid);


	// 4. firmware update
	void SendResponseForUpdateFirmware(const std::string& strTopic, std::string strCommand, std::string strType, std::string strView, std::string strTid,
										int resCode, int rawCode, std::vector<int> updateChannel);


	/////////////////////////////////////////////////////////////////////////////////////////////
	// for update
	int ThreadStartForUpdateInfos(int second);
	void thread_function_for_update_infos(int second);

	void UpdateDataForDashboardView();


private:
	int g_Max_Channel;
	std::string g_Device_id;

	std::string g_StrDeviceIP;
	std::string g_StrDevicePW;
	int g_WebPort;

	int g_Sub_camera_reg_Cnt;
	int g_Sub_network_info_Cnt;
	int g_Sub_device_info_Cnt;

	time_t g_UpdateTimeOfStoragePresence;
	time_t g_UpdateTimeOfStorageStatus;  // for 1. dashboard - storage status

	time_t g_UpdateTimeOfRegistered;	
	time_t g_UpdateTimeOfDeviceInfo;
	time_t g_UpdateTimeOfNetworkInterface;  // for 2. device info

	time_t g_UpdateTimeForFirmwareVersionOfSubdevices; // for 3. firmware version
	 
	time_t g_UpdateTimeForDashboardAPI;
	time_t g_UpdateTimeOut;

	ISUNAPIManagerObserver* observerForHbirdManager;

	// firmware version info
	std::vector< firmware_update_Info> g_vecFirmwareInfos;
};

