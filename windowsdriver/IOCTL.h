

// include this is user mode apps too

//#define	IOCTL_LPC_FLTR_LET_CPU_PSTATE		CTL_CODE(FILE_DEVICE_SERIAL_PORT, 0x904 , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_LPC_FLTR_SET_CPU_PSTATE		CTL_CODE(FILE_DEVICE_SERIAL_PORT, 0x905 , METHOD_BUFFERED, FILE_ANY_ACCESS)
//#define	IOCTL_LPC_FLTR_CPU_STAY_ALIVE		CTL_CODE(FILE_DEVICE_SERIAL_PORT, 0x906 , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_LPC_FLTR_SETNOW_CPU_PSTATE		CTL_CODE(FILE_DEVICE_SERIAL_PORT, 0x907 , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LPC_FLTR_GET_STATS				CTL_CODE(FILE_DEVICE_SERIAL_PORT, 0x908, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LPC_FLTR_SET_CFG				CTL_CODE(FILE_DEVICE_SERIAL_PORT, 0x909, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LPC_FLTR_GET_CFG				CTL_CODE(FILE_DEVICE_SERIAL_PORT, 0x90A, METHOD_BUFFERED, FILE_ANY_ACCESS)

// The stats are returned in a call to IOCTL_LP_CPU_FLTR_STAY_ALIVE by secifying an output buffer of the size of this struct
// The data is the number of continuous Stay alive limit hits, ie, the number of times the driver stopped setting the CPU low 
// because it didnt get the stay alive message before the stay alive failure limit got reached.
// The stats data also contains the number of CPUs and the cumlative count those cores have spent at the frequencies supported by the CPU.
// This stats data is reset on read, so gives an instantaneous picture of whether the calling code is setting the 
// stay alive failure limit high enough for the given load on the machine. (with greater loading the calling thread will be descheduled
// whcih means it cant call stay alive as often as needed to keep the CPU speed set low).
// The ideal is for this stat to be zero until some other work is done on the machine, whereupon it should increase.
// Values to support this on a quad core i5 were ThreadDelay of 20 ms and a StayAliveLimit of 50. 
// It also gives an instantaneous image of the time each CPU has spent at what frequency.

#pragma pack(push, 1)
typedef struct _LPCCPUFrequency
{
	ULONG	Status;
	ULONG	CPUFrequency;
	ULONG64	Time;
} LPCCPUFrequency, *PLPCCPUFrequency;

// For a twin core with 4 frequency states then the memory will look like:
// LPCPstateStats LPCCPUFrequency LPCCPUFrequency LPCCPUFrequency LPCCPUFrequency    LPCCPUFrequency LPCCPUFrequency LPCCPUFrequency LPCCPUFrequency
typedef struct _LPCPstateStats
{
	ULONG			StayAliveFailureHits;
	ULONG64			TimeStamp;
	ULONG			NumCPUs;
	ULONG			NumFrequencies;
}
LPCPstateStats, *PLPCPstateStats;  

typedef struct _LPCConfiguration
{
        ULONG ThreadDelay;
        ULONG StatsSampleRate;
        ULONG StayAliveFailureLimit;
} LPCConfiguration, *PLPCConfiguration; 

#pragma pack (pop)

