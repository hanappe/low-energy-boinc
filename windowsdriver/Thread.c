#include "SonyFilter.h"





//
// So this thread cycles through the CPUS reading the CPU speed and setting the stats, if the stats sample rate is satsfied.
// Additionally, if the pDevExt->pCPUDB[<procnum>].PSate has a value other than 0xFFFFFFFF then set the CPU speed to that PState 
//

VOID	PerfCtlThread(IN PVOID  Context)
{
	PDEVICE_EXTENSION2		pDevExt = (PDEVICE_EXTENSION2)Context;
	ULONG					Exceptions = 0;
	ULONG					CyclesPerSecond;
	ULONG					CycleCount = 0;
	ULONG64					oldtime = 0;

	// calculate how many times the thread goes round per second   WDF_REL_TIMEOUT_IN_MS()
	CyclesPerSecond = 1000 / pDevExt->Config.ThreadDelay;

	KdPrint(("LPCFILTER: PerfCtlThread  Enter with %d ms delay, %d stay alive limit, %d Stats per second \n", 
											pDevExt->Config.ThreadDelay, 
											pDevExt->Config.StayAliveFailureLimit,
											pDevExt->Config.StatsSampleRate));
	
	while(pDevExt->ThreadGo)
	{
		ULONG			x = 0;
		KAFFINITY		tempAffinity;
		BOOLEAN			statstaken	= FALSE;
		ULONG64			nowtime = 0;
		ULONG64			difftime = 0;
		LARGE_INTEGER	largeint1;
		NTSTATUS		Status;
		LARGE_INTEGER	waittimeout;
		KAFFINITY		oldAffinity;
		ULONG			ThreadDelay;


		waittimeout.LowPart = (pDevExt->Config.ThreadDelay * -10000);  // units of 100 nanoseconds,  100 x 10^-9
		waittimeout.HighPart = -1;


		// go to sleep for our specified time
		Status = KeDelayExecutionThread(KernelMode, FALSE, &waittimeout);



		//
		//	For each CPU switch to it 
		//
		for(x = 0 ; x < pDevExt->NumProcs ; x++)
		{


#if defined X64ARCH
			tempAffinity = (1i64 << x);// If you ever wondered what 1i64 is and 
			// why such an odd way of representing '1' is needed
			// take a look at http://msdn.microsoft.com/en-us/library/ke55d167.aspx
#elif defined X86ARCH
			tempAffinity = (1 << x);
#endif 

			// if it is a multi proc system switch to the next CPU
			if(pDevExt->NumProcs > 1)
			{
				// first time though make a note of the original affinity
				if(x == 0)
				{
					oldAffinity = KeSetSystemAffinityThreadEx(tempAffinity ); 
				}
				else
				{
					KeSetSystemAffinityThreadEx(tempAffinity ); 
				}
			}




			//
			//  We need to keep stats on the CPU frequencies as requested
			//

			if( pDevExt->Config.StatsSampleRate && (CycleCount == (CyclesPerSecond / pDevExt->Config.StatsSampleRate)) )
			{
				KIRQL				kirql;
				ULONG64				MsrData = 0;
				ULONG				y;
				PLPCCPUFrequency	pFreqData;

				// if it is the first time round
				if(statstaken == FALSE)
				{
					KeQueryTickCount (&largeint1); 
					memcpy(&nowtime, &largeint1, sizeof(ULONG64));
					nowtime = nowtime * pDevExt->MillisPerTick;

					difftime = nowtime - oldtime;
					oldtime = nowtime;
				}
				
				// set the flag to show we took stats already, we will rest the cyclecount later, #
				// but also reuse the same tick count for all the CPUs stats data
				statstaken = TRUE;

				//
				// Read the IA32_PERF_STS register and for that value increment the hit count for the CPU
				//

				
				KeRaiseIrql(HIGH_LEVEL, &kirql);

				try
				{
					ULONG	MSR_RANGE_MASK = 0;

					if(pDevExt->PCTAccessType == ADDRESS_SPACE_FFHW)
					{
						if(pDevExt->CpuManuf == INTEL)
						{
							MsrData = RdMsr(IA32_PERF_STS);
							MSR_RANGE_MASK = INTEL_MSR_RANGE;
						}
						else if(pDevExt->CpuManuf == AMD)
						{
							MsrData = RdMsr(AMD_PERF_STS);
							MSR_RANGE_MASK	= AMD_MSR_RANGE;
						}
					}
					
					pDevExt->pStats->TimeStamp = nowtime;

					pFreqData = (PLPCCPUFrequency)  (  ((PUCHAR)pDevExt->pStats) + 
															sizeof(LPCPstateStats) +  
															(sizeof(LPCCPUFrequency) * x * pDevExt->pStats->NumFrequencies)  );

					for(y = 0 ; y < pDevExt->pStats->NumFrequencies ; y++)
					{
						if(pFreqData[y].Status == (MsrData & MSR_RANGE_MASK))
						{
							pFreqData[y].Time += difftime;
							break;
						}
					}

					// on some CPUs rdmsr(CPU_PERF_STS) returns a value not in the reported list of values
					// so add the time to the highest speed
					if(y == pDevExt->pStats->NumFrequencies)
						pFreqData[0].Time += difftime;


					KeLowerIrql(kirql);
				}
				except(EXCEPTION_EXECUTE_HANDLER)  // if we dont mask out the top 231 bits on edx for armsr we get an exception on x64 CPUs
				{ 
					NTSTATUS				status = STATUS_SUCCESS;

					KeLowerIrql(kirql);

					status = GetExceptionCode();

					KdPrint(("LPCFILTER: 	Exception thrown reading MSR status is 0x%X\n", status));
					Exceptions++;
					if(Exceptions > 10) // if we got this many exceptions in a row then give up
						pDevExt->ThreadGo = FALSE;
				}

				
			}// end if(CycleCount == (CyclesPerSecond ....



			//
			// So we have our stats data, now we need to set the CPU speed if we have been told to do so
			//

			if(pDevExt->pCPUDB[x].PState !=  0xFFFFFFFF)
			{
				ULONG					MsrDataLo = 0;
				ULONG					MsrDataHi = 0;
				PACPI_METHOD_ARGUMENT	pArg;
				ULONG					index = 0;
				PPSSData				pPSSDataLow = NULL;
				PPSSData				pPSSDataHigh = NULL;
				ULONG					StayAlive;
				ULONG					StayAliveFailureLimit;
				KIRQL					kirql;
				ULONG					MSR_RANGE_MASK = 0;


				pArg = &(((PACPI_EVAL_OUTPUT_BUFFER)pDevExt->pPSSData)->Argument[0]);
				
				// Loop through the PPSData to the highest and lowest set value
				for(index = 0 ; (index < ((PACPI_EVAL_OUTPUT_BUFFER)pDevExt->pPSSData)->Count) && (index <= pDevExt->pCPUDB[x].PState) ; index++) 
				{
					if(index == 0)
						pPSSDataHigh = (PPSSData)&(pArg->Data[0]);

					pPSSDataLow = (PPSSData)&(pArg->Data[0]);
					
					pArg = ACPI_METHOD_NEXT_ARGUMENT(pArg);
				}

				if(pDevExt->CpuManuf == INTEL)
				{
					MSR_RANGE_MASK = INTEL_MSR_RANGE;
				}
				else if(pDevExt->CpuManuf == AMD)
				{
					MSR_RANGE_MASK = AMD_MSR_RANGE;
				}

				MsrDataLo = (MsrDataLo & ~MSR_RANGE_MASK) | (pPSSDataLow[4].Data & MSR_RANGE_MASK);
				MsrDataHi = (MsrDataLo & ~MSR_RANGE_MASK) | (pPSSDataHigh[4].Data & MSR_RANGE_MASK);




				// if the app didnt set StayAlive to 0 then after a period of time, StayAliveFailureLimit times the thread delay, 
				// we stop setting the CPU low since the app probably got descheduled due to intensive work by another
				// process
				StayAlive = InterlockedIncrement(&pDevExt->pCPUDB[x].StayAliveCount);

				StayAliveFailureLimit = pDevExt->Config.StayAliveFailureLimit;


				KeRaiseIrql(HIGH_LEVEL, &kirql);

				//
				//  Set the CPU speed low, or to high once before we stop managing that CPU
				//

				if(StayAlive < (StayAliveFailureLimit + 1))
				{
					try
					{
						if(StayAlive < StayAliveFailureLimit)
						{
							if(pDevExt->PCTAccessType == ADDRESS_SPACE_FFHW)
							{
#if defined X86ARCH
								if(pDevExt->CpuManuf == INTEL)
								{
									WrMsrExINTEL(MsrDataLo);
								}
								else if(pDevExt->CpuManuf == AMD)
								{
									WrMsrExAMD(MsrDataLo);
								}
#endif

#if defined X64ARCH
								if(pDevExt->CpuManuf == INTEL)
								{
									WrMsr64ExINTEL(MsrDataLo);
								}
								else if(pDevExt->CpuManuf == AMD)
								{
									WrMsr64ExAMD(MsrDataLo);
								}
#endif
							}
						}
						else// when stayalive is at StayAliveFailureLimit we set the CPU high, then dont touch it
						{
							if(pDevExt->PCTAccessType == ADDRESS_SPACE_FFHW)
							{
#if defined X86ARCH
								if(pDevExt->CpuManuf == INTEL)
								{
									WrMsrExINTEL(MsrDataHi);
								}
								else if(pDevExt->CpuManuf == AMD)
								{
									WrMsrExAMD(MsrDataHi);
								}
#endif

#if defined X64ARCH
								if(pDevExt->CpuManuf == INTEL)
								{
									WrMsr64ExINTEL(MsrDataHi);
								}
								else if(pDevExt->CpuManuf == AMD)
								{
									WrMsr64ExAMD(MsrDataHi);
								}
#endif
							}
						}

 						KeLowerIrql(kirql);

						Exceptions = 0; // if we got here we didnt get an exception so we can reset our counter
					}
					except(EXCEPTION_EXECUTE_HANDLER)  // if we dont mask out the top 231 bits on edx for armsr we get an exception on x64 CPUs
					{ 
						NTSTATUS				status = STATUS_SUCCESS;

						KeLowerIrql(kirql);

						status = GetExceptionCode();

						KdPrint(("LPCFILTER: 	Exception thrown writing MSR status is 0x%X\n", status));
						Exceptions++;
						if(Exceptions > 10) // if we got this many exceptions in a row then give up
							pDevExt->ThreadGo = FALSE;
					}
				}
				else
				{
					KeLowerIrql(kirql);
					
					// the app stopped setting the CPU state so we assume it isnt interested in managing this CPU 
					InterlockedExchange(&pDevExt->pCPUDB[x].PState, 0xFFFFFFFF);

					InterlockedIncrement(&pDevExt->pStats->StayAliveFailureHits);
				}
			}
		} // end For(x - 0 ; ....



		// reset out thread cycle count if we took stats or increment it
		if(statstaken)
			CycleCount = 0;
		else
			CycleCount++;


		// now revert to the old affinity if we changed it
		if(pDevExt->NumProcs > 1)
		{
			KeRevertToUserAffinityThreadEx(oldAffinity);
		}

	}// end while(pDevExt...

	KdPrint(("LPCFILTER: PerfCtlThread  Exit\n"));

	PsTerminateSystemThread(STATUS_SUCCESS);
}