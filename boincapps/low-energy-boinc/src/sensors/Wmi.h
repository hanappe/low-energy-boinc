#ifndef WMI_HPP
#define WMI_HPP

#include <vector>
#include <map>

#include <comdef.h>
#include <WbemIdl.h>

//#pragma comment(lib, "wbemuuid.lib")

class Wmi {

private:
	Wmi();
	~Wmi();
	bool initialize();
	void cleanup();

public:
	
	static Wmi * GetInstance() {
		if (!sm_wmi) {
			sm_wmi = new Wmi();
		}
		return sm_wmi;
	}
	static void Kill() {
		if (sm_wmi) {
			delete sm_wmi;
			sm_wmi = 0;
		}
	}

public:
	static size_t NumCore(); // Return number of core found at the initialization
	static bstr_t Username(); // Return number of core found at the initialization

private:
	static Wmi * sm_wmi; // Static member Wmi

	static size_t sm_num_cores; // We save the number of cores, assuming we can't change it in a computer. :D
	static bstr_t sm_username; // We save the number of cores, assuming we can't change it in a computer. :D

public:

	// Common Function
	IEnumWbemClassObject * request(const LPCWSTR requestStr);
	void requestData(const LPCWSTR request, const std::vector<LPCWSTR> & props, std::map<LPCWSTR, std::vector<VARIANT> > & res);
	void requestData(const char* WMIClass, const LPCWSTR dataName, VARIANT& v);

	//void getList(const char* WMIClass, const std::vector<LPCWSTR> & props, std::vector<VARIANT> & results);
	// same as getList - TODO remove one this both function
	void getMulti(const char* WMIClass, const std::vector<LPCWSTR> & props, std::map<LPCWSTR, std::vector<VARIANT> > & res);
	void getMulti2(const char* WMIClass, const std::vector<LPCWSTR> & props, std::map<LPCWSTR, std::vector<VARIANT> > & res);

	// Specific Function
	
	size_t getNumCore(); // Get number of core through WMI query
	long long getTotalCpuLoad();
	long long getEachCpuLoad(); //WIP, not really implemented
	long long getBoincProcessId(); //Get the process id of "boin.exe", 0 if doesn't exist.
	long long getBoincCpuLoad(); //Get all CPU load of Boinc app [0, 100]
	long long getUserCpuLoad(); //Get all CPU load of User app [0, 100]

	void getBoincAndUserCpuLoad(long long& boin_cpu_load, long long& user_cpu_load); //Get all CPU load of User app AND all CPU load of Boinc [0, 100]
	//bool getUserInfo(bstr_t& username, long long& user_id); // Get current logged username and id

	bool getUsername(bstr_t& username); // Get current logged username
	long long getProcessCpuLoad(long long processId); // Get Cpu usage [0, 100] of process given by process id
	long long getProcessCpuLoad(std::vector<long long> processId); // Get Cpu usage [0, 100] of process given by process id
	
	// Helper/Debug function
	void printAllProcess();

	std::map<BSTR, BSTR> getLoggedUsersId(); // ???

	bool getProcTimeAndTimeStamp(long long processId, VARIANT& proc, VARIANT& time); // WIP - doesn't work
	void getMsAcpi(); // WIP - doesn't work
	unsigned int getTemperature(); // WIP - doesn't work
	unsigned int getFanSpeed(); // WIP - doesn't work

    static HRESULT GetCpuTemperature(LPLONG temperature); // WIP - doesn't work

private:

	IWbemLocator *locator;
	IWbemServices *services;


};

#endif // WMI_HPP