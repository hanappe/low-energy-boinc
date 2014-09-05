#include "Wmi.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <comdef.h>
#include <algorithm>

Wmi * Wmi::sm_wmi = 0;
size_t Wmi::sm_num_cores = 0;
bstr_t Wmi::sm_username;
// Static
struct Variant {
	// Write in file
	static void WriteInFile(const std::string & filename, const VARIANT & v)
	{
		std::wofstream ofile;
		ofile.open(filename.c_str(), std::ios_base::app);
		//tempString = getData();
		//outputFile.write(tempString);
		//Variant::Print(std::wcout, v);
		Variant::Print(ofile, v);
		ofile.close();
	}

	static void WriteInFile(const std::string & filename, std::vector<VARIANT> & list)
	{
		std::wofstream ofile;
		ofile.open(filename.c_str(), std::ios_base::app);
		for (std::vector<VARIANT>::iterator i = list.begin(); i != list.end(); ++i) {
			//Variant::Print(std::wcout, (*i));
			Variant::Print(ofile, (*i));
			ofile << ":";
			//std::wcout << ":";
		}
		ofile << std::endl;
		//std::wcout << std::endl;
	
		ofile.close();
	}
	// Write in standard output
	static void Print(const VARIANT& v)
	{
		Variant::Print(std::wcout, v);
	}
	static void Print(const std::vector<VARIANT> & list)
	{
		for (std::vector<VARIANT>::const_iterator i = list.begin() ; i != list.end(); ++i ) {
			Variant::Print(std::wcout, (*i));
		}
	}

	// Write in stream
	static void Print(std::wostream& stream, const VARIANT& v)
	{
	
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
		/*
		switch (v.vt) {
			case VT_EMPTY:
				stream << L"(empty)";
				return;
			case VT_BSTR:
				stream << v.bstrVal;
				return;
			case VT_I4:
				stream << v.intVal;
			   return;
			case VT_NULL:
				stream << L"(null)";
				return;
			default:
				stream << L"(unknown type)";
				return;
		}*/
	}
		
};


Wmi::Wmi() {

	if(initialize()) {
		sm_num_cores = getNumCore();
		getUsername(sm_username);
	}
}

Wmi::~Wmi() {
	cleanup();
}

//static
size_t Wmi::NumCore()
{
	return sm_num_cores;
}

bstr_t Wmi::Username()
{
	return sm_username;
}

bool Wmi::initialize() {

	// Initialize default value
	locator = NULL;
	services = NULL;
	//enumerator = NULL;

	// Initialize COM

	HRESULT result = CoInitializeEx(0, COINIT_MULTITHREADED);
	
	if (FAILED(result))
    {
        std::cout << "Failed to initialize COM library. Error code = 0x" 
            << std::hex << result << std::endl;
        return false;
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

	if (FAILED(result))
    {
        std::cout << "Failed to initialize security. Error code = 0x" 
            << std::hex << result << std::endl;
        CoUninitialize();
        return false;
    }

	// Obtain the initial locator to WMI

	result = CoCreateInstance(
        CLSID_WbemLocator,             
        0, 
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, (LPVOID *) &locator);
 
    if (FAILED(result))
    {
        std::cout << "Failed to create IWbemLocator object."
            << " Err code = 0x"
            << std::hex << result << std::endl;
        CoUninitialize();
        return false;                 // Program has failed.
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

	if (FAILED(result))
    {
        std::cout << "Could not connect. Error code = 0x" 
             << std::hex << result << std::endl;
        locator->Release();     
        CoUninitialize();
        return false;
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

	if (FAILED(result))
    {
        std::cout << "Could not set proxy blanket. Error code = 0x" 
            << std::hex << result << std::endl;
        services->Release();
        locator->Release();     
        CoUninitialize();
        return false;
    }

	return true;
}

void Wmi::cleanup() {
	services->Release();
	locator->Release();
	//enumerator->Release();
	//object->Release();
	CoUninitialize();
}

IEnumWbemClassObject * Wmi::request(const LPCWSTR request)
{
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

void Wmi::requestData(const LPCWSTR requestStr, const std::vector<LPCWSTR> & props, std::map<LPCWSTR, std::vector<VARIANT> > & res)
{

	IEnumWbemClassObject * enumerator = this->request(requestStr);
	IWbemClassObject *object = 0;
	//Prepare the results array
	res.clear();
	
	HRESULT result;

	VARIANT variant;
	ULONG uReturn;
	while (1)
    {
        result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);

        if(0 == uReturn)
        {
            break;
        }

		
		VariantInit(&variant);

		for (std::vector<LPCWSTR>::const_iterator i = props.begin(); i != props.end(); ++i) {
			HRESULT result = object->Get((*i), 0, &variant, 0, 0);
			if (result == WBEM_S_NO_ERROR) {
				//std::wcout << "[[[" << (*i) << " " <<std::endl;
				//Variant::Print(variant);
				//std::wcout << "]]]" << std::endl;
				res[(*i)].push_back(variant);
			}
		}
    }

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();
}

void Wmi::requestData(const char* WMIClass, LPCWSTR dataName, VARIANT& variant) {

	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	// Use the IWbemServices pointer to make requests of WMI ----
	static const bstr_t WQL = bstr_t("WQL");
	static const bstr_t SELECT_FROM = bstr_t("SELECT * FROM ");

	HRESULT result = services->ExecQuery(WQL,
		SELECT_FROM + bstr_t(WMIClass),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&enumerator
	);

	if (FAILED(result))
    {
        std::cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << std::hex << result << std::endl;
        services->Release();
        locator->Release();
        CoUninitialize();

		//TODO return valid error

        return;
    }


	ULONG uReturn;
	for(;;) {
		result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(0 == uReturn) {
			break;
		}
		result = object->Get(dataName, 0, &variant, 0, 0);
		return;
	}

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();
	
}

void Wmi::getMulti(const char* WMIClass, const std::vector<LPCWSTR> & props, std::map<LPCWSTR, std::vector<VARIANT> > & res)
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	// Use the IWbemServices pointer to make requests of WMI ----
	static const bstr_t WQL = bstr_t("WQL");
	static const bstr_t SELECT_FROM = bstr_t("SELECT * FROM ");

	HRESULT result = services->ExecQuery(WQL,
		SELECT_FROM + bstr_t(WMIClass),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&enumerator
	);

	//std::cout << "result:" << result << std::endl;

	if (FAILED(result))
    {
        std::cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << std::hex << result << std::endl;
        services->Release();
        locator->Release();
        CoUninitialize();

		//TODO return valid error

        return;
    }

	//Prepare the results array
	res.clear();

	VARIANT variant;
	ULONG uReturn;
	while (1)
    {
        result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);

        if(0 == uReturn)
        {
            break;
        }

		
		VariantInit(&variant);

		for (std::vector<LPCWSTR>::const_iterator i = props.begin(); i != props.end(); ++i) {
			result = object->Get((*i), 0, &variant, 0, 0);
			if (result == WBEM_S_NO_ERROR) {
				//Variant::Print(variant);
				res[(*i)].push_back(variant);
			}
			//results.push_back((*i), VARIANT(variant));
		}
		/*
		VARIANT vtProp;
		VariantInit(&vtProp);

		hr = object->Get(L"PercentProcessorTime", 0, &vtProp, 0, 0);
		res[.push_back(VARIANT(variant));
		// Use it
		
		//hr = object->Get(L"TimeStamp_Sys100NS", 0, &vtProp, 0, 0);
		//std::wcout << "<ITER> PercentProcessorTimeA: " << vtProp.ulVal <<std::endl;
		//std::wcout << "<ITER> PercentProcessorTimeB: " << vtProp.ullVal <<std::endl;
		//std::wcout << "<ITER> PercentProcessorTimeC: " << vtProp.ullVal <<std::endl;
		//std::wcout << "<ITER> PercentProcessorTimeC: " << vtProp.uVal <<std::endl;
		// Use it
		*/
    }

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();
	
}

void Wmi::getCpuInfo(long long & cpuload, long long & cpufrequency)
{
	cpuload = 0;
	cpufrequency = 0;

	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	// Use the IWbemServices pointer to make requests of WMI ----
	static const bstr_t WQL = bstr_t("WQL");

	HRESULT result = services->ExecQuery(WQL,
		bstr_t("SELECT LoadPercentage, CurrentClockSpeed FROM Win32_Processor"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&enumerator
	);

	//std::cout << "result:" << result << std::endl;

	if (FAILED(result))
    {
        std::cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << std::hex << result << std::endl;
        services->Release();
        locator->Release();
        CoUninitialize();

		//TODO return valid error
        return;
    }


	VARIANT variant;
	ULONG uReturn;
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

		result = object->Get(L"CurrentClockSpeed", 0, &variant, 0, 0);
		if (result == WBEM_S_NO_ERROR) {
				//Variant::Print(variant);
				cpufrequency = variant.iVal;
		}
    }

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();
	
}

size_t Wmi::getNumCore()
{
	size_t num_core = 0;
	// Fields to select
	std::vector<LPCWSTR> properties;
	properties.push_back(L"NumberOfCores");

	// Request result
	std::map<LPCWSTR, std::vector<VARIANT> > res;

	this->requestData(bstr_t("SELECT NumberOfCores FROM Win32_Processor"), properties, res);
	
	// If the value is 
	std::map<LPCWSTR, std::vector<VARIANT> >::iterator i;
	i = res.find(L"NumberOfCores");
	if (i != res.end() ) {
		num_core = res[L"NumberOfCores"][0].iVal;
	}

	return num_core;
}

size_t Wmi::getMaxClockSpeed()
{
	size_t num_core = 0;
	// Fields to select
	std::vector<LPCWSTR> properties;
	properties.push_back(L"MaxClockSpeed");

	// Request result
	std::map<LPCWSTR, std::vector<VARIANT> > res;

	this->requestData(bstr_t("SELECT MaxClockSpeed FROM Win32_Processor"), properties, res);
	
	// If the value is 
	std::map<LPCWSTR, std::vector<VARIANT> >::iterator i;
	i = res.find(L"MaxClockSpeed");
	if (i != res.end() ) {
		num_core = res[L"MaxClockSpeed"][0].iVal;
	}

	return num_core;
}

long long Wmi::getTotalCpuLoad()
{
	VARIANT v;
	VariantInit(&v);
	
	this->requestData("Win32_Processor", L"LoadPercentage", v);
	//printVariant(v);
	return v.iVal;
}



long long Wmi::getEachCpuLoad() //TODO, not really implemente
{
	std::vector<LPCWSTR> properties;
	//properties.push_back(L"LoadPercentage");
	//properties.push_back(L"MaxClockSpeed");
	//properties.push_back(L"CurrentClockSpeed");
	properties.push_back(L"PercentProcessorTime");
				
	std::map<LPCWSTR, std::vector<VARIANT> > res;
	//m_wmi_query->getMulti("Win32_PerfRawData_PerfOS_Processor", properties, res);
	
	return 0;
}

long long Wmi::getBoincProcessId()
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	// Use the IWbemServices pointer to make requests of WMI ----
	static const bstr_t WQL = bstr_t("WQL");
	static const bstr_t SELECT_FROM = bstr_t("SELECT * FROM ");

	HRESULT result = services->ExecQuery(WQL,
		bstr_t("SELECT * FROM  Win32_Process WHERE Name='boinc.exe'"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&enumerator
	);

	if (FAILED(result))
    {
        std::cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << std::hex << result << std::endl;
        services->Release();
        locator->Release();
        CoUninitialize();

		//TODO return valid error

        return 0;
    }

	HRESULT proc_id_result = -1;

	VARIANT vProcessId;

	ULONG uReturn;

	for(;;) {
		
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		//std::cout << "enumerator result: " << result << "uReturn: " << uReturn << std::endl;
		if(uReturn == 0) {
			break;
		}

		VariantInit(&vProcessId);

		// Put all data in Variants 

		std::vector<VARIANT> variants;
		proc_id_result = object->Get(L"ProcessID", 0, &vProcessId, 0, 0);

		/*

		result = object->Get(L"Description", 0, &variant, 0, 0);
		variants.push_back(variant);
		result = object->Get(L"CSName", 0, &variant, 0, 0);
		variants.push_back(variant);
		result = object->Get(L"CSCreationClassName", 0, &variant, 0, 0);
		variants.push_back(variant);
		result = object->Get(L"CreationClassName", 0, &variant, 0, 0);
		variants.push_back(variant);
		result = object->Get(L"OSCreationClassName", 0, &variant, 0, 0);
		variants.push_back(variant);

		*/

		// Write all variants in file
		//Variant::WriteInFile("processes.txt", variants);
		Variant::Print(variants);
	}

	long long proc_id = 0;

	if (SUCCEEDED(proc_id_result)) {
		proc_id = vProcessId.iVal;
	}

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

	return proc_id;
}


long long Wmi::getBoincCpuLoad()
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	const long long num_core = NumCore();

	const long long processId = getBoincProcessId();
	//std::cout << "Boinc ProcessId: " << processId << std::endl;
	
	if (!processId) {
		return 0;
	}

	//this->requestData(bstr_t("SELECT NumberOfCores FROM Win32_Processor"), properties, res);
	
	enumerator = request(bstr_t("select * from Win32_PerfFormattedData_PerfProc_Process where CreatingProcessID='") + bstr_t(processId) + bstr_t("'"));
	
	HRESULT result_proc, result_user, result_privileged;
	result_proc = result_user = result_privileged = -1;

	long long total_proc, total_user, total_privileged;
	total_proc = total_user = total_privileged = 0;

	VARIANT proc, user, privileged;
	VariantInit(&proc);
	VariantInit(&user);
	VariantInit(&privileged);

	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(uReturn == 0) {
			break;
		}

		result_proc = object->Get(L"PercentProcessorTime", 0, &proc, 0, 0);
		result_user = object->Get(L"PercentUserTime", 0, &user, 0, 0);
		//result_proc = object->Get(L"PercentPrivilegedTime", 0, &privileged, 0, 0);
		total_proc += _wtoi(proc.bstrVal);
		total_user += _wtoi(user.bstrVal);
		//total_privileged += privileged.iVal;

		//std::cout << "PercentProcessorTime: " << proc.iVal << " : ";
		//Variant::Print(proc);
		//std::cout << std::endl;
	}

	// Percent processor time is the usage of one core (not one cpu), value in range [0,100] have to transpose.
	// With 2 cores, a process with 80 % core usage = a process with 40% of cpu.
	//std::cout << "total_proc: " << total_proc << std::endl;
	//std::cout << "total proc: " << total_user << std::endl;

	long long final_cpu_usage_proc = total_proc / num_core;
	long long final_cpu_usage_user = total_user / num_core;
	
	//std::cout << "final_cpu_usage_proc: " << final_cpu_usage_proc << std::endl;
	//std::cout << "total final_cpu_usage_user: " << final_cpu_usage_user << std::endl;

	// What is the most relevant ? percent proc or percent user (?)

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

	return final_cpu_usage_proc;
}

namespace {

	// Split std::string

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
	}

	std::string wtoa(const std::wstring& wstr)
	{
		return (std::string(wstr.begin(), wstr.end()));
	}
	
	bstr_t GetShortName(bstr_t systemUserName) {
		std::wstring wstr(systemUserName, SysStringLen(systemUserName));
		std::string name  = wtoa(wstr);
		
		std::vector<std::string> chunks = split(name, '\\');
		std::string short_name = chunks.back(); // get the last string chunk

		return bstr_t(short_name.c_str());
	};
}


bool Wmi::getUsername(bstr_t& username)
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	bool res = false;
	username = bstr_t();
	enumerator = request(bstr_t("select * from Win32_ComputerSystem"));

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
			username = GetShortName(vUsername.bstrVal);
			res = true;
		}
	}

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

	return res;
}

long long Wmi::getUserCpuLoad()
{

	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	long long total_user_cpu_load = 0;

	const long long num_core = NumCore();

	if (!num_core) {
		return 0;
	}

	std::vector<long long> process_id_list;

	bstr_t username;
	if (getUsername(username)) {

		enumerator = request(
			bstr_t("SELECT Name, ProcessId FROM  Win32_Process")
		);
		
		VARIANT var;

		ULONG uReturn;
		for(;;) {
			HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
			if(0 == uReturn) {
				break;
			}

			VariantInit(&var);

			// Put all data in Variants 

			std::vector<VARIANT> variants;

			result = object->Get(L"Name", 0, &var, 0, 0);
			variants.push_back(var);
			
			bstr_t name = var.bstrVal;

			long long process_id = 0;
			
			if(name != bstr_t("System") && name != bstr_t("System Idle Process")) {
				// Declare
				IWbemClassObject* pwcrGetOwnerIn = NULL;
				IWbemClassObject* pwcrGetOwnerOut = NULL;
				IWbemClassObject* pOutParams = NULL;
						
				VariantInit(&var);
				// Get Process ID of object - Required for Get Owner Input
				result = object->Get(L"ProcessId", 0, &var, NULL, NULL );
						
				process_id = var.iVal;

				/*
				result = object->Get(L"Handle", 0, &var, NULL, NULL );
				std::cout << "HANDLE: ";
					Variant::Print(var);
				std::cout << std::endl;
				*/
				// Execute WMI Providor - Get Owner
				// Requires Admin rights

				//result = services->GetObject(_bstr_t("Win32_Process"), 0, NULL, &object, NULL);  
				
				//std::cout << "before" << pwcrGetOwnerIn;
				// - Get Service Object
				//result = object->GetMethod(bstr_t("GetOwner"), 0, &pwcrGetOwnerIn, &pwcrGetOwnerOut);    // - Get Service Method
							//std::cout << "after" << pwcrGetOwnerIn;
				// Execute Method
				result = services->ExecMethod(bstr_t("Win32_Process.Handle=") + bstr_t(process_id), bstr_t(L"GetOwner"), 0, NULL, pwcrGetOwnerIn, &pOutParams, NULL);
				
				VARIANT user;
				VariantInit(&user);
				if (pOutParams) {
					result = pOutParams->Get(L"User", 0, &user, 0, 0);
				}
					
				if (user.vt == VT_BSTR) {
					bstr_t process_owner = user.bstrVal;
					//std::wcout << L"process_owner: " << process_owner << std::endl;
					// Process
					// - Exchange Result only if its a local user. 
					if(process_owner.length() && process_owner != bstr_t("SYSTEM") && process_owner != bstr_t("LOCAL SERVICE") && process_owner != bstr_t("NETWORK SERVICE"))
					{   
						process_id_list.push_back(process_id);
					}
				}	
			}
		}
	} // if getUsername

	total_user_cpu_load = getProcessCpuLoad(process_id_list);

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

	if (total_user_cpu_load > 0)
		total_user_cpu_load = total_user_cpu_load / num_core;

	return total_user_cpu_load;
}

long long Wmi::getProcessCpuLoad(std::vector<long long> process_id_list)
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	enumerator = request(bstr_t("select IDProcess, PercentUserTime from Win32_PerfFormattedData_PerfProc_Process where PercentUserTime > 0"));
	
	long long total_proc = 0;

	VARIANT var;
	VariantInit(&var);

	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);

		if(uReturn == 0) {
			break;
		}

		result = object->Get(L"IDProcess", 0, &var, 0, 0);

		if(std::find(process_id_list.begin(), process_id_list.end(), var.iVal) != process_id_list.end()) {
			result = object->Get(L"PercentUserTime", 0, &var, 0, 0);

			total_proc += _wtoi(var.bstrVal);
		}
	}

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

	return total_proc;
}

long long Wmi::getProcessCpuLoad(long long processId)
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	enumerator = request(bstr_t("select PercentProcessorTime from Win32_PerfFormattedData_PerfProc_Process where IDProcess='") + bstr_t(processId) + bstr_t("' and PercentProcessorTime > 0"));
	
	HRESULT result_proc;
	result_proc = -1;

	long long total_proc;
	total_proc = 0;

	VARIANT proc;
	VariantInit(&proc);


	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);

		if(uReturn == 0) {
			break;
		}

		result_proc = object->Get(L"PercentProcessorTime", 0, &proc, 0, 0);

		total_proc = _wtoi(proc.bstrVal);
	}

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

	return total_proc;
}


void Wmi::getBoincAndUserCpuLoad(long long& boin_cpu_load, long long& user_cpu_load) {
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	boin_cpu_load = 0;
	user_cpu_load = 0;

	bool boinc_process_exists = false;
	bool user_logged = false;

	long long boinc_process_id = getBoincProcessId();
	if (boinc_process_id) {
		boinc_process_exists = true;
	}


	std::vector<long long> process_id_list;

	enumerator = request(
		bstr_t("SELECT Name, CreatingProcessID, IDProcess, PercentProcessorTime FROM  Win32_PerfFormattedData_PerfProc_Process WHERE PercentUserTime > 0")
	);
		
	VARIANT var;

	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(0 == uReturn) {
			break;
		}

		VariantInit(&var);

		// Declare
		IWbemClassObject* pwcrGetOwnerIn = NULL;
		IWbemClassObject* pwcrGetOwnerOut = NULL;
		IWbemClassObject* pOutParams = NULL;
						
		VariantInit(&var);
		// Get Process ID of object - Required for Get Owner Input
		result = object->Get(L"IDProcess", 0, &var, NULL, NULL );
						
		long long process_id = var.iVal;

		// Get parent process id
		VariantInit(&var);
		result = object->Get(L"CreatingProcessID", 0, &var, NULL, NULL );
		long long process_parent_id = var.iVal;

		// Get total cpu load
		VariantInit(&var);
		result = object->Get(L"PercentProcessorTime", 0, &var, NULL, NULL );
		long long process_cpu_load = _wtoi(var.bstrVal);

		if (boinc_process_exists && process_parent_id == boinc_process_id) {

			boin_cpu_load += process_cpu_load;

		} else {
			result = services->ExecMethod(bstr_t("Win32_Process.Handle=") + bstr_t(process_id), bstr_t(L"GetOwner"), 0, NULL, pwcrGetOwnerIn, &pOutParams, NULL);
				
			VARIANT user;
			VariantInit(&user);
			if (pOutParams) {
				result = pOutParams->Get(L"User", 0, &user, 0, 0);
			}
					
			if (user.vt == VT_BSTR) {
				bstr_t process_owner = user.bstrVal;
				//std::wcout << L"process_owner: " << process_owner << std::endl;
				// Process
				// - Exchange Result only if its a local user. 
				
					//Variant::Print(var);
					//std::cout << std::endl;
				if(process_owner.length() && process_owner == Username())
				{   
					//std::cout << name << ": Process Owner: " << process_owner << std::endl;
					user_cpu_load += process_cpu_load;
				}
			}	
		}
	} // for(;;)


	if (enumerator)
		enumerator->Release();

	if (object)
		object->Release();

	if (boin_cpu_load)
		boin_cpu_load /= NumCore();

	if (user_cpu_load)
		user_cpu_load /= NumCore();
}
void Wmi::printAllProcess()
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	enumerator = request(
			//bstr_t("ASSOCIATORS OF {Win32_LogonSession.LogonId='") + bstr_t(login_id) + bstr_t("'} Where AssocClass=Win32_Account Role=Antecedent")
			bstr_t("SELECT * FROM  Win32_Process")
		);

	VARIANT variant;

	ULONG uReturn;
	for(;;) {
		HRESULT result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(0 == uReturn) {
			break;
		}

		VariantInit(&variant);

		// Put all data in Variants 

		std::vector<VARIANT> variants;

		result = object->Get(L"Name", 0, &variant, 0, 0);
		variants.push_back(variant);
		
		/// Write all variants in file
		//Variant::WriteInFile("processes.txt", variants);
		Variant::Print(variants);
	}

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();
}

void Wmi::getMsAcpi()
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	static const bstr_t WQL = bstr_t("WQL");

	HRESULT result = services->ExecQuery(WQL,
		bstr_t("SELECT * FROM Win32_TemperatureProbe"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&enumerator
	);


	if (FAILED(result))
    {
        std::cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << std::hex << result << std::endl;
        services->Release();
        locator->Release();
        CoUninitialize();

		//TODO return valid error
        return;
    }

	VARIANT vUserName;
	VARIANT vUserId;

	ULONG uReturn;
	for(;;) {
		result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(0 == uReturn) {
	
			break;
		}

		VariantInit(&vUserName);
		VariantInit(&vUserId);

		std::vector<VARIANT> list;

		result = object->Get(L"Caption", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"Name", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"VariableSpeed", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"DeviceID", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"CurrentTemperature", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"CurrentRead", 0, &vUserName, 0, 0);
		list.push_back(vUserName);

		//Variant::Print(list);
		//res[vUserName.bstrVal] = vUserId.bstrVal;
	}

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

	return;
}

HRESULT Wmi::GetCpuTemperature(LPLONG pTemperature)
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

    if (pTemperature == NULL)
        return E_INVALIDARG;

    *pTemperature = -1;
    HRESULT ci = CoInitialize(NULL); // needs comdef.h
    //HRESULT hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        IWbemLocator *pLocator; // needs Wbemidl.h & Wbemuuid.lib
        hr = CoCreateInstance(CLSID_WbemAdministrativeLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLocator);
        if (SUCCEEDED(hr))
        {
            IWbemServices *pServices;
            BSTR ns = SysAllocString(L"root\\WMI");
            hr = pLocator->ConnectServer(ns, NULL, NULL, NULL, 0, NULL, NULL, &pServices);
            pLocator->Release();
            SysFreeString(ns);
            if (SUCCEEDED(hr))
            {
                BSTR query = SysAllocString(L"SELECT * FROM MSAcpi_ThermalZoneTemperature");
                BSTR wql = SysAllocString(L"WQL");
                IEnumWbemClassObject *pEnum;
                hr = pServices->ExecQuery(wql, query, WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, NULL, &pEnum);
                SysFreeString(wql);
                SysFreeString(query);
                pServices->Release();
                if (SUCCEEDED(hr))
                {
                    IWbemClassObject *pObject;
                    ULONG returned;
                    hr = pEnum->Next(WBEM_INFINITE, 1, &pObject, &returned);
                    pEnum->Release();
                    if (SUCCEEDED(hr))
                    {
                        BSTR temp = SysAllocString(L"CurrentTemperature");
                        VARIANT v;
                        VariantInit(&v);
                        hr = pObject->Get(temp, 0, &v, NULL, NULL);
                        pObject->Release();
                        SysFreeString(temp);
                        if (SUCCEEDED(hr))
                        {
                            *pTemperature = V_I4(&v);
                        }
						Variant::Print(v);
                        VariantClear(&v);
                    }
                }
            }
            if (ci == S_OK)
            {
                CoUninitialize();
            }
        }
    }

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

    return hr;
}

unsigned int Wmi::getTemperature()
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	static const bstr_t WQL = bstr_t("WQL");

	HRESULT result = services->ExecQuery(WQL,
		bstr_t("SELECT * FROM Win32_TemperatureProbe"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&enumerator
	);


	if (FAILED(result))
    {
        std::cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << std::hex << result << std::endl;
        services->Release();
        locator->Release();
        CoUninitialize();

		//TODO return valid error
        return 0;
    }

	VARIANT vUserName;
	VARIANT vUserId;

	ULONG uReturn;
	for(;;) {
		result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(0 == uReturn) {
			break;
		}
		VariantInit(&vUserName);
		VariantInit(&vUserId);
		std::vector<VARIANT> list;

		result = object->Get(L"Accuracy", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"Availability", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"CurrentReading", 0, &vUserName, 0, 0);
		result = object->Get(L"NominalReading", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		list.push_back(vUserName);
		result = object->Get(L"MaxReadable", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"MinReadable", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"Name", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"Tolerance", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"Resolution", 0, &vUserName, 0, 0);
		list.push_back(vUserName);

		std::cout << "PRINT: " << std::endl;
		Variant::Print(list);
	}

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

	return 0;
}


unsigned int Wmi::getFanSpeed()
{
	IEnumWbemClassObject *enumerator = 0;
	IWbemClassObject * object = 0;

	static const bstr_t WQL = bstr_t("WQL");

	HRESULT result = services->ExecQuery(WQL,
		bstr_t("SELECT * FROM Win32_Fan"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&enumerator
	);


	if (FAILED(result))
    {
        std::cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << std::hex << result << std::endl;
        services->Release();
        locator->Release();
        CoUninitialize();

		//TODO return valid error
        return 0;
    }

	VARIANT vUserName;
	VARIANT vUserId;

	ULONG uReturn;
	for(;;) {
		result = enumerator->Next(WBEM_INFINITE, 1, &object, &uReturn);
		if(0 == uReturn) {
			break;
		}
		VariantInit(&vUserName);
		VariantInit(&vUserId);
		std::vector<VARIANT> list;

		result = object->Get(L"ActiveCooling", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"Availability", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"Caption", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"DesiredSpeed", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"Name", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"Status", 0, &vUserName, 0, 0);
		list.push_back(vUserName);
		result = object->Get(L"VariableSpeed", 0, &vUserName, 0, 0);
		list.push_back(vUserName);

		std::cout << "PRINT: " << std::endl;
		Variant::Print(list);
	}

	if (enumerator)
		enumerator->Release();
	if (object)
		object->Release();

	return 0;
}