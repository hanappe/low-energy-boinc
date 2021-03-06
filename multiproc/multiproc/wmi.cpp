#include "wmi.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <comdef.h>
#include <WbemIdl.h>
#include <intrin.h>

#include <assert.h>

#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <set>

#include "log.h"

// Kev
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

static IWbemLocator* locator = NULL;
static IWbemServices* services = NULL;
static size_t _num_core = 0; // We save the number of cores, assuming we can't change it in a computer. :D
static bstr_t _username = bstr_t("");
static long long _current_process_id = 0;
static int _initialized = 0;


static void Print(std::wostream& stream, const VARIANT& v) {
	
		switch (v.vt) {
			case VT_EMPTY:
				stream << L"(empty)" << std::endl;
				return;
			case VT_BSTR:
				stream << L"VT_BSTR " << v.bstrVal << std::endl;
				return;
			case VT_I4:
				stream << L"VT_I4 " << v.intVal << std::endl;
			   return;
			case VT_NULL:
				stream << L"(null)" << std::endl;
				return;
			default:
				stream << L"(unknown type)";
				return;
		}
}

// Split std::string

char* token;
char* string;
char* tofree;
/*
string = strdup("abc,def,ghi");
	if (string != NULL) {

	char * toFree = string;

	while ((token = strsep(&string, "\\")) != NULL) {
		printf("%s\n", token);
	}
	*/

void split_and_get_last(const std::string &s, std::string& res) {
	std::stringstream ss(s);
	while (std::getline(ss, res, '\\')) {
	}
}


/*
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}*/

std::string wtoa(const std::wstring& wstr) {
	return (std::string(wstr.begin(), wstr.end()));
}

bstr_t GetShortName(bstr_t systemUserName) {
	std::wstring wstr(systemUserName, SysStringLen(systemUserName));
	std::string name  = wtoa(wstr);
		
	//std::vector<std::string> chunks = split(name, '\\');
	//std::string short_name = chunks.back(); // get the last string chunk

	//std::cout << short_name << std::endl;
	//return bstr_t(short_name.c_str());
	std::string short_name;
	split_and_get_last(name, short_name);
	return bstr_t(short_name.c_str());
};



static IEnumWbemClassObject* request(const LPCWSTR request) {
	static const bstr_t WQL = bstr_t("WQL");
	
	IEnumWbemClassObject * res_numerator = 0x0;

	HRESULT result = services->ExecQuery(WQL,
		bstr_t(request),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&res_numerator
	);

	if (FAILED(result))
    {
        std::cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << std::hex << result << std::endl;
        services->Release();
        locator->Release();
        CoUninitialize();
        return 0;
    }
	return res_numerator;
}

static int get_username() {
	
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	bool res = false;
	_username = bstr_t();
	enumerator = request(bstr_t("select UserName from Win32_ComputerSystem"));

	VARIANT vUsername;
	VariantInit(&vUsername);

	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(uReturn == 0) {
			break;
		}
		result = object->Get(L"UserName", 0, &vUsername, 0, 0);

		if (SUCCEEDED(result)) {
			_username = GetShortName(vUsername.bstrVal);
			res = true;
		}
		object->Release();
	}

	if (enumerator)
		enumerator->Release();

	//std::cout << "get_username(): " << _username << std::endl;

	return res;
}

static int get_num_core() {
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	int res = -1;
	_num_core = 0;
	enumerator = request(bstr_t("select NumberOfCores from Win32_Processor"));

	VARIANT vUsername;
	VariantInit(&vUsername);

	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(uReturn == 0) {
			break;
		}
		result = object->Get(L"NumberOfCores", 0, &vUsername, 0, 0);

		if (SUCCEEDED(result)) {
			_num_core = vUsername.uiVal;
			res = 0;
		}
		object->Release();
	}

	if (enumerator)
		enumerator->Release();

	//std::cout << "get_num_core(): " << _num_core << std::endl;

	return res;
}

static void get_current_process_id() {

	_current_process_id = GetCurrentProcessId();
}

static void get_initial_values() {
	get_username();
	get_num_core();
	get_current_process_id();

}

int wmi_initialize() {

	// Initialize default value
	locator = NULL;
	services = NULL;
	//enumerator = NULL;

	// Initialize COM

	HRESULT result = CoInitializeEx(0, COINIT_MULTITHREADED);
	
	if (FAILED(result)) {
        log_err("Failed to initialize COM library. Error code = %x", result);
        return -1;
    }

	// Set general COM security levels

	result = CoInitializeSecurity(
        NULL, 
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities 
        NULL                         // Reserved
    );

	if (FAILED(result)) {
       log_err("Failed to initialize security. Error code = %x", result);
        CoUninitialize();
        return -1;
    }

	// Obtain the initial locator to WMI

	result = CoCreateInstance(
        CLSID_WbemLocator,             
        0, 
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, (LPVOID *) &locator);
 
    if (FAILED(result)) {
       log_err("Failed to create IWbemLocator object. Error code = %x", result);
        CoUninitialize();
        return -1;                 // Program has failed.
    }

	// Connect to WMI through the IWbemLocator::ConnectServer method

	// Connect to the root\cimv2 namespace with
    // the current user and obtain pointer 'services'
    // to make IWbemServices calls.

	result = locator->ConnectServer(
         _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
         NULL,                    // User name. NULL = current user
         NULL,                    // User password. NULL = current
         0,                       // Locale. NULL indicates current
         NULL,                    // Security flags.
         0,                       // Authority (for example, Kerberos)
         0,                       // Context object 
         &services                // pointer to IWbemServices proxy
    );

	if (FAILED(result)) {
        log_err("Could not connect. Error code = %x", result);
        locator->Release();     
        CoUninitialize();
        return -1;
    }

	//std::cout << "Connected to ROOT\\CIMV2 WMI namespace" << std::endl;

	// Set security levels on the proxy

	result = CoSetProxyBlanket(
       services,                        // Indicates the proxy to set
       RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
       RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
       NULL,                        // Server principal name 
       RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
       RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
       NULL,                        // client identity
       EOAC_NONE                    // proxy capabilities 
    );

	if (FAILED(result)) {
        log_err("Could not set proxy blanket. Error code = %x", result);
        services->Release();
        locator->Release();     
        CoUninitialize();
        return -1;
    }

	get_initial_values();

	_initialized = 1;
	return 0;
}

/// Float version


int wmi_get_cpu_dataf(
	float* cpu_frequency,
	float* cpu_load_comp,
	float* cpu_load_user,
	float* cpu_load_sys
	) {
	
	if (wmi_get_cpu_frequency(cpu_frequency) == -1) {
		return -1;
	}

	if (wmi_get_cpu_comp_user_sys(cpu_load_comp, cpu_load_user, cpu_load_sys) == -1) {
		return -1;
	}
	return 0;
}



int wmi_get_cpu_frequency(float* pcpu_frequency) {

	assert(_initialized);
	assert(pcpu_frequency);

	float& cpu_frequency = *pcpu_frequency;

	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	// Use the IWbemServices pointer to make requests of WMI ----
	static const bstr_t WQL = bstr_t("WQL");
	static const bstr_t Query = bstr_t("SELECT CurrentClockSpeed FROM Win32_Processor");

	HRESULT result = services->ExecQuery(WQL,
		Query,
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&enumerator
	);

	if (FAILED(result)) {
		log_err("Query for operating system name failed."
					 " Error code = %d \n", result);
        services->Release();
        locator->Release();
        CoUninitialize();

		//TODO return valid error
        return -1;
    }

	VARIANT variant;
	ULONG uReturn;

	for(;;)
	{
		result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
			
		if(0 == uReturn) {
			break;
		}

		VariantInit(&variant);

		result = object->Get(L"CurrentClockSpeed", 0, &variant, 0, 0);
		if (result == WBEM_S_NO_ERROR) {
				//Variant::Print(variant);
				cpu_frequency = (float)variant.iVal;
		}


		object->Release();
	}

	if (enumerator)
		enumerator->Release();	

	return 0;
}

namespace {
	//std::map<long long, >_owner_map
	std::set<long long> user_proc_set;
	std::set<long long> system_proc_set;
}

int wmi_get_cpu_comp_user_sys(float* pcpu_load_comp, float* pcpu_load_user, float* pcpu_load_sys) {

	assert(_initialized);
	assert(pcpu_load_comp);
	assert(pcpu_load_user);
	assert(pcpu_load_sys);

	float& cpu_load_comp = *pcpu_load_comp;
	float& cpu_load_user = *pcpu_load_user;
	float& cpu_load_sys = *pcpu_load_sys;

	IEnumWbemClassObject* enumerator = 0;
	IWbemClassObject* object = 0;

	cpu_load_comp = 0;
	cpu_load_user = 0;

	bool user_logged = false;

	long long current_process_id = _current_process_id;

	enumerator = request(
		bstr_t("SELECT Name, IDProcess, PercentProcessorTime FROM Win32_PerfFormattedData_PerfProc_Process WHERE PercentUserTime > 0")
	);
		
	VARIANT var;
	VARIANT var_name;
	VARIANT user;

	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);

		if(0 == uReturn) {
			break;
		}
			
		VariantInit(&var);
		// Get Process ID of object - Required for Get Owner Input
		result = object->Get(L"IDProcess", 0, &var, NULL, NULL );
		long long process_id = var.iVal;

		// Get total cpu load
		VariantInit(&var);
		result = object->Get(L"PercentProcessorTime", 0, &var, NULL, NULL );
		long long process_cpu_load = _wtoi(var.bstrVal);

		//std::wcout << "process_cpu_load: " << process_cpu_load << " var.bstrVal : " << var.bstrVal << " app name: " << var_name.bstrVal << std::endl;

		//std::cout << "app name: " << wtoa(var_name.bstrVal) << std::endl;
		//std::cout << "process_cpu_load: " << process_cpu_load << std::endl;
		if (process_id == 0) {

			// Process id 0 = System Idle Process
			continue;

		} else if (process_id == current_process_id) {

			//std::cout << "COMP test:" << process_cpu_load << std::endl;
			cpu_load_comp = (float)process_cpu_load;

		} else if(user_proc_set.find(process_id) != user_proc_set.end()) {

			//std::cout << "user fnd:" << process_id << '\n';
			//Print(std::wcout, var_name);
			cpu_load_user += (float)process_cpu_load;

		} else if(system_proc_set.find(process_id) != system_proc_set.end()) {

			//std::cout << "sys Fnd:" << process_id << '\n';
			//Print(std::wcout, var_name);
			cpu_load_sys += (float)process_cpu_load;

		} else {
			//std::cout << "TEST: " << process_id << '\n';
					// Declare
			IWbemClassObject* pwcrGetOwnerIn = NULL;
			IWbemClassObject* pwcrGetOwnerOut = NULL;
			IWbemClassObject* pOutParams = NULL;
			result = services->ExecMethod(bstr_t("Win32_Process.Handle=") + bstr_t(process_id), bstr_t(L"GetOwner"), 0, NULL, pwcrGetOwnerIn, &pOutParams, NULL);
				
			VariantInit(&user);
			if (pOutParams) {
				result = pOutParams->Get(L"User", 0, &user, 0, 0);
				pOutParams->Release();
			}
			//std::cout << "result: " << result << std::endl;
			if (SUCCEEDED(result) && user.vt == VT_BSTR) {
				const bstr_t& process_owner = user.bstrVal;
				if (process_owner.length() && process_owner == _username) { 
					//std::cout << "user Insrt: " << process_id << " <_> " << process_cpu_load << '\n';
					//Print(std::wcout, var_name);
					user_proc_set.insert(process_id);
					cpu_load_user += (float)process_cpu_load;
				}
			} else {

				// Get process name
				{
					result = object->Get(L"Name", 0, &var_name, NULL, NULL);
					//std::cout << "Name: " << wtoa(var_name.bstrVal) << std::endl;
				}

				if (bstr_t(var_name.bstrVal) != bstr_t("_Total")) {
					//std::cout << "OTHER test: " << process_cpu_load << std::endl;
					//std::cout << "system Insrt:" << process_id << " <_> " << process_cpu_load << '\n';
					//Print(std::wcout, var_name);
					system_proc_set.insert(process_id);
					cpu_load_sys += (float)process_cpu_load;
				}
			}

			//pOutParams->Release();
		}

		object->Release();
	} // for(;;)


	VariantClear(&var);
    VariantClear(&var_name);
	VariantClear(&user);

	// Normalize value
	float total_cpu_load = cpu_load_comp + cpu_load_user + cpu_load_sys;
	if (total_cpu_load > 100) {
		cpu_load_comp = (cpu_load_comp / total_cpu_load) * 100;
		cpu_load_user = (cpu_load_user / total_cpu_load) * 100;
		cpu_load_sys = (cpu_load_sys / total_cpu_load) * 100;
	}
	//std::cout << "cpu_load_comp f: " << cpu_load_comp << std::endl;
	//std::cout << "cpu_load_user f: " << cpu_load_user << std::endl;
	//std::cout << "cpu_load_sys f: " << cpu_load_sys << std::endl;

	if (enumerator)
		enumerator->Release();

	return 0;	
}


int wmi_get_battery_info() {

	std::cout << "wmi_get_battery_info" << std::endl;

	//assert(_initialized);
	//assert(pcpu_load_comp);
	//assert(pcpu_load_user);
	//assert(pcpu_load_sys);

	//float& cpu_load_comp = *pcpu_load_comp;
	//float& cpu_load_user = *pcpu_load_user;
	//float& cpu_load_sys = *pcpu_load_sys;

	IEnumWbemClassObject* enumerator = 0;
	IWbemClassObject* object = 0;

	//cpu_load_comp = 0;
	//cpu_load_user = 0;

	bool user_logged = false;

	long long current_process_id = _current_process_id;

	
	enumerator = request(
		// uint16, uint32, uint16, uint32... 
		bstr_t("SELECT"
		" Availability, BatteryRechargeTime, BatteryStatus,"
		"Caption, DesignCapacity, DesignVoltage, FullChargeCapacity"
		" FROM  Win32_Battery")
	);
		
	
	ULONG uReturn;
	for(;;) {
		
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(0 == uReturn) {
			break;
		}

		VARIANT var;
		VariantInit(&var);
		result = object->Get(L"Voltage", 0, &var, NULL, NULL );
		
		if (SUCCEEDED(result)) {
			Print(std::wcout, var);
			//unsigned short v = var.uiVal;
			//std::cout << "Availability: " << v << std::endl; 
		}
		
		VariantInit(&var);
		result = object->Get(L"DischargeRate", 0, &var, NULL, NULL );
		
		if (SUCCEEDED(result)) {
			Print(std::wcout, var);
			//unsigned int v = var.uiVal;
			//std::cout << "BatteryRechargeTime: " << v << std::endl; 
		}

		VariantInit(&var);
		result = object->Get(L"Caption", 0, &var, NULL, NULL );
		
		if (SUCCEEDED(result)) {
			Print(std::wcout, var);
			//unsigned int v = var.uiVal;
			//std::cout << "BatteryRechargeTime: " << v << std::endl; 
		}

		VariantInit(&var);
		result = object->Get(L"DesignCapacity", 0, &var, NULL, NULL );
		
		if (SUCCEEDED(result)) {
			Print(std::wcout, var);
			//unsigned int v = var.uiVal;
			//std::cout << "DesignCapacity: " << v << std::endl; 
		}

		VariantInit(&var);
		result = object->Get(L"DesignVoltage", 0, &var, NULL, NULL );
		
		if (SUCCEEDED(result)) {
			Print(std::wcout, var);
			//unsigned long long v = var.ullVal;
			//std::cout << "DesignVoltage: " << v << std::endl; 
		}

		VariantInit(&var);
		result = object->Get(L"FullChargeCapacity", 0, &var, NULL, NULL );
		
		if (SUCCEEDED(result)) {
			Print(std::wcout, var);
			//unsigned int v = var.uiVal;
			//std::cout << "FullChargeCapacity: " << v << std::endl; 
		}


		object->Release();
	} // for(;;)

	if (enumerator)
		enumerator->Release();

	return 0;	

}


void wmi_free() {

}
