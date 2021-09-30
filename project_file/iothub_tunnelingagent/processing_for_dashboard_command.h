#ifndef PROCESSING_FOR_DASHBOARD_COMMAND_H_
#define PROCESSING_FOR_DASHBOARD_COMMAND_H_
#pragma once // 2017.11.22 hwanjang
#include <string>
#include <mutex>

#include "jansson.h"

struct IProcessingForDashboardCommandObserver {
  virtual ~IProcessingForDashboardCommandObserver() {};

  virtual void SendResponseToPeerForTunneling(const std::string& topic, const void* payload, int size) = 0;
};

class ProcessingForDashboardCommand {
	public:
		ProcessingForDashboardCommand();
		~ProcessingForDashboardCommand();

		void RegisterObserverForHbirdManager(IProcessingForDashboardCommandObserver* callback);

		bool gathering_command(const std::string& strTopic, json_t* root, int type);  // 0 : normal , 1 : devicestatus

	private:

		// SUNAPI
		int file_exit(std::string& filename);

		static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
		static size_t WriteMemoryCallbackForHeader(void* contents, size_t size, size_t nmemb, void* userp);

		int ThreadStartForGagthering(const std::string& strTopic, json_t* root, int type, int index);
		bool processing_gathering(const std::string& strTopic, json_t* root, int type, int index);  // 0 : normal , 1 : devicestatus

		void gatheringResponseForTunneling(int index, const std::string& topic, const void* payload);
		void Send_GatheringData(const std::string& topic, const void* payload, int payload_size);

		IProcessingForDashboardCommandObserver* observerForHbirdManager;

		std::string g_strCommand_;
		std::string g_strType_;
		std::string g_strTid_;
		std::string g_strMethod_;
		std::string g_strUrl_;
		std::string g_strBody_;

		std::mutex g_mutex_lock_;

		int gathering_count_;
		int total_index; // temp

		std::vector<unsigned char> mqttMsgResponse;
		std::vector<std::string> mqttStringMsgResponse;
};

#endif  // PROCESSING_FOR_DASHBOARD_COMMAND_H_
