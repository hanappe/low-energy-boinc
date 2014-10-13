#include "SonyFilter.h"

//
//   64 bit assembler is in helper.asm in the AMD64 subdir of the build dir
//  This is because the 64 bit compiler does not support inine assembly
//

ULONG64	RdMsr(ULONG	RdMsrId)
{
  ULONG64		MsrValue = 0;
  ULONG			MsrValueLo = 0;
  ULONG			MsrValueHi = 0;

#if defined X64ARCH
  MsrValue = RdMsr64(RdMsrId);	// call into the func in helper.asm for x64 CPUs
#endif

#if defined X86ARCH
  __asm mov ecx, RdMsrId
  __asm rdmsr            
  __asm mov MsrValueLo, Eax 
  __asm mov MsrValueHi, Edx
   MsrValue = MsrValueLo;
  *(((ULONG*)&MsrValue)+1) = MsrValueHi;
#endif
  return MsrValue;

}


#ifdef X86ARCH
VOID WrMsrExINTEL(ULONG lo)
{
  __asm mov ecx, IA32_PERF_STS
  __asm rdmsr  
  __asm and edx, 1
  __asm mov ecx, IA32_PERF_CTL
  __asm mov eax, lo
  __asm wrmsr
}

VOID WrMsrExAMD(ULONG lo)
{
  __asm mov edx, 0
  __asm mov ecx, AMD_PERF_CTL
  __asm mov eax, lo
  __asm wrmsr
}

#endif


CPU_TYPE GetProcessorManufAndMSRSupport(VOID)
{
	LONG	array[4];
	LONG	val;
	LONG	val1;
	PCHAR	buffer = ExAllocatePoolWithTag(NonPagedPool, 1024, 'ynos');

	if(buffer)
	{
		memset(buffer, 0, 1024);
	  
		// get the manufacturer
		__cpuid(array, 0);

		((int *)buffer)[0] = array[1];
		((int *)buffer)[1] = array[3];
		((int *)buffer)[2] = array[2];

		KdPrint(("	CPU manufacturer is %s\n", buffer));


		if(strncmp(buffer, "GenuineIntel", 4 * 3) == 0)
		{
			ExFreePool(buffer);
			
			__cpuid(array, 1);

			// Intel_CPUID_Jan2011.pdf Bits 8 to 11 store the family, Pentium chips have values of 0101 and higher
			// CPUs from Pentium on support MSRs
			val =  (array[0] >> 8 ) & 0xF;

			KdPrint(("	CPU family is 0x%X\n", val));


			if( val >=  0x5) 
				return INTEL;

			else
				return UNSUPPORTED;
		}
		else if(strncmp(buffer, "AuthenticAMD", 4 * 3) == 0)
		{
			ExFreePool(buffer);

			__cpuid(array, 1);

			//AMD CPUID SPEC Sept 2010 pdf Bits 8 to 11 store the base family, and if 0xF, bits 20 to 27 store the extended family
			// in which case add base and extended to get the full family
			val =  (array[0] >> 8 ) & 0xf;
			
			if(val == 0xf)
			{
				val1 = (array[0] >> 20) & 0xff;
				val += val1;
			}

			KdPrint(("	CPU family is 0x%X  EDX is 0x%X\n", val, array[3]));
			
			// bit 5 of EDX which gets returned in array[3] tells us about support for WRMSR and RDMSR
			// which are detailed in AMD64 Architecture Programmer’s Manual Volume 2
			if(array[3] & 0x20)
			{
				KdPrint(("	MSRs are supported\n"));
				return AMD;
			}
			else
			{
				KdPrint(("	MSRs are not supported\n"));
				return UNSUPPORTED;
			}
		}
	}
	return UNSUPPORTED;
}
