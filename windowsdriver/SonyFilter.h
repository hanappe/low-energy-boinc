#include <wdm.h> //<ntddk.h>
#include "stdarg.h"
#include "stdio.h"
#include <Ntstrsafe.h>
#include <excpt.h>
#include <Acpiioct.h>
#include "ioctl.h"


typedef enum _CPU_TYPE{INTEL, AMD, UNSUPPORTED} CPU_TYPE;


#define INTEL_MSR_RANGE		(0xffff)

// Intel register values for MSR
#define IA32_PERF_STS   0x198
#define IA32_PERF_CTL   0x199


#define AMD_MSR_RANGE		(0xF)

// AMD register values for MSR
#define AMD_PERF_CTL	0xC0010062
#define AMD_PERF_STS	0xC0010063



#ifndef BYTE
#define BYTE UCHAR
#endif


#define ADDRESS_SPACE_SYSMEM 0
#define ADDRESS_SPACE_SYSIO 1
#define ADDRESS_SPACE_PCI 2
#define ADDRESS_SPACE_EMBED 3
#define ADDRESS_SPACE_SMBUS 4
#define ADDRESS_SPACE_FFHW 0x7f


#pragma pack (push)
#pragma pack (1)



//
//  Data and data type definitions
//

//  Eval methods Ex returns this size data for _PSS
typedef struct _PSSData
{
	USHORT	Type;
	USHORT	Length;
	ULONG	Data;
	ULONG	Padding;
} PSSData, *PPSSData;

//  Eval Methods returns this size data for _PSS
typedef struct _PSSData1
{
	USHORT	Type;
	USHORT	Length;
	ULONG	Data;
} PSSData1, *PPSSData1;



typedef struct _PCTData
{
	BYTE	GeneralRegisterDescriptor;	//0x82
	USHORT	Length;						// 0x000C
	BYTE	AddressSpaceKeyword;		// 0x00 System Memory,    0x01 System IO,   0x02  PCI Config Space,   0x03  Embedded Controller,   0x04 SMBus,  0x7F  Func Fixed HW
	BYTE	RegisterBitWidth;			// register width in bits
	BYTE	RegisterBitOffset;			// offset to start of register in bits from the Register Address
	BYTE	AddressSize;				// 0 undefined, 1 Byte, 2 Word, 3 Dword, 4 Qword
	ULONG64	RegisterAddress;			
	BYTE	AccessSize;					// optional
	BYTE	DescriptorName;				// optional
}PCTData, *PPCTData;



typedef struct _CPUDataBase
{
	ULONG	PState;					// tells the driver the power state to run the CPU at, set by the app
	ULONG	StayAliveCount;			// if this gets to 10 then we stop setting the CPU low.  This is reset to zero by the app
} CPUDataBase, *PCPUDataBase;


#pragma pack (pop)



//
// Device Extension forward declaration for the control device object (for the app to use)
//
typedef struct _DEVICE_EXTENSION2 DEVICE_EXTENSION2, *PDEVICE_EXTENSION2;

//
// Device Extension type.  There is a device object for the filter, and one for the app to use
//
typedef enum _TYPE{CONTROL, FILTER} TYPE;


//
// Device Extension for filter object
//
typedef struct _DEVICE_EXTENSION {

	TYPE					Type;					// This ia FILTER. Since the filter and the controll DO use the same dispatch entry points
													// this flag lets the code know which DO the Irp is for
													
	PDEVICE_EXTENSION2		pOtherExt;				// points to the CONTROL device extension

	PDEVICE_OBJECT			DeviceObject;			// filter device object

    PDEVICE_OBJECT			TargetDeviceObject;		 // Target Device Object (what the filter is sitting on top of)

	IO_REMOVE_LOCK			RemoveLock;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Device extension for control object (one with the symbolic link that can be opened by user mode apps)
//
typedef struct _DEVICE_EXTENSION2
{
	TYPE					Type;					// this will always be CONTROL

	PDEVICE_EXTENSION		pOtherExt;				// points to the FILTER device extension

    PDEVICE_OBJECT			DeviceObject;			// control device object

	WCHAR					linkNamestr[32];		// symbolic link name
	UNICODE_STRING			linkName;

	BOOLEAN					IsDataObtained;			// since we have to get the data at CreateFile we have to protect 
													// it so we dont re-get it when another app opens the driver.
	PUCHAR					pPCTData;				// CPU register data
	PUCHAR					pPSSData;				// CPU frequency data
	PUCHAR					pChildEnumData;			// ACPI namespace names

	CPU_TYPE				CpuManuf;				// Intel or AMD
	
	ULONG					PCTAccessType;

	ULONG					NumProcs;
	
	BOOLEAN					ThreadGo;				// controls thread execution, when FALSE the thread will exit

	PLPCPstateStats			pStats;					// Stats data. 

	ULONG					StatsDataSize;

	HANDLE					DispatcherThreadHandle;
	
	PKTHREAD				DispatcherThreadObject;

	ULONG					MillisPerTick;

	LPCConfiguration		Config;

	PCPUDataBase			pCPUDB;

	KSPIN_LOCK				AppLock;			// so the control device is only touched by one app at a time

} DEVICE_EXTENSION2, *PDEVICE_EXTENSION2;



//
// Function declarations
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
FPFilterForwardIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FPFilterAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    );


NTSTATUS
FPFilterDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FPFilterDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FPFilterStartDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FPFilterRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FPFilterSendToNextDriver(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FPFilterGenericIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


VOID
FPFilterUnload(
    IN PDRIVER_OBJECT DriverObject
    );


NTSTATUS
FPFilterIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );



VOID	PerfCtlThread(IN PVOID  Context);

VOID EvaluateMethods( 
	IN PDEVICE_EXTENSION2 pDevExt, PACPI_ENUM_CHILD		pChild);

VOID EvaluateMethodsEx( 
	IN PDEVICE_EXTENSION2 pDevExt, PDEVICE_OBJECT pTargetDeviceObject, PACPI_ENUM_CHILD		pChild);

VOID EvalMethods(IN PDEVICE_EXTENSION2 pDevExt);

VOID EvalMethodsEx(IN PDEVICE_EXTENSION2 pDevExt, PDEVICE_OBJECT pTargetDeviceObject);

NTSTATUS GetACPIPstateData(PDEVICE_EXTENSION2 pDevExt, PDEVICE_OBJECT pTargetDeviceObject);

VOID DumpKids(PACPI_ENUM_CHILDREN_OUTPUT_BUFFER pACPIChild);

VOID	DumpEval(PACPI_EVAL_OUTPUT_BUFFER pACPIEval, PACPI_EVAL_INPUT_BUFFER pBuf);

ULONG64 RdMsr(ULONG Register);

CPU_TYPE	GetProcessorManufAndMSRSupport(VOID);

#if defined X86ARCH
VOID	WrMsrExINTEL(ULONG lo);
VOID	WrMsrExAMD(ULONG lo);
#endif

#ifdef X64ARCH
extern ULONG64	RdMsr64(ULONG Register);
extern VOID		WrMsr64ExINTEL(ULONG lo);
extern VOID		WrMsr64ExAMD(ULONG lo);
#endif
