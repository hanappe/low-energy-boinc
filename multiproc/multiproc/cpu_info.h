
#ifndef _CPUINFO_H_
#define _CPUINFO_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct host_info_t {
    int timezone;                 // local STANDARD time - UTC time (in seconds)
    char domain_name[256];
    char serialnum[256];
    char ip_addr[256];
    char host_cpid[64];

    int p_ncpus;
    char p_vendor[256];
    char p_model[256];
    char p_features[1024];
    double p_fpops;
    double p_iops;
    double p_membw;
    double p_calculated;          // when benchmarks were last run, or zero
    bool p_vm_extensions_disabled;

    double d_total;               // Total amount of disk in bytes
    double d_free;                // Total amount of free disk in bytes

    char os_name[256];
    char os_version[256];

	// Memory info
	double m_nbytes;              // Total amount of memory in bytes
    double m_cache;
    double m_swap;                // Total amount of swap space in bytes

    char mac_address[256];      // MAC addr e.g. 00:00:00:00:00:00
                                // currently populated for Android

    //int get_local_network_info();
};


/// PROCESSOR

int get_cpuid(unsigned int info_type, unsigned int& a, unsigned int& b, unsigned int& c, unsigned int& d);

int get_processor_vendor(char* name, int name_size);
int get_processor_version(int& family, int& model, int& stepping);
int get_processor_name(char* name, int name_size);
int get_processor_count(int& processor_count);

int get_processor_cache(int& cache);

int get_processor_info(
    char* p_vendor, int p_vendor_size, char* p_model, int p_model_size,
    char* p_features, int p_features_size, double& p_cache, int& p_ncpus);

/// OS
int get_os_info(char* os_name, int os_name_size, char* os_version, int os_version_size);

/// Memory

int get_memory_info(double* bytes, double* swap);

/// HOST

// if running on batteries return value = 0
int is_running_on_batteries();

void clear_host_info(host_info_t* hi);
int get_host_info(host_info_t* hi);

void print_host_info(host_info_t* hi);

#ifdef __cplusplus
}
#endif

#endif // _CPUINFO_H_
