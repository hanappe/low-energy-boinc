#include "battery_info.h"
#include "log.h"

#include <assert.h>

#include <WINIOCTL.h>
#include <batclass.h>
#include <Setupapi.h>
#include <devguid.h>

typedef BOOL (CALLBACK *BATTERYENUMPROC)(LPWSTR lpszBattery);

static HANDLE _bathandle = 0;
static float _max_capacity = 0.0f;

static BOOL BatteryQueryInformation(HANDLE hHandle,
                        BATTERY_QUERY_INFORMATION_LEVEL InfoLevel,
                        LPVOID lpBuffer,
                        DWORD dwBufferSize);

int battery_init() {
	

	#define GBS_HASBATTERY 0x1
	#define GBS_ONBATTERY  0x2

	DWORD dwResult = GBS_ONBATTERY;
	HDEVINFO hdev = NULL;
	int idev = 0;
	BOOL b;
	DWORD cbRequired = 0;
	SP_DEVICE_INTERFACE_DATA did;

	hdev = SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY, 
                                0, 
                                0, 
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (INVALID_HANDLE_VALUE == hdev) {
		//return hdev;
		printf("GUID_DEVCLASS_BATTERY error\n");
		return -1;
	}

	for (idev = 0; idev < 100; idev++) {
		printf("for\n");
		
		memset(&did, 0, sizeof(did));
		did.cbSize = sizeof(did);

		b = SetupDiEnumDeviceInterfaces(hdev,
                                      0,
                                      &GUID_DEVCLASS_BATTERY,
                                      idev,
                                      &did);
		if (!b) {
			break;
		}

        SetupDiGetDeviceInterfaceDetail(hdev,
                                        &did,
                                        0,
                                        0,
                                        &cbRequired,
                                        0);

		if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
			PSP_DEVICE_INTERFACE_DETAIL_DATA pdidd =
			 (PSP_DEVICE_INTERFACE_DETAIL_DATA) LocalAlloc(LPTR, cbRequired);

			if (!pdidd) {
				log_err("battery_init: LocalAlloc failed, size %d", cbRequired);
				SetupDiDestroyDeviceInfoList(hdev);
				return -1;
			}

			pdidd->cbSize = sizeof(*pdidd);
            if (SetupDiGetDeviceInterfaceDetail(hdev,
                                                &did,
                                                pdidd,
                                                cbRequired,
                                                &cbRequired,
                                                0))
            {
				// Enumerated a battery.  Ask it for information.
				HANDLE hBattery = CreateFile(pdidd->DevicePath,
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL);
				if (INVALID_HANDLE_VALUE != hBattery) {
					//printf("battery handle: %d\n", hBattery);
					//_bat[idev] = hBattery;
					_bathandle  = hBattery;
					LocalFree(pdidd);
					break;
				} else {
					log_warn("battery_init: failed to open device file: %s", pdidd->DevicePath);
				}
			} else {
					log_warn("battery_init:SetupDiGetDeviceInterfaceDetail failed");
			}

			LocalFree(pdidd);
		} // end if ERROR_INSUFFICIENT_BUFFER

	} // end for
      
	SetupDiDestroyDeviceInfoList(hdev);

	if (_bathandle != 0) {
		BATTERY_INFORMATION BatteryInfo;

		memset(&BatteryInfo, 0, sizeof(BATTERY_INFORMATION));
	
		if (BatteryQueryInformation(_bathandle,
									BatteryInformation,
									(LPVOID)&BatteryInfo,
									sizeof(BatteryInfo))) {
			_max_capacity = (float) BatteryInfo.FullChargedCapacity;
		} else {
			log_err("battery_info: failed to retrieve the maximum capacity of the battery");
			 CloseHandle(_bathandle);
			 _bathandle = 0;
			return -1;
		}

		battery_print_info(_bathandle);
	}
	printf("battery_init end \n");
	return 0;
}

int battery_is_present() {
	return _bathandle != 0;
}

static ULONG BatteryGetTag(HANDLE hHandle)
{
    ULONG uBatteryTag = 0;
    DWORD cbBytesReturned, dwTmp = 0;

    if (!DeviceIoControl(hHandle,
                         IOCTL_BATTERY_QUERY_TAG,
                         &dwTmp,
                         sizeof(DWORD),
                         &uBatteryTag,
                         sizeof(ULONG),
                         &cbBytesReturned,
                         NULL))
    {
        return 0;
    }

    return uBatteryTag;
}

// bool
static BOOL BatteryQueryInformation(HANDLE hHandle,
                        BATTERY_QUERY_INFORMATION_LEVEL InfoLevel,
                        LPVOID lpBuffer,
                        DWORD dwBufferSize)
{
    BATTERY_QUERY_INFORMATION BatteryQueryInfo;
    DWORD cbBytesReturned;

    BatteryQueryInfo.BatteryTag = BatteryGetTag(hHandle);
    if (!BatteryQueryInfo.BatteryTag)
        return FALSE;

    BatteryQueryInfo.InformationLevel = InfoLevel;
    return DeviceIoControl(hHandle,
                           IOCTL_BATTERY_QUERY_INFORMATION,
                           &BatteryQueryInfo,
                           sizeof(BATTERY_QUERY_INFORMATION),
                           lpBuffer,
                           dwBufferSize,
                           &cbBytesReturned,
                           NULL);
}

// bool
static BOOL BatteryQueryStatus(HANDLE hHandle,
                   BATTERY_STATUS *lpBatteryStatus,
                   DWORD dwBufferSize)
{
    BATTERY_WAIT_STATUS BatteryWaitStatus = {0};
    DWORD cbBytesReturned;

    BatteryWaitStatus.BatteryTag = BatteryGetTag(hHandle);
    if (!BatteryWaitStatus.BatteryTag)
        return FALSE;

    return DeviceIoControl(hHandle,
                           IOCTL_BATTERY_QUERY_STATUS,
                           &BatteryWaitStatus,
                           sizeof(BATTERY_WAIT_STATUS),
                           lpBatteryStatus,
                           dwBufferSize,
                           &cbBytesReturned,
                           NULL);
}

/*
static BOOL
EnumBatteryDevices(BATTERYENUMPROC lpBatteryEnumProc)
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA pDevDetail;
    SP_DEVICE_INTERFACE_DATA spDeviceData;
    DWORD cbRequired;
    HDEVINFO hDevInfo;
    INT iBattery = 0;

    hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY,
                                   0, 0,
                                   DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    for (;;)
    {
        ZeroMemory(&spDeviceData, sizeof(SP_DEVICE_INTERFACE_DATA));
        spDeviceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        if (!SetupDiEnumDeviceInterfaces(hDevInfo,
                                         0,
                                         &GUID_DEVCLASS_BATTERY,
                                         iBattery,
                                         &spDeviceData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
        }

        cbRequired = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo,
                                        &spDeviceData,
                                        0, 0,
                                        &cbRequired, 0);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            continue;

        pDevDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA) LocalAlloc(LPTR, cbRequired);
        if (!pDevDetail)
            continue;

        pDevDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetail(hDevInfo,
                                             &spDeviceData,
                                             pDevDetail,
                                             cbRequired,
                                             &cbRequired,
                                             0))
        {
            LocalFree(pDevDetail);
            continue;
        }

        if (!lpBatteryEnumProc(pDevDetail->DevicePath))
        {
            LocalFree(pDevDetail);
            SetupDiDestroyDeviceInfoList(hDevInfo);
            return FALSE;
        }

        LocalFree(pDevDetail);
        ++iBattery;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    return TRUE;
}
*/

int battery_get_max_capacity(float* max_capacity)
{
	*max_capacity = _max_capacity;
	return 0;
}

int battery_get_current_info(
	float* current_capacity,
	float* life_percent)
{
	BATTERY_MANUFACTURE_DATE BatteryDate;
    BATTERY_STATUS BatteryStatus;

	assert(current_capacity);
	assert(life_percent);

	memset(&BatteryDate, 0, sizeof(BATTERY_MANUFACTURE_DATE));
	memset(&BatteryStatus, 0, sizeof(BATTERY_STATUS));
		
    if (BatteryQueryStatus(_bathandle,
                           &BatteryStatus,
                           sizeof(BatteryStatus)))
    {
        // Current capacity 
        //Index = IoAddValueName(1, IDS_BAT_CURRENT_CAPACITY, 1);
        //printf("Current capacity : %ld mWh (%ld%%)\n",
        //               BatteryStatus.Capacity,
        //               (BatteryStatus.Capacity * 100) / BatteryInfo.FullChargedCapacity
		//			   );

		*current_capacity = (float)BatteryStatus.Capacity;
        //printf("battery_get_info Capacity: %f mWh\n", *current_capacity);

		*life_percent = ((float) BatteryStatus.Capacity * 100.0f) / _max_capacity;
        //printf("battery_get_info Capacity: %f mWh\n", *life_percent);

        // Voltage 
        //Index = IoAddValueName(1, IDS_BAT_VOLTAGE, 1);
		if (BatteryStatus.Voltage == BATTERY_UNKNOWN_VOLTAGE ) {
			BatteryStatus.Voltage = 0;
		}

        //printf("Voltage :%ld mV\n", BatteryStatus.Voltage);

		//printf("Rate :%ld mV\n", BatteryStatus.Rate);

        // Status 
        //Index = IoAddValueName(1, IDS_STATUS, 1);
        //BatteryPowerStateToText(BatteryStatus.PowerState, szText, sizeof(szText));
    }
	
	return 0;
}

int battery_print_info(HANDLE hbat)
{
	BATTERY_MANUFACTURE_DATE BatteryDate;
    BATTERY_INFORMATION BatteryInfo;
    BATTERY_STATUS BatteryStatus;
    //WCHAR szText[MAX_STR_LEN];

	memset(&BatteryDate, 0, sizeof(BatteryDate));
	memset(&BatteryInfo, 0, sizeof(BatteryInfo));
	memset(&BatteryStatus, 0, sizeof(BatteryStatus));

	if (BatteryQueryInformation(hbat,
                                BatteryInformation,
                                (LPVOID) &BatteryInfo,
                                sizeof(BatteryInfo)))
    {
        // Capacity
        printf("Capacity: %ld mWh\n", BatteryInfo.DesignedCapacity);

        // Type
        //GetBatteryTypeString(BatteryInfo, szText, sizeof(szText));
       
		// printf("Capacity: %d\n", );

        // Full charged capacity	
        //Index = IoAddValueName(1, IDS_BAT_FULL_CAPACITY, 1);
		 printf("FullChargedCapacity: %ld mWh\n", BatteryInfo.FullChargedCapacity);

        // Depreciation
        //Index = IoAddValueName(1, IDS_BAT_DEPRECIATION, 1);

		printf("Depreciation: %ld mWh\n", 100 - (BatteryInfo.FullChargedCapacity * 100) /
                       BatteryInfo.DesignedCapacity);
    }

	
    if (BatteryQueryStatus(hbat,
                           &BatteryStatus,
                           sizeof(BatteryStatus)))
    {
        // Current capacity 
        //Index = IoAddValueName(1, IDS_BAT_CURRENT_CAPACITY, 1);
        printf("Current capacity : %ld mWh (%ld%%)\n",
                       BatteryStatus.Capacity,
                       (BatteryStatus.Capacity * 100) / BatteryInfo.FullChargedCapacity
					   );


        // Voltage 
        //Index = IoAddValueName(1, IDS_BAT_VOLTAGE, 1);
		if (BatteryStatus.Voltage == BATTERY_UNKNOWN_VOLTAGE ) {
			BatteryStatus.Voltage = 0;
		}

        printf("Voltage :%ld mV\n", BatteryStatus.Voltage);
		printf("Rate :%ld mV\n", BatteryStatus.Rate);

        // Status 
        //Index = IoAddValueName(1, IDS_STATUS, 1);
        //BatteryPowerStateToText(BatteryStatus.PowerState, szText, sizeof(szText));
    }
	
	return 0;
}

int battery_stop() {
	return CloseHandle(_bathandle);
}