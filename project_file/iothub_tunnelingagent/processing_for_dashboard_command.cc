#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <fstream>
#include <chrono>
#include <vector>

#include "processing_for_dashboard_command.h"

#include "curl/curl.h"
#include "win_time.h"
#include <bitset>

using namespace std;
using std::this_thread::sleep_for;

#define TEST_DEVICE_ADDRESS "192.168.11.5"  // for my device

// Default timeout is 0 (zero) which means it never times out during transfer.
#define CURL_OPT_TIMEOUT 10  // 2018.11.30 : 3 -> 10

#define REMOTE_INFO_FILE "config/remote_info.txt"

#if 0
// QA
#define DEFAULT_CONFIG "{\"url\":\"https://config.qa.htw2.hbird-iot.com:443/v1.5/\",\"config_path\":\"config/model/device\",\"bearer\":\"authorization: bearer \"}"
#define DEVICE_TOKEN_API "{\"url\":\"https://auth.qa.htw2.hbird-iot.com/v1.5/\",\"key_path\":\"device/key/type/cloudGateway\",\"bearer\":\"authorization: bearer \"}"
#else
// DEV
#define DEFAULT_CONFIG "{\"url\":\"https://config.dev.htw2.hbird-iot.com:443/v1.5/\",\"config_path\":\"config/model/device\",\"bearer\":\"authorization: bearer \"}"
#define DEVICE_TOKEN_API "{\"url\":\"https://auth.dev.htw2.hbird-iot.com/v1.5/\",\"key_path\":\"device/key/type/cloudGateway\",\"bearer\":\"authorization: bearer \"}"
#endif

// for debug
#ifdef _DEBUG
#define SUNAPI_DEBUG
#endif
#define API_DEBUG

// for SUNAPI Command
struct MemoryStruct {
	char* memory;
	size_t size;
};

ProcessingForDashboardCommand::ProcessingForDashboardCommand(){

	printf("[hwanjang] ProcessingForDashboardCommand -> constructor !!!\n");
	gathering_count_ = 0;
	total_index = 1;

	observerForHbirdManager = nullptr;
}

ProcessingForDashboardCommand::~ProcessingForDashboardCommand() {
	printf("[hwanjang] ProcessingForDashboardCommand::~ProcessingForDashboardCommand() -> Destructor !!!\n");

}

void ProcessingForDashboardCommand::RegisterObserverForHbirdManager(IProcessingForDashboardCommandObserver* callback)
{
	observerForHbirdManager = callback;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// for 

size_t ProcessingForDashboardCommand::WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
   size_t realsize = size * nmemb;
   struct MemoryStruct *mem = (struct MemoryStruct *)userp;
     
   mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);

   if (mem->memory != NULL)
   {
	   memcpy(&(mem->memory[mem->size]), contents, realsize);
	   mem->size += realsize;
	   mem->memory[mem->size] = 0;
   }   
   else
   {
	   /* out of memory! */
	   printf("not enough memory (realloc returned NULL)\n");
	   return 0;
   }

//printf("[hwanjang] WriteMemoryCallback() -> realsize : %d\n-> %s\n", realsize, mem->memory);

   return realsize;
}

///////////////////////////////////////////////////////////////////////////////
//SUNAPI
///////////////////////////////////////////////////////////////////////////////

int ProcessingForDashboardCommand::file_exit(std::string& filename)
{
	FILE* file;
	if ((file = fopen(filename.c_str(), "r")))
	{
		fclose(file);
		return 1;
	}
	return 0;
}

size_t ProcessingForDashboardCommand::WriteMemoryCallbackForHeader(void* contents, size_t size, size_t nmemb, void* userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct* mem = (struct MemoryStruct*)userp;

	mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory == NULL) {
		/* out of memory! */
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	//printf("[hwanjang] WriteMemoryCallback() -> realsize : %d\n-> %s\n", realsize, mem->memory);

	return realsize;
}

// thread
/////////////////////////////////////////////////////////////////////////////////////////////
// for gathering test

bool ProcessingForDashboardCommand::gathering_command(const std::string& strTopic, json_t* root, int type)
{
	json_t* gathering_cmd = root;

	char* strCommand, * strType, * strTid;
	int result = json_unpack(root, "{s:s, s:s, s:s}", "command", &strCommand, "type", &strType, "tid", &strTid);

	if (result)
	{
		printf("[hwanjang] Error !! tunneling_command -> json root is wrong -> return  !!!\n");
		return false;
	}		

	//////////////////////////////////////////////////////////////////////
	// root -> json_t* message  
	json_t* objMessage = json_object_get(root, "message");
	if (!json_is_object(objMessage))
	{
		printf("[hwanjang] Error !! fail to get JSON value from sub message !!!!\n");
		return false;
	}

	json_t* jmsg_headers = json_object_get(objMessage, "headers");

	char* strMethod, *strUrl, *strBody;

#if 0 // option #1 - check null
	json_t* jmsg_body = json_object_get(objMessage, "body");
	if (json_is_null(jmsg_body))
	{
		result = json_unpack(root, "{s{s:s, s:s}}", "message", "method", &strMethod, "url", &strUrl);
		g_strBody_.empty();
	}
	else
	{
		result = json_unpack(root, "{s{s:s, s:s, s:s}}", "message", "method", &strMethod, "url", &strUrl, "body", &strBody);

		printf("[hwanjang] Tunneling param , body : %s \n", strBody);

		g_strBody_ = strBody;
	}

	if (result)
	{
		printf("[hwanjang] Error !! get message data !!!!\n");
		return false;
	}

#else  // option #2
	result = json_unpack(root, "{s{s:s, s:s}}", "message", "method", &strMethod, "url", &strUrl);

	if (result)
	{
		printf("[hwanjang] Error !! get message datas , method or url !!!!\n");
		return false;
	}

	if (!strncmp("POST", strMethod, 4))
	{
		result = json_unpack(root, "{s{s:s}}", "message", "body", &strBody);

		if (result)
		{
			printf("[hwanjang] Error !! get body data !!!!\n");
			return false;
		}

		printf("[hwanjang] Tunneling param , body : %s\n", strBody);
	}
#endif

	g_strCommand_ = strCommand;
	g_strType_ = strType;
	g_strTid_ = strTid;
	g_strMethod_ = strMethod;
	g_strUrl_ = strUrl;
		
	std::cout << "gathering_command() -> type : " << strType << std::endl;

	for (int i = 0; i < total_index; i++)
	{
		json_t* j_headers = json_deep_copy(jmsg_headers);
		ThreadStartForGagthering(strTopic, j_headers, type, i);
	}
}

int ProcessingForDashboardCommand::ThreadStartForGagthering(const std::string& strTopic, json_t* root, int type, int index)
{
	std::thread thread_function_th([=] { processing_gathering(strTopic, root, type, index); });
	thread_function_th.detach();

	return 0;
}

bool ProcessingForDashboardCommand::processing_gathering(const std::string& strTopic, json_t* jmsg_headers, int type, int index)
{
	printf("ProcessingForDashboardCommand::processing_gathering() -> index : %d\n", index);

	int result = 0;
	// root -> command, type, tid

#if 0
	char* strCommand, * strType, * strTid;
	result = json_unpack(root, "{s:s, s:s, s:s}", "command", &strCommand, "type", &strType, "tid", &strTid);

	if (result)
	{
		printf("[hwanjang] Error !! processing_gathering -> json root is wrong -> return  !!!\n");
		return false;
	}

	//////////////////////////////////////////////////////////////////////
	// root -> json_t* message  
	json_t* objMessage = json_object_get(root, "message");
	if (!json_is_object(objMessage))
	{
		printf("[hwanjang] Error !! fail to get JSON value from sub message !!!!\n");
		return false;
	}

	json_t* jmsg_headers = json_object_get(objMessage, "headers");

	char* strMethod, * strUrl, * strBody;

#if 1 // option #1 - check null
	json_t* jmsg_body = json_object_get(objMessage, "body");
	if (json_is_null(jmsg_body))
	{
		result = json_unpack(root, "{s{s:s, s:s}}", "message", "method", &strMethod, "url", &strUrl);
	}
	else
	{
		result = json_unpack(root, "{s{s:s, s:s, s:s}}", "message", "method", &strMethod, "url", &strUrl, "body", &strBody);

		printf("[hwanjang] Tunneling param , body : %s \n", strBody);
	}

	if (result)
	{
		printf("[hwanjang] Error !! get message data !!!!\n");
		return false;
	}

#else  // option #2
	result = json_unpack(root, "{s{s:s, s:s}}", "message", "method", &strMethod, "url", &strUrl);

	if (result)
	{
		printf("[hwanjang] Error !! get message datas , method or url !!!!\n");
		return;
	}

	if (!strncmp("POST", strMethod, 4))
	{
		result = json_unpack(root, "{s{s:s}}", "message", "body", &strBody);

		if (result)
		{
			printf("[hwanjang] Error !! get body data !!!!\n");
			return;
		}

		printf("[hwanjang] Tunneling param , body : %s\n", strMethod, strUrl);
	}
#endif

#endif

	//printf("[hwanjang] Tunneling param , method : %s , url : %s\n", strMethod, strUrl);
	printf("[hwanjang] Tunneling param , method : %s , url : %s\n", g_strMethod_.c_str(), g_strUrl_.c_str());

	// tunneling

	CURL* curl_handle;
	struct MemoryStruct bodyChunk, headerChunk;

	bodyChunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
	bodyChunk.size = 0;    /* no data at this point */

	headerChunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
	headerChunk.size = 0;    /* no data at this point */

	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();

	if (curl_handle == NULL)
	{
		printf("error ... initialize cURL !!!\n");
		curl_easy_cleanup(curl_handle);
		/* we're done with libcurl, so clean it up */
		curl_global_cleanup();
		return false;
	}

	struct curl_slist* headerList = NULL;


	// --------------------- Requset PARSERED Headers Start -------------------------------
#if 0
	std::string strHost, strProtocol, strPort;
	std::string  headerItem;

	for (Json::Value::const_iterator itr = jmsg_headers.begin(); itr != jmsg_headers.end(); itr++) {
		headerItem.clear();
		// if(method.asString() == "POST")
		std::cout << "    " << itr.name().c_str() << " : " << jmsg_headers[itr.name()].asString() << std::endl;

		if (itr.name() == "host") {
			strHost = jmsg_headers[itr.name()].asString();
		}
		else {
			if (itr.name() == "x-forwarded-proto")
				strProtocol = jmsg_headers[itr.name()].asString();
			if (itr.name() == "x-forwarded-port")
				strPort = jmsg_headers[itr.name()].asString();

			if (itr.name() != "accept-encoding") {
				headerItem = itr.name();
				headerItem.append(" : ");
				headerItem.append(jmsg_headers[itr.name()].asString());
				headerList = curl_slist_append(headerList, headerItem.c_str());

				printf("header item : %s\n", headerItem.c_str());
			}
		}
	}
#else
	///////////////////////////////////////////////////////////////////////////
	// JANSSON	
	std::string strHost, strProtocol, strPort;
	std::string  headerItem;

	json_t* value;
	const char* key;

	json_object_foreach(jmsg_headers, key, value) {
		printf("key: %s", key);
		if (!strncmp("host", key, 4))
		{
			strHost = json_string_value(value);
		}
		else if (!strncmp("x-forwarded-proto", key, 17))
		{
			strProtocol = json_string_value(value);
		}
		else if (!strncmp("x-forwarded-port", key, 16))
		{
			strPort = json_string_value(value);
		}
		else if (strncmp("accept-encoding", key, 15))
		{
			headerItem = key;
			headerItem.append(" : ");
			headerItem.append(json_string_value(value));
			headerList = curl_slist_append(headerList, headerItem.c_str());

			printf("header item : %s\n", headerItem.c_str());
		}
		else
		{
			printf("Unnecessary data ... key : %s , value : %s\n", key, json_string_value(value));
		}
	}
#endif
	// --------------------- Requset PARSERED Headers End -------------------------------
	std::string strRepuestPort = "443";

	if (strProtocol != "https")
	{
		strRepuestPort.clear();
		strRepuestPort = "80";
	}

	// tunneling
	std::string strRequestUrl, strRepuestHost, strUserPW;

#if 1  //  for web team test

	std::string strIP, strPW;
	std::string sunapi_info_file = "config/camera.cfg";

	json_error_t j_error;
	json_t* json_out = json_load_file(sunapi_info_file.c_str(), 0, &j_error);
	if (j_error.line != -1)
	{
		printf("[hwanjang] Error !! get camera info !!!\n");
		printf("json_load_file returned an invalid line number -- > CFG file check !!\n");

		strIP = "127.0.0.1";
		strPW = "admin:5tkatjd!";
	}
	else
	{
#if 1
		char* SUNAPIServerIP, * devicePW;
		result = json_unpack(json_out, "{s:s, s:s}", "ip", &SUNAPIServerIP, "pw", &devicePW);

		if (result || !SUNAPIServerIP || !devicePW)
		{
			printf("[hwanjang] Error !! camera cfg is wrong -> retur false !!!\n");

			strIP = "127.0.0.1";
			strPW = "admin:5tkatjd!";
		}
		else
		{
			strIP = SUNAPIServerIP;
			strPW = devicePW;
		}
#else
		// SUNAPI Server IP
		json_t* ip_p = NULL;
		ip_p = json_object_get(json_out, "ip");

		if (!ip_p || !json_is_string(ip_p))
		{
			json_decref(json_out);
			return false;
		}

		int size_ip = json_string_length(ip_p);
		char* strIPAddress = new char[size_ip];

		strcpy(strIPAddress, json_string_value(ip_p));

		strIP = strIPAddress;

		// device pw
		json_t* pw_p = NULL;
		pw_p = json_object_get(json_out, "pw");

		if (!pw_p || !json_is_string(pw_p))
		{
			json_decref(json_out);
			return false;
		}

		int size_pw = json_string_length(pw_p);
		char* strDevPW = new char[size_pw];

		strcpy(strDevPW, json_string_value(pw_p));

		strPW = strDevPW;
#endif

	}
	json_decref(json_out);

	strRepuestHost = strIP;

	strRequestUrl = strProtocol;
	strRequestUrl.append("://");
	strRequestUrl.append(strRepuestHost);
	strRequestUrl.append(":");
	strRequestUrl.append(strRepuestPort);
	//strRequestUrl.append(strUrl);  // strUrl is equal to jmsg_path.asString()
	strRequestUrl.append(g_strUrl_);  // strUrl is equal to jmsg_path.asString()

	strUserPW = strPW;
#endif

	printf("[hwanjang] Request URL : %s\n", strRequestUrl.c_str());

	/*
	  200508 ±ÇÇõÁø
	  resolve_str
	  ex) server0.htwda03u1803001526.dev.htw.hbird-iot.com:52061:211.221.81.152
	*/
	std::string strResolve = "server0.";
	strResolve.append(strHost);
	strResolve.append(":");
	strResolve.append(strRepuestPort);
	strResolve.append(":");
	strResolve.append(strRepuestHost);

	std::cout << "resolve_str : " << strResolve << std::endl;

	/*
	  200508 ±ÇÇõÁø
	  connect_to_str
	  ex) htwda03u1803001526.dev.htw.hbird-iot.com:80:server0.htwda03u1803001526.dev.htw.hbird-iot.com:52061
	*/
	char connect_to_str[1024] = { 0, };
	std::string strConnection = strHost;
	strConnection.append(":");
	strConnection.append(strPort);
	strConnection.append(":");
	strConnection.append("server0.");
	strConnection.append(strHost);
	strConnection.append(":");
	strConnection.append(strRepuestPort);

	std::cout << "connect_to_str : " << strConnection << std::endl;

	struct curl_slist* resolve_slist = NULL;
	resolve_slist = curl_slist_append(NULL, strResolve.c_str());
	struct curl_slist* connect_to_slist = NULL;
	connect_to_slist = curl_slist_append(NULL, strConnection.c_str());

	curl_easy_setopt(curl_handle, CURLOPT_RESOLVE, resolve_slist);
	curl_easy_setopt(curl_handle, CURLOPT_CONNECT_TO, connect_to_slist);
	curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, NULL);

	//headerList = curl_slist_append(headerList, "Accept: application/json");
	//curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headerList);
	//curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "GET");

	//curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, strMethod);
	curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, g_strMethod_.c_str());


	//if (!strncmp("POST", strMethod, 4))
	if (g_strMethod_ == "POST")
	{
		std::string bodyMsg = g_strBody_;
		headerList = curl_slist_append(headerList, "Expect:");
		headerList = curl_slist_append(headerList, "content-length:");
		headerList = curl_slist_append(headerList, "content-type:");

		if (bodyMsg.length() > 0)
		{
			curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, bodyMsg.c_str());
			curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, bodyMsg.size());
		}
	}

	//headerList = curl_slist_append(headerList, "Accept: application/json");

	// header List
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headerList);

	curl_easy_setopt(curl_handle, CURLOPT_URL, strRequestUrl.c_str());
	curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST); // digest authentication
	curl_easy_setopt(curl_handle, CURLOPT_USERPWD, strUserPW.c_str());

	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, true);

	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);

	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&bodyChunk);

	curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, WriteMemoryCallbackForHeader);
	curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void*)&headerChunk);

	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, CURL_OPT_TIMEOUT);

	CURLcode res = curl_easy_perform(curl_handle);

	std::string res_str;

	json_t* mqtt_MainMsg, * objSub;

	mqtt_MainMsg = json_object();
	objSub = json_object();

	/* check for errors */
	if (res != CURLE_OK) {
		printf("ProcessingForDashboardCommand::processing_gathering() -> curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	}
	else
	{
		/*
		 * Now, our chunk.memory points to a memory block that is chunk.size
		 * bytes big and contains the remote file.
		 *
		 * Do something nice with it!
		 */
		long response_code;
		curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
		printf("response code : %d\n", response_code);

		//sub_Msg["statusCode"] = res;
		json_t* intRes = json_integer(response_code);
		json_object_set(objSub, "statusCode", intRes);

#if 1	// debug
		printf("headerChunk size : %lu bytes \n", (long)headerChunk.size);
		printf("headerChunk data : %s\n", headerChunk.memory);

		printf("bodyChunk size : %lu bytes \n", (long)bodyChunk.size);
		printf("bodyChunk data : %s\n", bodyChunk.memory);
#endif

		// Header
		std::string responseHeader = headerChunk.memory;

		//mqtt_Msg["command"] = strCommand;
		json_object_set(mqtt_MainMsg, "command", json_string(g_strCommand_.c_str()));
		//mqtt_Msg["type"] = "response";
		json_object_set(mqtt_MainMsg, "type", json_string("response"));
		//mqtt_Msg["message"] = sub_Msg;
		json_object_set(mqtt_MainMsg, "message", objSub);
		//mqtt_Msg["tid"] = strTid;
		json_object_set(mqtt_MainMsg, "tid", json_string(g_strTid_.c_str()));

		int cnt = 0;
#if 0
		std::string hsonProtocol = "HSON0001";
		std::vector<unsigned char> msgProtocolByte(hsonProtocol.length(), 0);

		std::copy(hsonProtocol.begin(), hsonProtocol.end(), msgProtocolByte.begin());
#else
		std::string hsonProtocol = "HSON0001";

		typedef vector<unsigned char> bufferType;
		bufferType msgProtocolByte(8);

		std::copy(hsonProtocol.begin(), hsonProtocol.end(), msgProtocolByte.begin());
#endif

		// MQTT Message size
		std::vector<unsigned char> mqttMsgBorderlineByte;

		//std::string strMQTTMsg = writer.write(mqtt_Msg);		
		std::string strMQTTMsg = json_dumps(mqtt_MainMsg, 0);

		printf("strMQTTMsg size : %lu\n%s\n", strMQTTMsg.size(), strMQTTMsg.c_str());

		//uint32_t mqttMsgBorderline = strMQTTMsg.size() + msgProtocolByte.size() + 4; // 4 is mqttMsgBorderlineByte size
		uint32_t mqttMsgBorderline = strMQTTMsg.size() + 8 + 4; // 4 is mqttMsgBorderlineByte size

		printf("mqttMsgBorderline size : %d\n", mqttMsgBorderline);

#if 0
		mqttMsgBorderlineByte.push_back(static_cast<unsigned char>((mqttMsgBorderline & 0xFF000000) >> 24));
		cout << "1. mqttMsgBorderlineByte : " << bitset<8>(static_cast<unsigned char>((mqttMsgBorderline & 0xFF000000) >> 24)) << endl;
		mqttMsgBorderlineByte.push_back(static_cast<unsigned char>((mqttMsgBorderline & 0x00FF0000) >> 16));
		cout << "2. mqttMsgBorderlineByte : " << bitset<8>(static_cast<unsigned char>((mqttMsgBorderline & 0x00FF0000) >> 16)) << endl;
		mqttMsgBorderlineByte.push_back(static_cast<unsigned char>((mqttMsgBorderline & 0x0000FF00) >> 8));
		cout << "3. mqttMsgBorderlineByte : " << bitset<8>(static_cast<unsigned char>((mqttMsgBorderline & 0x0000FF00) >> 8)) << endl;
		mqttMsgBorderlineByte.push_back(static_cast<unsigned char>(mqttMsgBorderline & 0x000000FF));
		cout << "4. mqttMsgBorderlineByte : " << bitset<8>(static_cast<unsigned char>((mqttMsgBorderline & 0x000000FF))) << endl;
#else
		bitset<32> bsMqttMsg(mqttMsgBorderline);
		std::string strbsMqttMsg = bsMqttMsg.to_string();
		mqttMsgBorderlineByte.resize(strbsMqttMsg.length());

		cout << "bitset mqttMsgBorderline size : " << strbsMqttMsg.length() << ", " << strbsMqttMsg.c_str() << endl;

		std::copy(strbsMqttMsg.begin(), strbsMqttMsg.end(), mqttMsgBorderlineByte.begin());

		//mqttMsgBorderlineByte.insert(mqttMsgBorderlineByte.end(), strbsMqttMsg.begin(), strbsMqttMsg.end());
		
#endif
		msgProtocolByte.insert(msgProtocolByte.end(), mqttMsgBorderlineByte.begin(), mqttMsgBorderlineByte.end());

		cout << "1. msgProtocolByte size : " << msgProtocolByte.size() << endl << &msgProtocolByte[0] << endl;

		std::string stringMSgProtocolByte((char*)&msgProtocolByte[0], msgProtocolByte.size());

		char* chMsgProtocolByte = new char[msgProtocolByte.size()];

		strncpy(chMsgProtocolByte, (char*)&msgProtocolByte[0], msgProtocolByte.size());

		cout << " --> to string ..." << endl << stringMSgProtocolByte << endl;
		cout << " --> to char* ..." << endl << chMsgProtocolByte << endl;


		// MQTT Message
		std::vector<unsigned char> mqttMsgByte(strMQTTMsg.length(), 0);
		std::copy(strMQTTMsg.begin(), strMQTTMsg.end(), mqttMsgByte.begin());

		msgProtocolByte.insert(msgProtocolByte.end(), mqttMsgByte.begin(), mqttMsgByte.end());

		cout << "2. msgProtocolByte : " << msgProtocolByte.size() << endl << &msgProtocolByte[0] << endl;

		// response header size
		std::vector<unsigned char> headerBorderlineByte;
		uint32_t headerBorderline = mqttMsgBorderline + responseHeader.size() + 4; // 4 is headerBorderlineByte size

		printf("headerBorderline size : %d\n", headerBorderline);

#if 0
		headerBorderlineByte.push_back(static_cast<unsigned char>((headerBorderline & 0xFF000000) >> 24));
		headerBorderlineByte.push_back(static_cast<unsigned char>((headerBorderline & 0x00FF0000) >> 16));
		headerBorderlineByte.push_back(static_cast<unsigned char>((headerBorderline & 0x0000FF00) >> 8));
		headerBorderlineByte.push_back(static_cast<unsigned char>(headerBorderline & 0x000000FF));
#else
		bitset<32> bsHeader(headerBorderline);
		std::string strbsHeader = bsHeader.to_string();
		headerBorderlineByte.resize(strbsMqttMsg.length());

		cout << "bitset headerBorderline size : " << strbsHeader .length() << ", " << strbsHeader.c_str() << endl;		

		std::copy(strbsHeader.begin(), strbsHeader.end(), headerBorderlineByte.begin());
		//headerBorderlineByte.insert(headerBorderlineByte.end(), strbsHeader.begin(), strbsHeader.end());
#endif

		msgProtocolByte.insert(msgProtocolByte.end(), headerBorderlineByte.begin(), headerBorderlineByte.end());

		cout << "3. msgProtocolByte : " << msgProtocolByte.size() << endl << &msgProtocolByte[0] << endl;

		// response header
		std::vector<unsigned char> responseHeaderByte(responseHeader.length(), 0);
		std::copy(responseHeader.begin(), responseHeader.end(), responseHeaderByte.begin());

		printf("responseHeader.size : %lu\n%s\n", responseHeader.length(), responseHeader.c_str());

		msgProtocolByte.insert(msgProtocolByte.end(), responseHeaderByte.begin(), responseHeaderByte.end());

		cout << "4. msgProtocolByte : " << msgProtocolByte.size() << endl << &msgProtocolByte[0] << endl;

		// response body
		//std::vector<byte> responseBodyByte(responseBody.length(), 0);
		//std::copy(responseBody.begin(), responseBody.end(), responseBodyByte.begin());

		//printf("responseBody.size : %d\n%s\n", responseBody.length(), responseBody.c_str());
		//msgProtocolByte.insert(msgProtocolByte.end(), responseBodyByte.begin(), responseBodyByte.end());

		// body
		unsigned char* byteResponseBody = (unsigned char*)bodyChunk.memory;
		// response body
		std::vector<unsigned char> responseBodyByte(byteResponseBody, byteResponseBody + bodyChunk.size);

		msgProtocolByte.insert(msgProtocolByte.end(), responseBodyByte.begin(), responseBodyByte.end());

		cout << "5. msgProtocolByte : " << msgProtocolByte.size() << endl << &msgProtocolByte[0] << endl;

		//std::string sendMsg(reinterpret_cast<const char*>(&msgProtocolByte[0]), msgProtocolByte.size());
		//printf("[hwanjang] ProcessingForDashboardCommand::processing_gathering() response ---> sendMsg size: %d\n%s\n", sendMsg.size() , sendMsg.c_str());

		printf("[hwanjang] ProcessingForDashboardCommand::processing_gathering() response ---> msgProtocolByte size: %lu\n", msgProtocolByte.size());

		//gatheringResponseForTunneling(index, strTopic, &msgProtocolByte[0]);

#if 0
		json_t* subMsg;

		subMsg = json_object();

		json_t* jt_index = json_integer(index);
		json_object_set(subMsg, "index", jt_index);

		//json_t* strPayload = json_object();
		//int result_value = json_string_setn(strPayload, (char*)&msgProtocolByte[0], msgProtocolByte.size());

		char* stdMSgProtocolByte = (char*)&msgProtocolByte[0];
		std::string utf8_payload(msgProtocolByte.begin(), msgProtocolByte.end());
		printf("[hwanjang] ProcessingForDashboardCommand::processing_gathering() response ---> utf8_payload size: %d\n", utf8_payload.size());


		json_t* strPayload = json_stringn_nocheck(stdMSgProtocolByte, msgProtocolByte.size());
		json_object_set(subMsg, "payload", strPayload);
		//json_object_set_nocheck(subMsg, "payload", strPayload);
#else

		char* chrMSgProtocolByte = (char*)&msgProtocolByte[0];

		cout << "chrMSgProtocolByte size : " << strlen(chrMSgProtocolByte) << endl << ", " <<endl << chrMSgProtocolByte << endl;

		std::string strMSgProtocolByte((char*)&msgProtocolByte[0], msgProtocolByte.size());

		cout << "strMSgProtocolByte size : " << strMSgProtocolByte.length() << endl << ", " << endl << strMSgProtocolByte.c_str() << endl;

		json_t* subMsg = json_pack("{}");

		json_error_t error_t;
		/* Build the JSON object {"foo": 42, "bar": 7} */
		/* s# : Build a string from a non-null terminated buffer */
		subMsg = json_pack_ex(&error_t,0, "{s:i,s:s#}", "index", index, "payload", strMSgProtocolByte.c_str(), msgProtocolByte.size());

#endif

		
		//printf("[hwanjang] ProcessingForDashboardCommand::processing_gathering() ---> strPayload size: %d\n", json_string_length(strPayload));
		printf("[hwanjang] ProcessingForDashboardCommand::processing_gathering() ---> subMsg json_object_size size: %d\n", json_object_size(subMsg));

#if 1
		size_t sub_size = json_dumpb(subMsg, NULL, 0, 0);
		if (sub_size == 0)
		{
			return false;
		}

		char* buff = new char[sub_size];
		sub_size = json_dumpb(subMsg, buff, sub_size, 0);

		std::string strMQTTSubMsg(buff, sub_size);

		// response body
		std::vector<unsigned char> byteResponseBuff(sub_size, 0);
		std::copy(strMQTTSubMsg.begin(), strMQTTSubMsg.end(), byteResponseBuff.begin());

		Send_GatheringData(strTopic, &byteResponseBuff[0], byteResponseBuff.size());

#else
		std::string strMQTTSubMsg = json_dumps(subMsg, 0);

		// response body
		std::vector<unsigned char> byteResponseBuff(strMQTTSubMsg.size(), 0);
		std::copy(strMQTTSubMsg.begin(), strMQTTSubMsg.end(), byteResponseBuff.begin());

		Send_GatheringData(strTopic, &byteResponseBuff[0], byteResponseBuff.size());
#endif




		//printf("[hwanjang] ProcessingForDashboardCommand::processing_gathering() ---> send message size: %d\n", strResponseMsg.size());
		//Send_GatheringData(strTopic, strResponseMsg);
	}

	curl_slist_free_all(headerList);
	curl_slist_free_all(resolve_slist);
	curl_slist_free_all(connect_to_slist);

	curl_easy_cleanup(curl_handle);

	free(bodyChunk.memory);
	free(headerChunk.memory);

	/* we're done with libcurl, so clean it up */
	curl_global_cleanup();

	return true;
}


void ProcessingForDashboardCommand::gatheringResponseForTunneling(int index, const std::string& topic, const void* payload)
{
#if 0
	// tid , payload

	json_t* mqtt_MainMsg, * objSub;

	mqtt_MainMsg = json_object();
	objSub = json_object();

	//sub_Msg["statusCode"] = res;
	json_t* intRes = json_integer(index);
	json_object_set(objSub, "index", intRes);

	json_t* strPayload = json_string((char*)payload);
	json_object_set(objSub, "payload", strPayload);

	std::vector<json_t*> mqttMsgResponse;
	mqttMsgResponse.insert(mqttMsgResponse.end(), objSub);

	std::string strResponseMsg = json_dumps(objSub, 0);

	Send_GatheringData(topic, strResponseMsg);
#endif
}

void ProcessingForDashboardCommand::Send_GatheringData(const std::string& topic, const void* payload, int payload_size)
{
	std::lock_guard<std::mutex> gathering_lock(g_mutex_lock_);

	// payload
	unsigned char* byteResponsePayload = (unsigned char*)payload;
	// response body
	std::vector<unsigned char> responseBodyByte(byteResponsePayload, byteResponsePayload + payload_size);

	mqttMsgResponse.insert(mqttMsgResponse.end(), responseBodyByte.begin(), responseBodyByte.end());
	
	gathering_count_++;

	if (gathering_count_ < total_index)
	{
		printf("ProcessingForDashboardCommand::Send_GatheringData() -> gathering_count_ : %d ... gathering ...\n", gathering_count_);
		printf("--> payload_size : %d ,  mqttStringMsgResponse.size : %d\n", payload_size, mqttStringMsgResponse.size());
		return;
	}

	printf("ProcessingForDashboardCommand::Send_GatheringData() -> gathering_count_ : %d ... end ... MQTT msg size : %d\n", gathering_count_, mqttMsgResponse.size());
	
	////////////////////////////////////////////////////////////////////////////////
	json_t* mqtt_MainMsg, * objSub;

	mqtt_MainMsg = json_object();
	objSub = json_object();

	//mqtt_Msg["command"] = strCommand;
	json_object_set(mqtt_MainMsg, "command", json_string(g_strCommand_.c_str()));
	//mqtt_Msg["type"] = "response";
	json_object_set(mqtt_MainMsg, "type", json_string("response"));
	//mqtt_Msg["message"] = sub_Msg;
	//json_object_set(mqtt_MainMsg, "message", json_string(&mqttMsgResponse[0]));
	json_t* strPayload = json_stringn_nocheck((char*)&mqttMsgResponse[0], mqttMsgResponse.size());
	json_object_set(mqtt_MainMsg, "message", strPayload);
	 
	//mqtt_Msg["tid"] = strTid;
	json_object_set(mqtt_MainMsg, "tid", json_string(g_strTid_.c_str()));

	// MQTT Message
#if 0
	std::string strMQTTMsg = json_dump(mqtt_MainMsg, 0);

	std::vector<unsigned char> mqttMsgByte(strMQTTMsg.length(), 0);
	std::copy(strMQTTMsg.begin(), strMQTTMsg.end(), mqttMsgByte.begin());
#else
	size_t main_size = json_dumpb(mqtt_MainMsg, NULL, 0, 0);

	char* buff = new char[main_size];
	main_size = json_dumpb(mqtt_MainMsg, buff, main_size, 0);
	std::string strMQTTMsg(buff, main_size);

	// response main body
	std::vector<unsigned char> byteResponseBuff(main_size, 0);

	std::copy(strMQTTMsg.begin(), strMQTTMsg.end(), byteResponseBuff.begin());

#endif
	printf("Send MQTT size : %d, msg : \n%s\n", byteResponseBuff.size(), &byteResponseBuff[0]);

	observerForHbirdManager->SendResponseToPeerForTunneling(topic, &byteResponseBuff[0], byteResponseBuff.size());
}

