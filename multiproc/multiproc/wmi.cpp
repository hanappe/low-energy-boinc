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

	std::cout << "get_username(): " << _username << std::endl;

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

	std::cout << "get_num_core(): " << _num_core << std::endl;

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
}


int wmi_get_cpu_data(
	long long* cpu_load,
	long long* cpu_frequency,
	long long* cpu_load_comp,
	long long* cpu_load_user
	) {
	
	if (wmi_get_cpu_load_frequency(cpu_load, cpu_frequency) == -1) {
		return -1;
	}

	/*if (wmi_get_cpu_comp(cpu_load_comp) == -1) {
		return -1;
	}*/

	if (wmi_get_cpu_comp_user(cpu_load_comp, cpu_load_user) == -1) {
		return -1;
	}
	return 0;
}



int wmi_get_cpu_load_frequency(long long* _cpuload, long long* _cpufrequency) {

	assert(_initialized);
	assert(_cpuload);
	assert(_cpufrequency);

	long long& cpuload = *_cpuload;
	long long& cpufrequency = *_cpufrequency;

	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	// Use the IWbemServices pointer to make requests of WMI ----
	static const bstr_t WQL = bstr_t("WQL");
	static const bstr_t Query = bstr_t("SELECT LoadPercentage, CurrentClockSpeed FROM Win32_Processor");

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

	//for(;;) {
		
		
		
		while (1)
		{
			result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
			
			

			if(0 == uReturn) {
				break;
			}

			VariantInit(&variant);

			result = object->Get(L"LoadPercentage", 0, &variant, 0, 0);
			if (result == WBEM_S_NO_ERROR) {
					//Variant::Print(variant);
					cpuload = variant.iVal;
			}
			//Variant::Print(variant);

			result = object->Get(L"CurrentClockSpeed", 0, &variant, 0, 0);
			if (result == WBEM_S_NO_ERROR) {
					//Variant::Print(variant);
					cpufrequency = variant.iVal;
			}

			/*for (int i = 0; ;++i) {
				
				Variant::Print(variant);
			}*/

			object->Release();
		}
		//Sleep(3000);
		//std::cout << "SLEEP: " << enumerator->Reset() << " " << WBEM_S_NO_ERROR << " "<<  WBEM_E_INVALID_OPERATION << std::endl;
		

	//}

	if (enumerator)
		enumerator->Release();
		
	
}


int wmi_get_cpu_comp_user(long long* pcpu_load_comp, long long* pcpu_load_user) {

	assert(_initialized);
	assert(pcpu_load_comp);
	assert(pcpu_load_user);

	long long& cpu_load_comp = *pcpu_load_comp;
	long long& cpu_load_user = *pcpu_load_user;

	IEnumWbemClassObject* enumerator = 0;
	IWbemClassObject* object = 0;

	cpu_load_comp = 0;
	cpu_load_user = 0;

	bool user_logged = false;

	long long current_process_id = _current_process_id;

	enumerator = request(
		bstr_t("SELECT IDProcess, PercentProcessorTime FROM  Win32_PerfFormattedData_PerfProc_Process WHERE PercentUserTime > 0")
	);
		
	VARIANT var;

	ULONG uReturn;
	for(;;) {
		//std::cout << "before enum" << std::endl;
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		//std::cout << "after enum" << std::endl;
		if(0 == uReturn) {
			break;
		}

		// Declare
		IWbemClassObject* pwcrGetOwnerIn = NULL;
		IWbemClassObject* pwcrGetOwnerOut = NULL;
		IWbemClassObject* pOutParams = NULL;
						
		VariantInit(&var);
		// Get Process ID of object - Required for Get Owner Input
		result = object->Get(L"IDProcess", 0, &var, NULL, NULL );
		long long process_id = var.iVal;

		// Get total cpu load
		VariantInit(&var);
		result = object->Get(L"PercentProcessorTime", 0, &var, NULL, NULL );
		long long process_cpu_load = _wtoi(var.bstrVal);

		/*
		// Get name
		{
			VariantInit(&var_name);
			result = object->Get(L"Name", 0, &var_name, NULL, NULL);
		}*/

		//std::wcout << "process_cpu_load: " << process_cpu_load << " var.bstrVal : " << var.bstrVal << " app name: " << var_name.bstrVal << std::endl;

		//std::cout << "app name: " << wtoa(var_name.bstrVal) << std::endl;
		std::cout << "process_cpu_load: " << process_cpu_load << std::endl;

		if (process_id == current_process_id) {
			//std::cout << " " << std::endl;
			cpu_load_comp += process_cpu_load;

		} else {
			result = services->ExecMethod(bstr_t("Win32_Process.Handle=") + bstr_t(process_id), bstr_t(L"GetOwner"), 0, NULL, pwcrGetOwnerIn, &pOutParams, NULL);
				
			VARIANT user;
			VariantInit(&user);
			if (pOutParams) {
				result = pOutParams->Get(L"User", 0, &user, 0, 0);
			}
					
			if (SUCCEEDED(result) && user.vt == VT_BSTR) {
				const bstr_t& process_owner = user.bstrVal;
				//std::wcout << L"process_owner: " << process_owner << std::endl;
				// Process
				// - Exchange Result only if its a local user. 
				
					//Variant::Print(var);
					//std::cout << std::endl;
				if (process_owner.length() && process_owner == _username) {   
					//std::cout << name << ": Process Owner: " << process_owner << std::endl;
					cpu_load_user += process_cpu_load;
				}
			}	
		}

		object->Release();
	} // for(;;)

	// Normalize value
	{
		
	}


	if (enumerator)
		enumerator->Release();

	return 0;	
}




int wmi_get_cpu_comp_user2(long long* pcpu_load_comp, long long* pcpu_load_user) {

	assert(_initialized);
	assert(pcpu_load_comp);
	assert(pcpu_load_user);

	long long& cpu_load_comp = *pcpu_load_comp;
	long long& cpu_load_user = *pcpu_load_user;

	IEnumWbemClassObject* enumerator = 0;
	IWbemClassObject* object = 0;

	cpu_load_comp = 0;
	cpu_load_user = 0;

	bool user_logged = false;

	long long current_process_id = _current_process_id;

	enumerator = request(
		//bstr_t("SELECT IDProcess, PercentProcessorTime FROM  Win32_PerfFormattedData_PerfProc_Process WHERE PercentUserTime > 0")
		//bstr_t("ASSOCIATORS OF {Win32_LogicalDisk.DeviceID=\"C:\"} ")
		bstr_t("ASSOCIATORS OF {Win32_LogicalDisk.DeviceID=\"C:\"} ")
	);
		
	VARIANT var;

	ULONG uReturn;
	for(;;) {
		std::cout << "before associate enum" << std::endl;
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		std::cout << "after associate enum" << std::endl;
		if(0 == uReturn) {
			break;
		}

		// Declare
		IWbemClassObject* pwcrGetOwnerIn = NULL;
		IWbemClassObject* pwcrGetOwnerOut = NULL;
		IWbemClassObject* pOutParams = NULL;
						
		VariantInit(&var);
		// Get Process ID of object - Required for Get Owner Input
		result = object->Get(L"IDProcess", 0, &var, NULL, NULL );
		long long process_id = var.iVal;

		// Get total cpu load
		VariantInit(&var);
		result = object->Get(L"Name", 0, &var, NULL, NULL );
		long long process_cpu_load = _wtoi(var.bstrVal);

		/*
		// Get name
		{
			VariantInit(&var_name);
			result = object->Get(L"Name", 0, &var_name, NULL, NULL);
		}*/

		//std::wcout << "process_cpu_load: " << process_cpu_load << " var.bstrVal : " << var.bstrVal << " app name: " << var_name.bstrVal << std::endl;

		std::cout << "associator name: " << wtoa(var.bstrVal) << std::endl;
		//std::cout << "process_cpu_load: " << process_cpu_load << std::endl;

		if (process_id == current_process_id) {
			//std::cout << " " << std::endl;
			cpu_load_comp += process_cpu_load;

		} else {
			result = services->ExecMethod(bstr_t("Win32_Process.Handle=") + bstr_t(process_id), bstr_t(L"GetOwner"), 0, NULL, pwcrGetOwnerIn, &pOutParams, NULL);
				
			VARIANT user;
			VariantInit(&user);
			if (pOutParams) {
				result = pOutParams->Get(L"User", 0, &user, 0, 0);
			}
					
			if (user.vt == VT_BSTR) {
				const bstr_t& process_owner = user.bstrVal;
				//std::wcout << L"process_owner: " << process_owner << std::endl;
				// Process
				// - Exchange Result only if its a local user. 
				
					//Variant::Print(var);
					//std::cout << std::endl;
				if (process_owner.length() && process_owner == _username) {   
					//std::cout << name << ": Process Owner: " << process_owner << std::endl;
					cpu_load_user += process_cpu_load;
				}
			}	
		}

		object->Release();
	} // for(;;)


	if (enumerator)
		enumerator->Release();
	/*
	if (cpu_load_comp) {
		std::cout << "cpu_load_comp A: " << cpu_load_comp << std::endl;
		std::cout << "cpu_load_comp B: " << (cpu_load_comp/_num_core) << std::endl;
		//cpu_load_comp /= _num_core;
	}
	if (cpu_load_user) {
		std::cout << "cpu_load_user A: " << cpu_load_user << std::endl;
		std::cout << "cpu_load_user B: " << (cpu_load_user/_num_core) << std::endl;
		
		//cpu_load_user /= _num_core;
	
	}*/
	return 0;
}

int wmi_get_cpu_comp(long long* pcpu_load_comp) {

	assert(_initialized);
	assert(pcpu_load_comp);

	long long& cpu_load_comp = *pcpu_load_comp;

	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	cpu_load_comp = 0;

	long long current_process_id = GetCurrentProcessId();

	enumerator = request(
		bstr_t("SELECT Name, PercentProcessorTime, IDProcess FROM  Win32_PerfFormattedData_PerfProc_Process WHERE IDProcess=" + bstr_t(current_process_id))
		//bstr_t("SELECT Name, PercentProcessorTime, IDProcess FROM  Win32_PerfFormattedData_PerfProc_Process" + current_process_id)
	);
		
	VARIANT var_proc;
	VARIANT var_name;
	VARIANT var_cpu;


	std::cout << "test1 current process id : " << current_process_id << std::endl;
	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);

		if(0 == uReturn) {
			break;
		}

		//std::cout << "test3" << std::endl;

		VariantInit(&var_proc);
		// Get Process ID of object - Required for Get Owner Input
		result = object->Get(L"IDProcess", 0, &var_proc, NULL, NULL );
		long long process_id = var_proc.iVal;

		// Get total cpu load
		VariantInit(&var_cpu);
		result = object->Get(L"PercentProcessorTime", 0, &var_cpu, NULL, NULL );
		long long process_cpu_load = _wtoi(var_cpu.bstrVal);

		// Get name
		{
			VariantInit(&var_name);
			result = object->Get(L"Name", 0, &var_name, NULL, NULL);
		}

		if (process_cpu_load) {
			cpu_load_comp = process_cpu_load;
		}

		//std::wcout << "process_cpu_load: " << process_cpu_load << " var.bstrVal : " << var.bstrVal << " app name: " << var_name.bstrVal << std::endl;

		//std::cout << "app name: " << wtoa(var_name.bstrVal) << " IDProcess : " << process_id << " process_cpu_load : " << process_cpu_load << std::endl;
		

		object->Release();
	} // for(;;)


	if (enumerator)
		enumerator->Release();

	/*if (cpu_load_comp) {
		std::cout << "cpu_load_comp A: " << cpu_load_comp << std::endl;
		std::cout << "cpu_load_comp B: " << (cpu_load_comp/_num_core) << std::endl;
		cpu_load_comp /= _num_core;
	}*/

	return 0;
}


int wmi_get_cpu_user(long long* pcpu_load_user) {

	assert(_initialized);
	assert(pcpu_load_user);

	long long& cpu_load_user = *pcpu_load_user;
	cpu_load_user = 0;

	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	long long current_process_id = GetCurrentProcessId();

	enumerator = request(
		bstr_t("SELECT Handle FROM  Win32_Process WHERE IDProcess")
		//bstr_t("SELECT Name, PercentProcessorTime, IDProcess FROM  Win32_PerfFormattedData_PerfProc_Process" + current_process_id)
	);
		
	VARIANT var_proc;
	VARIANT var_handle;
	VARIANT var_cpu;

	std::cout << "test1 current process id : " << current_process_id << std::endl;
	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);

		if(0 == uReturn) {
			break;
		}

		//std::cout << "test3" << std::endl;

		// Get total cpu load
		VariantInit(&var_handle);
		result = object->Get(L"Handle", 0, &var_handle, NULL, NULL );


		//std::wcout << "process_cpu_load: " << process_cpu_load << " var.bstrVal : " << var.bstrVal << " app name: " << var_name.bstrVal << std::endl;

		std::cout << "app Handle: " << wtoa(var_handle.bstrVal) << std::endl;
		

		object->Release();
	} // for(;;)


	if (enumerator)
		enumerator->Release();

	/*if (cpu_load_comp) {
		std::cout << "cpu_load_comp A: " << cpu_load_comp << std::endl;
		std::cout << "cpu_load_comp B: " << (cpu_load_comp/_num_core) << std::endl;
		cpu_load_comp /= _num_core;
	}*/

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
		bstr_t("SELECT Name, IDProcess, PercentProcessorTime FROM  Win32_PerfFormattedData_PerfProc_Process WHERE PercentUserTime > 0")
	);
		
	VARIANT var;
	VARIANT var_name;
	ULONG uReturn;
	for(;;) {
		//std::cout << "before enum" << std::endl;
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		//std::cout << "after enum" << std::endl;
		if(0 == uReturn) {
			break;
		}

		// Declare
		IWbemClassObject* pwcrGetOwnerIn = NULL;
		IWbemClassObject* pwcrGetOwnerOut = NULL;
		IWbemClassObject* pOutParams = NULL;
						
		VariantInit(&var);
		// Get Process ID of object - Required for Get Owner Input
		result = object->Get(L"IDProcess", 0, &var, NULL, NULL );
		long long process_id = var.iVal;

		// Get total cpu load
		VariantInit(&var);
		result = object->Get(L"PercentProcessorTime", 0, &var, NULL, NULL );
		long long process_cpu_load = _wtoi(var.bstrVal);

		
		// Get name
		{
			VariantInit(&var_name);
			result = object->Get(L"Name", 0, &var_name, NULL, NULL);
			//std::cout << "Name: " << wtoa(var_name.bstrVal) << std::endl;
		}

		//std::wcout << "process_cpu_load: " << process_cpu_load << " var.bstrVal : " << var.bstrVal << " app name: " << var_name.bstrVal << std::endl;

		//std::cout << "app name: " << wtoa(var_name.bstrVal) << std::endl;
		//std::cout << "process_cpu_load: " << process_cpu_load << std::endl;

		if (process_id == current_process_id) {
			//std::cout << "COMP test:" << process_cpu_load << std::endl;
			cpu_load_comp = (float)process_cpu_load;

		} else {
			result = services->ExecMethod(bstr_t("Win32_Process.Handle=") + bstr_t(process_id), bstr_t(L"GetOwner"), 0, NULL, pwcrGetOwnerIn, &pOutParams, NULL);
				
			VARIANT user;
			VariantInit(&user);
			if (pOutParams) {
				result = pOutParams->Get(L"User", 0, &user, 0, 0);
			}
			//std::cout << "result: " << result << std::endl;
			if (SUCCEEDED(result) && user.vt == VT_BSTR) {
				const bstr_t& process_owner = user.bstrVal;
				//std::cout << "process_owner: " << wtoa(user.bstrVal) << std::endl;
				// Process
				// - Exchange Result only if its a local user. 
				
					//Variant::Print(var);
					//std::cout << std::endl;

				if (process_owner.length() && process_owner == _username) { 
					//std::cout << "Process owner test: " << process_cpu_load << std::endl;
					//std::cout << name << ": Process Owner: " << process_owner << std::endl;
					//std::cout << "OWNER test: " << process_cpu_load << std::endl;
					cpu_load_user += (float)process_cpu_load;
				}
			} else {
				if (bstr_t(var_name.bstrVal) != bstr_t("_Total")) {
					//std::cout << "OTHER test: " << process_cpu_load << std::endl;
					cpu_load_sys += (float)process_cpu_load;
				}
			}	
		}

		object->Release();
	} // for(;;)

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



void wmi_free() {

}
