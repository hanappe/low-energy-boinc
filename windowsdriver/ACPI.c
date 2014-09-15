#include "SonyFilter.h"


/////////////////////////////////////////////  SOME DEBUG STUFF
// debug dumps a chunk of hex data
#ifdef DBG
VOID DumpFrame(UCHAR * d, SIZE_T	t)
{
	SIZE_T	i, v;
	SIZE_T	remains;

	if( t > 12)
	{
		remains = t % 12;

		KdPrint(("LPCFILTER: \n\n"));

		for(i = 0 ; i < (t - remains) ; i+=12)
		{
			KdPrint(("LPCFILTER: %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X\n", d[i], d[i+1], d[i+2], d[i+3], d[i+4], d[i+5], d[i+6], d[i+7], d[i+8],d[i+9],d[i+10],d[i+11]));
		}

		KdPrint(("\n"));

		for(v = i ; v < i + remains ; v+=2)
		{
			KdPrint(("LPCFILTER: %.2X %.2X  ", d[v], d[v+1]));

		}

		KdPrint(("\n\n"));

	}
	else
	{
		KdPrint(("\n\n"));

		for(v = 0 ; v < t ; v+=2)
		{
			KdPrint(("LPCFILTER: %.2X %.2X  ", d[v], d[v+1]));
		}

		KdPrint(("\n\n"));

	}
}



//
//  This code is ued to dump out ACPI data from the conversation between the CPU driver and acpi.sys
//
VOID DumpKids(PACPI_ENUM_CHILDREN_OUTPUT_BUFFER pACPIChild)
{
	PACPI_ENUM_CHILD		pChild;
	ULONG					index;

	pChild = &(  ((PACPI_ENUM_CHILDREN_OUTPUT_BUFFER)(pACPIChild))->Children[0]);	

	for(index = 0 ; index < ((PACPI_ENUM_CHILDREN_OUTPUT_BUFFER)(pACPIChild))->NumberOfChildren ; index++)
	{
		DbgPrint("Children returned %s\n", pChild->Name);

/*		if( (strstr( pChild->Name, "_PCT"))
			||
			(strstr( pChild->Name, "_PSS"))
			||
			(strstr( pChild->Name, "SPSS"))
			||
			(strstr( pChild->Name, "XPSS")) )
		{

			DbgPrint("\n	EvaluateMethods:\n");

			EvaluateMethods(pDevExt, pChild);
		}
		else
		{
			DbgPrint("Ignoring %s\n", pChild->Name);
		}
*/
		pChild = ACPI_ENUM_CHILD_NEXT(pChild);
	}
}

//
//		This code is ued to dump out ACPI data from the conversation between the CPU driver and acpi.sys
//
VOID	DumpEval(PACPI_EVAL_OUTPUT_BUFFER pACPIEval, PACPI_EVAL_INPUT_BUFFER pBuf)
{
	PACPI_METHOD_ARGUMENT		pArg;
	ULONG						index;



	pArg = &(pACPIEval->Argument[0]);
	for(index = 0 ; index < pACPIEval->Count ; index++)
	{
		switch(pArg->Type)
		{
			case ACPI_METHOD_ARGUMENT_INTEGER:
				{
				DbgPrint("Argument type: INTEGER \n");
				DbgPrint("			Value: %d\n", pArg->Argument);
				break;
				}

			case ACPI_METHOD_ARGUMENT_STRING:
				{
				DbgPrint("Argument type: STRING \n");
				DbgPrint("			Length: %d Value: %s\n", pArg->DataLength, pArg->Data); 
				break;
				}

			case ACPI_METHOD_ARGUMENT_BUFFER:
				{
				DbgPrint("Argument type: BUFFER \n");

				if(strstr(pBuf->MethodName, "_PCT"))
				{
					PPCTData pPctData = (PPCTData)&(pArg->Data[0]);
					DbgPrint("	GeneralRegisterDescriptor 0x%X\n", pPctData->GeneralRegisterDescriptor);
					DbgPrint("	Length 0x%X\n", pPctData->Length);
					switch( pPctData->AddressSpaceKeyword)
					{
					case ADDRESS_SPACE_SYSMEM:
						DbgPrint("	ADDRESS_SPACE_SYSMEM\n");
						break;
					case ADDRESS_SPACE_SYSIO:
						DbgPrint("	ADDRESS_SPACE_SYSIO\n");
						break;
					case ADDRESS_SPACE_PCI:
						DbgPrint("	ADDRESS_SPACE_PCI\n");
						break;
					case ADDRESS_SPACE_EMBED:
						DbgPrint("	ADDRESS_SPACE_EMBED\n");
						break;
					case ADDRESS_SPACE_SMBUS:
						DbgPrint("	ADDRESS_SPACE_SMBUS\n");
						break;
					case ADDRESS_SPACE_FFHW:
						DbgPrint("	ADDRESS_SPACE_FFHW\n");
						break;
					}
					DbgPrint("	RegisterBitWidth 0x%X\n", pPctData->RegisterBitWidth);
					DbgPrint("	RegisterBitOffset 0x%X\n", pPctData->RegisterBitOffset);
					DbgPrint("	AddressSize 0x%X\n", pPctData->AddressSize);
					DbgPrint("	Register addres %I64d\n", pPctData->RegisterAddress);
					DbgPrint("	AccessSize 0x%X\n", pPctData->AccessSize);
					DbgPrint("	DescriptorName 0x%X\n", pPctData->DescriptorName);
				}
				break;
				}

			case ACPI_METHOD_ARGUMENT_PACKAGE:
				{
				ULONG i = 0;

				DbgPrint("Argument type: PACKAGE \n");

//			DbgPrint("Evaldata  sizein %d sizeout %d count %d size ACPI_EVAL_OUTPUT_BUFFER %d  size ACPI_METHOD_ARGUMENT %d  size data %d size PPSSData %d\n", 
//										pACPIEval->Length,
//										pACPIEval->Length,
//										pACPIEval->Count,
//										sizeof(ACPI_EVAL_OUTPUT_BUFFER),
//										sizeof(ACPI_METHOD_ARGUMENT),
//										pArg->DataLength,
//										sizeof(PSSData));


				if(strstr(pBuf->MethodName, "_PSS"))
				{
					PPSSData1 pPSSData;

					pPSSData = (PPSSData1)pArg->Data;

//					DumpFrame(pArg->Data, pArg->DataLength);

					DbgPrint("	Data length %d\n", pPSSData[0].Length);

					DbgPrint("	Frequency: %d\n", pPSSData[0].Data);
					DbgPrint("	Power: %d\n", pPSSData[1].Data);
					DbgPrint("	Latency: %d\n", pPSSData[2].Data);
					DbgPrint("	Bus Master Latency: %d\n", pPSSData[3].Data);
					DbgPrint("	Control: 0x%X\n", pPSSData[4].Data);
					DbgPrint("	Status: 0x%X\n", pPSSData[5].Data);
				}

//				else if(strstr(pBuf->MethodName, "SPSS"))
//				{
//					PPSSData1 pPSSData;
//
//					pPSSData = (PPSSData1)&(pArg->Data[0]);
//
///					DumpFrame(pArg->Data, pArg->DataLength);
//
//					DbgPrint("	Frequency: %d\n", pPSSData[0].Data);
//					DbgPrint("	Power: %d\n", pPSSData[1].Data);
//					DbgPrint("	Latency: %d\n", pPSSData[2].Data);
//					DbgPrint("	Bus Master Latency: %d\n", pPSSData[3].Data);
//					DbgPrint("	Control: 0x%X\n", pPSSData[4].Data);
//					DbgPrint("	Status: 0x%X\n", pPSSData[5].Data);
//				}

				break;
				}
		}

		pArg = ACPI_METHOD_NEXT_ARGUMENT(pArg);
	}
}
#endif
//////////////////////////////////////////////////////////////  END DEBUG STUFF







// 
//   Gets and stores in the device extension the ACPI data for _PCT and _PSS
//
NTSTATUS GetACPIPstateData(PDEVICE_EXTENSION2 pDevExt, PDEVICE_OBJECT pTargetDeviceObject)
{
	PIRP								pIrp;
	PIRP								pIrp2;
	ACPI_ENUM_CHILDREN_INPUT_BUFFER		inBuf;
	ACPI_ENUM_CHILDREN_OUTPUT_BUFFER	outSizeBuf = {0};
	PACPI_ENUM_CHILDREN_OUTPUT_BUFFER	pOutBuf;
	ULONG								bufSize = 0;
	KEVENT								evt;
	IO_STATUS_BLOCK						ioBlk;
	NTSTATUS							stat;
	ULONG								index;
	NTSTATUS							status = STATUS_SUCCESS;

	KeInitializeEvent(&evt, SynchronizationEvent, FALSE);

	inBuf.Signature = ACPI_ENUM_CHILDREN_INPUT_BUFFER_SIGNATURE;
	inBuf.Flags		= ENUM_CHILDREN_MULTILEVEL;
	inBuf.Name[0]	= '\0';
	inBuf.NameLength = 0;

	pIrp = IoBuildDeviceIoControlRequest(IOCTL_ACPI_ENUM_CHILDREN,
										pTargetDeviceObject,
										&inBuf,
										sizeof(ACPI_ENUM_CHILDREN_INPUT_BUFFER),
										&outSizeBuf,
										sizeof(outSizeBuf),
										FALSE,
										&evt,
										&ioBlk);
	if(pIrp)
	{
		stat = IoCallDriver(pTargetDeviceObject, pIrp);
		if(stat == STATUS_PENDING)
		{
			KeWaitForSingleObject(&evt, Executive, KernelMode, FALSE, 0);
			stat = ioBlk.Status;
		}

		if( (stat == STATUS_BUFFER_OVERFLOW) // which we are expecting since we specified a small buffer...
			||
			(stat == STATUS_BUFFER_TOO_SMALL)  )
		{
			if( (outSizeBuf.Signature != ACPI_ENUM_CHILDREN_OUTPUT_BUFFER_SIGNATURE) 
				||
				(outSizeBuf.NumberOfChildren < sizeof(ACPI_ENUM_CHILDREN_INPUT_BUFFER))  )
			{
				status = STATUS_ACPI_INVALID_DATA;
			}
			else
			{
				pOutBuf = ExAllocatePoolWithTag(PagedPool, outSizeBuf.NumberOfChildren, 'ynos');
				if(!pOutBuf)
				{	
					status = STATUS_INSUFFICIENT_RESOURCES;
				}
				else
				{
					RtlZeroMemory(pOutBuf, outSizeBuf.NumberOfChildren);

					KeClearEvent(&evt);

					pIrp2 = IoBuildDeviceIoControlRequest(IOCTL_ACPI_ENUM_CHILDREN,
										pTargetDeviceObject,
										&inBuf,
										sizeof(ACPI_ENUM_CHILDREN_INPUT_BUFFER),
										pOutBuf,
										outSizeBuf.NumberOfChildren,
										FALSE,
										&evt,
										&ioBlk);

					if(pIrp2)
					{
						stat = IoCallDriver(pTargetDeviceObject, pIrp2);
						if(stat == STATUS_PENDING)
						{
							KeWaitForSingleObject(&evt, Executive, KernelMode, FALSE, 0);
							stat = ioBlk.Status;
						}

						if(stat == STATUS_SUCCESS)
						{
							if(pDevExt->pChildEnumData)
							{
								ExFreePool(pDevExt->pChildEnumData);
								pDevExt->pChildEnumData = NULL;							
							}
							pDevExt->pChildEnumData = ExAllocatePoolWithTag(PagedPool, outSizeBuf.NumberOfChildren, 'ynos');

							if(pDevExt->pChildEnumData)
							{
								memcpy(pDevExt->pChildEnumData, pOutBuf, outSizeBuf.NumberOfChildren);
								EvalMethodsEx(pDevExt, pTargetDeviceObject);
							}
						}
						else
						{
							status = stat;
						}
					}
					else
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
					}

					ExFreePool(pOutBuf);
				}
			}
		}
		else
		{
			status = stat;
		}
	}
	else
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
	}

return status;
}



//
//  Our driver does an eval ex, which allows enumeration of child devices, to get its CPU data
//

VOID EvalMethodsEx(IN PDEVICE_EXTENSION2 pDevExt, PDEVICE_OBJECT  pTargetDeviceObject)
{
	PACPI_ENUM_CHILD		pChild;
	ULONG					index;

	pChild = &(  ((PACPI_ENUM_CHILDREN_OUTPUT_BUFFER)(pDevExt->pChildEnumData))->Children[0]);	


	for(index = 0 ; index < ((PACPI_ENUM_CHILDREN_OUTPUT_BUFFER)(pDevExt->pChildEnumData))->NumberOfChildren ; index++)
	{
		KdPrint(("LPCFILTER: Children returned %s\n", pChild->Name));

		if( (strstr( pChild->Name, "_PCT"))
			||
			(strstr( pChild->Name, "_PSS"))
			||
			(strstr( pChild->Name, "SPSS"))
			||
			(strstr( pChild->Name, "XPSS")) )
		{

			KdPrint(("\n	EvaluateMethodsEx:\n"));
			EvaluateMethodsEx(pDevExt, pTargetDeviceObject, pChild);
		}
		else
		{
			KdPrint(("LPCFILTER: Ignoring %s\n", pChild->Name));
		}

		pChild = ACPI_ENUM_CHILD_NEXT(pChild);
	}
}


VOID EvaluateMethodsEx(IN PDEVICE_EXTENSION2 pDevExt, PDEVICE_OBJECT pTargetDeviceObject, IN PACPI_ENUM_CHILD pChild)
{
	ACPI_EVAL_INPUT_BUFFER_EX	inBuf;
	ACPI_EVAL_OUTPUT_BUFFER		outSizeBuf;
	PACPI_EVAL_OUTPUT_BUFFER	pOutBuf = NULL;
	NTSTATUS					status;
	PACPI_METHOD_ARGUMENT		pArg;
	PIRP						pIrp;
	PIRP						pIrp2;
	KEVENT						evt;
	IO_STATUS_BLOCK				ioBlk;
	BOOLEAN						allocd = FALSE;

	if(pChild->NameLength > 256)
	{
		KdPrint(("LPCFILTER: Child name length too long, %d\n", pChild->NameLength));
		return;
	}

	RtlZeroMemory(&inBuf, sizeof(ACPI_EVAL_INPUT_BUFFER_EX)); 

	RtlCopyMemory(inBuf.MethodName, pChild->Name, pChild->NameLength);
	
	inBuf.Signature			=  ACPI_EVAL_INPUT_BUFFER_SIGNATURE_EX; 
	
	RtlZeroMemory(&outSizeBuf, sizeof(ACPI_EVAL_OUTPUT_BUFFER)); // we start of specifying a small buffer.
																// ACPI then retuens with the correct bufer size

	KeInitializeEvent(&evt, SynchronizationEvent, FALSE);

	pIrp = IoBuildDeviceIoControlRequest(IOCTL_ACPI_EVAL_METHOD_EX,   
											pTargetDeviceObject,
											&inBuf,
											sizeof(ACPI_EVAL_INPUT_BUFFER_EX), 
											&outSizeBuf,
											sizeof(ACPI_EVAL_OUTPUT_BUFFER),
											FALSE,
											&evt,
											&ioBlk);

	status = IoCallDriver(pTargetDeviceObject, pIrp);
	if(status ==  STATUS_PENDING)
	{
		KeWaitForSingleObject(&evt, Executive, KernelMode, FALSE, 0);
		status = ioBlk.Status;
	}

	if(status ==  STATUS_BUFFER_OVERFLOW) // which we are expecting sicne we specified a small buffer...
	{
		pOutBuf = ExAllocatePoolWithTag(PagedPool, outSizeBuf.Length, 'ynos');
		if(pOutBuf)
		{
			KeClearEvent(&evt);
			
			RtlZeroMemory(pOutBuf, outSizeBuf.Length);

			pIrp2 = IoBuildDeviceIoControlRequest(IOCTL_ACPI_EVAL_METHOD_EX, 
											pTargetDeviceObject,
											&inBuf,
											sizeof(ACPI_EVAL_INPUT_BUFFER_EX), 
											pOutBuf,
											outSizeBuf.Length,
											FALSE,
											&evt,
											&ioBlk);

			status = IoCallDriver(pTargetDeviceObject, pIrp2);
			if(status ==  STATUS_PENDING)
			{
				KeWaitForSingleObject(&evt, Executive, KernelMode, FALSE, 0);
				status = ioBlk.Status;
			}

			allocd = TRUE;
		}
	}
	else if(status == STATUS_SUCCESS)
	{
		pOutBuf = &outSizeBuf; 
	}

	if(status == STATUS_SUCCESS)
	{
		ULONG						index = 0;

		if(pOutBuf->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE)
		{
			KdPrint(("LPCFILTER: Invalid signature in evaluate method out buffer\n"));
		}
		else
		{
			// we have valid data. If it is _PCT or _PSS data then store it in our device extension
			if(strstr(pChild->Name, "_PCT"))
			{
				if(pDevExt->pPCTData)
				{
					ExFreePool(pDevExt->pPCTData);
					pDevExt->pPCTData = NULL;
				}
			    pDevExt->pPCTData = ExAllocatePoolWithTag(PagedPool, outSizeBuf.Length, 'ynos');
				if(pDevExt->pPCTData)
				{
					memcpy(pDevExt->pPCTData, pOutBuf, outSizeBuf.Length); 
				}
			}
			else if(strstr(pChild->Name, "_PSS"))
			{
				if(pDevExt->pPSSData)
				{
					ExFreePool(pDevExt->pPSSData);
					pDevExt->pPSSData = NULL;
				}
			    pDevExt->pPSSData = ExAllocatePoolWithTag(PagedPool, outSizeBuf.Length, 'ynos');
				if(pDevExt->pPSSData)
				{
					memcpy(pDevExt->pPSSData, pOutBuf, outSizeBuf.Length); 
				}

				// 
				//  Allocate a chunk of memory for our stats data.  Needs to be the stats struct plus one CPUStats struct per CPU
				//  and one CPUFrequencies struct per frequency supported
				//

				pDevExt->StatsDataSize = sizeof(LPCPstateStats) +
										(sizeof(LPCCPUFrequency) * pDevExt->NumProcs * pOutBuf->Count);
				if(pDevExt->pStats)
				{
					ExFreePool(pDevExt->pStats);
					pDevExt->pStats = NULL;
				}
				pDevExt->pStats = ExAllocatePoolWithTag(NonPagedPool,  pDevExt->StatsDataSize, 'ynos');
				
				if(pDevExt->pStats)
				{
					ULONG	x = 0;
					ULONG	y = 0;

					memset(pDevExt->pStats, 0, pDevExt->StatsDataSize);

					pDevExt->pStats->NumCPUs				= pDevExt->NumProcs;
					pDevExt->pStats->StayAliveFailureHits	= 0;
					pDevExt->pStats->TimeStamp				= 0;
					pDevExt->pStats->NumFrequencies			= pOutBuf->Count;

					for(x = 0 ; x < pDevExt->NumProcs ; x++)
					{
						// set up a PCPUFrequencies pointer, first one goes after the SonyFilterStats 
						// the second after SonyFilterStats and the first array of CPUFrequencies
						// etc...
						PLPCCPUFrequency pFreqData = (PLPCCPUFrequency)  (  ((PUCHAR)pDevExt->pStats) + 
															sizeof(LPCPstateStats) +  
															(sizeof(LPCCPUFrequency) * x * pDevExt->pStats->NumFrequencies)  );

						
						pArg = &(pOutBuf->Argument[0]);

						for(y = 0 ; y < pOutBuf->Count ; y++)
						{
							PPSSData pPSSData = (PPSSData)pArg->Data;
							pFreqData[y].Status			= pPSSData[5].Data;				
							pFreqData[y].CPUFrequency	= pPSSData[0].Data;
							pFreqData[y].Time			= 0;

							pArg = ACPI_METHOD_NEXT_ARGUMENT(pArg);
						}
					}
				}
				else
				{
					pDevExt->StatsDataSize = 0;
				}
			}


			pArg = &(pOutBuf->Argument[0]);
			for(index = 0 ; index < pOutBuf->Count ; index++)
			{
				switch(pArg->Type)
				{
					case ACPI_METHOD_ARGUMENT_INTEGER:
						{
	//					DbgPrint("Argument type: INTEGER \n");
	//					DbgPrint("			Value: %d\n", pArg->Argument);
						break;
						}

					case ACPI_METHOD_ARGUMENT_STRING:
						{
		//				DbgPrint("Argument type: STRING \n");
		//				DbgPrint("			Length: %d Value: %s\n", pArg->DataLength, pArg->Data); 
						break;
						}

					case ACPI_METHOD_ARGUMENT_BUFFER:
						{
						KdPrint(("LPCFILTER: Argument type: BUFFER \n"));

						if(strstr(pChild->Name, "_PCT"))
						{
							PPCTData pPctData = (PPCTData)&(pArg->Data[0]);
							KdPrint(("LPCFILTER: 	GeneralRegisterDescriptor 0x%X\n", pPctData->GeneralRegisterDescriptor));
							KdPrint(("LPCFILTER: 	Length 0x%X\n", pPctData->Length));
							switch( pPctData->AddressSpaceKeyword)
							{
							case ADDRESS_SPACE_SYSMEM:
								KdPrint(("LPCFILTER: 	ADDRESS_SPACE_SYSMEM\n"));
								pDevExt->PCTAccessType = ADDRESS_SPACE_SYSMEM;
								break;
							case ADDRESS_SPACE_SYSIO:
								KdPrint(("LPCFILTER: 	ADDRESS_SPACE_SYSIO\n"));
								pDevExt->PCTAccessType = ADDRESS_SPACE_SYSIO;
								break;
							case ADDRESS_SPACE_PCI:
								KdPrint(("LPCFILTER: 	ADDRESS_SPACE_PCI\n"));
								pDevExt->PCTAccessType = ADDRESS_SPACE_PCI;
								break;
							case ADDRESS_SPACE_EMBED:
								KdPrint(("LPCFILTER: 	ADDRESS_SPACE_EMBED\n"));
								pDevExt->PCTAccessType = ADDRESS_SPACE_EMBED;
								break;
							case ADDRESS_SPACE_SMBUS:
								KdPrint(("LPCFILTER: 	ADDRESS_SPACE_SMBUS\n"));
								pDevExt->PCTAccessType = ADDRESS_SPACE_SMBUS;
								break;
							case ADDRESS_SPACE_FFHW:
								KdPrint(("LPCFILTER: 	ADDRESS_SPACE_FFHW\n"));
								pDevExt->PCTAccessType = ADDRESS_SPACE_FFHW;
								break;
							}
							KdPrint(("LPCFILTER: 	RegisterBitWidth 0x%X\n", pPctData->RegisterBitWidth));
							KdPrint(("LPCFILTER: 	RegisterBitOffset 0x%X\n", pPctData->RegisterBitOffset));
							KdPrint(("LPCFILTER: 	AddressSize 0x%X\n", pPctData->AddressSize));
							KdPrint(("LPCFILTER: 	Register addres %I64d\n", pPctData->RegisterAddress));
							KdPrint(("LPCFILTER: 	AccessSize 0x%X\n", pPctData->AccessSize));
							KdPrint(("LPCFILTER: 	DescriptorName 0x%X\n", pPctData->DescriptorName));
						}
						break;
						}

					case ACPI_METHOD_ARGUMENT_PACKAGE:
						{
						ULONG i = 0;

						KdPrint(("LPCFILTER: Argument type: PACKAGE \n"));

//			DbgPrint("Evaldata  sizein %d sizeout %d count %d size ACPI_EVAL_OUTPUT_BUFFER %d  size ACPI_METHOD_ARGUMENT %d  size data %d size PPSSData %d\n", 
//										outSizeBuf.Length,
//										pOutBuf->Length,
//										pOutBuf->Count,
//										sizeof(ACPI_EVAL_OUTPUT_BUFFER),
//										sizeof(ACPI_METHOD_ARGUMENT),
//										pArg->DataLength,
//										sizeof(PSSData));


						if(strstr(pChild->Name, "_PSS"))
						{
							PPSSData pPSSData;

							pPSSData = (PPSSData)pArg->Data;
							
							KdPrint(("LPCFILTER: 	Data length %d\n", pPSSData[0].Length));
							KdPrint(("LPCFILTER: 	Frequency: %d\n", pPSSData[0].Data));
							KdPrint(("LPCFILTER: 	Power: %d\n", pPSSData[1].Data));
							KdPrint(("LPCFILTER: 	Latency: %d\n", pPSSData[2].Data));
							KdPrint(("LPCFILTER: 	Bus Master Latency: %d\n", pPSSData[3].Data));
							KdPrint(("LPCFILTER: 	Control: 0x%X\n", pPSSData[4].Data));
							KdPrint(("LPCFILTER: 	Status: 0x%X\n", pPSSData[5].Data));
						}

	//					else if(strstr(pChild->Name, "SPSS"))
	//					{
	//						PPSSData pPSSData;
//
//							pPSSData = (PPSSData)&(pArg->Data[0]);
//
//							DbgPrint("	Frequency: %d\n", pPSSData[0].Data);
//							DbgPrint("	Power: %d\n", pPSSData[1].Data);
//							DbgPrint("	Latency: %d\n", pPSSData[2].Data);
//							DbgPrint("	Bus Master Latency: %d\n", pPSSData[3].Data);
//							DbgPrint("	Control: 0x%X\n", pPSSData[4].Data);
//							DbgPrint("	Status: 0x%X\n", pPSSData[5].Data);
//						}

						break;
						}
				}

				pArg = ACPI_METHOD_NEXT_ARGUMENT(pArg);
			}
		}
	}
	else
	{
		KdPrint(("LPCFILTER: Failed to evaluate %s  with status 0x%X\n", pChild->Name,   status));
	}

	if(allocd)
		ExFreePool(pOutBuf);
}


/*
VOID EvalMethods(IN PDEVICE_EXTENSION2 pDevExt)
{
	PACPI_ENUM_CHILD		pChild;
	ULONG					index;

	pChild = &(  ((PACPI_ENUM_CHILDREN_OUTPUT_BUFFER)(pDevExt->pChildEnumData))->Children[0]);	

	for(index = 0 ; index < ((PACPI_ENUM_CHILDREN_OUTPUT_BUFFER)(pDevExt->pChildEnumData))->NumberOfChildren ; index++)
	{
//		DbgPrint("Children returned %s\n", pChild->Name);

		if( (strstr( pChild->Name, "_PCT"))
			||
			(strstr( pChild->Name, "_PSS"))
			||
			(strstr( pChild->Name, "SPSS"))
			||
			(strstr( pChild->Name, "XPSS")) )
		{

//			DbgPrint("\n	EvaluateMethods:\n");

			EvaluateMethods(pDevExt, pChild);
		}
		else
		{
	//		DbgPrint("Ignoring %s\n", pChild->Name);
		}

		pChild = ACPI_ENUM_CHILD_NEXT(pChild);
	}
}





//
//  These funcs, for eval methods, arent used by the driver really, they are only here for debug purposes
//

VOID EvaluateMethods(IN PDEVICE_EXTENSION2 pDevExt, IN PACPI_ENUM_CHILD pChild)
{
	ACPI_EVAL_INPUT_BUFFER		inBuf;
	ACPI_EVAL_OUTPUT_BUFFER		outSizeBuf;
	PACPI_EVAL_OUTPUT_BUFFER	pOutBuf = NULL;
	NTSTATUS					status;
	PACPI_METHOD_ARGUMENT		pArg;
	PIRP						pIrp;
	PIRP						pIrp2;
	KEVENT						evt;
	IO_STATUS_BLOCK				ioBlk;
	BOOLEAN						allocd = FALSE;

	if(pChild->NameLength > 256)
	{
		DbgPrint("Child name length too long, %d\n", pChild->NameLength);
		return;
	}

	RtlZeroMemory(&inBuf, sizeof(ACPI_EVAL_INPUT_BUFFER)); 

	//RtlCopyMemory(inBuf.MethodName, pChild->Name, pChild->NameLength);
	RtlCopyMemory(inBuf.MethodName, pChild->Name + (pChild->NameLength - 5), 4);
	
	inBuf.Signature			=  ACPI_EVAL_INPUT_BUFFER_SIGNATURE; 
	
	RtlZeroMemory(&outSizeBuf, sizeof(ACPI_EVAL_OUTPUT_BUFFER));

	KeInitializeEvent(&evt, SynchronizationEvent, FALSE);

	pIrp = IoBuildDeviceIoControlRequest(IOCTL_ACPI_EVAL_METHOD,   
											pDevExt->pOtherExt->TargetDeviceObject,
											&inBuf,
											sizeof(ACPI_EVAL_INPUT_BUFFER), 
											&outSizeBuf,
											sizeof(ACPI_EVAL_OUTPUT_BUFFER),
											FALSE,
											&evt,
											&ioBlk);

	status = IoCallDriver(pDevExt->pOtherExt->TargetDeviceObject, pIrp);
	if(status ==  STATUS_PENDING)
	{
		KeWaitForSingleObject(&evt, Executive, KernelMode, FALSE, 0);
		status = ioBlk.Status;
	}

	if(status ==  STATUS_BUFFER_OVERFLOW) // which we are expecting sicne we specified a small buffer...
	{
		pOutBuf = ExAllocatePoolWithTag(PagedPool, outSizeBuf.Length, 'ynos');
		if(pOutBuf)
		{
			KeClearEvent(&evt);
			
			RtlZeroMemory(pOutBuf, outSizeBuf.Length);

			pIrp2 = IoBuildDeviceIoControlRequest(IOCTL_ACPI_EVAL_METHOD, 
											pDevExt->pOtherExt->TargetDeviceObject,
											&inBuf,
											sizeof(ACPI_EVAL_INPUT_BUFFER), 
											pOutBuf,
											outSizeBuf.Length,
											FALSE,
											&evt,
											&ioBlk);

			status = IoCallDriver(pDevExt->pOtherExt->TargetDeviceObject, pIrp2);
			if(status ==  STATUS_PENDING)
			{
				KeWaitForSingleObject(&evt, Executive, KernelMode, FALSE, 0);
				status = ioBlk.Status;
			}

			allocd = TRUE;
		}
	}
	else if(status == STATUS_SUCCESS)
	{
		pOutBuf = &outSizeBuf; 
	}

	if(status == STATUS_SUCCESS)
	{
		ULONG						index = 0;

		if(pOutBuf->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE)
		{
			DbgPrint("Invalid signature in evaluate method out buffer\n");
		}
		else
		{



			// we have valid data. If it is _PCT or _PSS data then store it in our device extension
			if(strstr(pChild->Name, "_PCT"))
			{
				if(pDevExt->pPCTData)
				{
					ExFreePool(pDevExt->pPCTData);
					pDevExt->pPCTData = NULL;
				}
			    pDevExt->pPCTData = ExAllocatePoolWithTag(PagedPool, outSizeBuf.Length, 'ynos');
				if(pDevExt->pPCTData)
				{
					memcpy(pDevExt->pPCTData, pOutBuf, outSizeBuf.Length); 
				}
			}
			else if(strstr(pChild->Name, "_PSS"))
			{
				if(pDevExt->pPSSData)
				{
					ExFreePool(pDevExt->pPSSData);
					pDevExt->pPSSData = NULL;
				}
			    pDevExt->pPSSData = ExAllocatePoolWithTag(PagedPool, outSizeBuf.Length, 'ynos');
				if(pDevExt->pPSSData)
				{
					memcpy(pDevExt->pPSSData, pOutBuf, outSizeBuf.Length); 
				}
			}



			pArg = &(pOutBuf->Argument[0]);
			for(index = 0 ; index < pOutBuf->Count ; index++)
			{

				switch(pArg->Type)
				{
					case ACPI_METHOD_ARGUMENT_INTEGER:
						{
						DbgPrint("Argument type: INTEGER \n");
						DbgPrint("			Value: %d\n", pArg->Argument);
						break;
						}

					case ACPI_METHOD_ARGUMENT_STRING:
						{
						DbgPrint("Argument type: STRING \n");
						DbgPrint("			Length: %d Value: %s\n", pArg->DataLength, pArg->Data); 
						break;
						}

					case ACPI_METHOD_ARGUMENT_BUFFER:
						{
						DbgPrint("Argument type: BUFFER \n");

						if(strstr(pChild->Name, "_PCT"))
						{
							PPCTData pPctData = (PPCTData)&(pArg->Data[0]);
							DbgPrint("	GeneralRegisterDescriptor 0x%X\n", pPctData->GeneralRegisterDescriptor);
							DbgPrint("	Length 0x%X\n", pPctData->Length);
							switch( pPctData->AddressSpaceKeyword)
							{
							case ADDRESS_SPACE_SYSMEM:
								DbgPrint("	ADDRESS_SPACE_SYSMEM\n");
								break;
							case ADDRESS_SPACE_SYSIO:
								DbgPrint("	ADDRESS_SPACE_SYSIO\n");
								break;
							case ADDRESS_SPACE_PCI:
								DbgPrint("	ADDRESS_SPACE_PCI\n");
								break;
							case ADDRESS_SPACE_EMBED:
								DbgPrint("	ADDRESS_SPACE_EMBED\n");
								break;
							case ADDRESS_SPACE_SMBUS:
								DbgPrint("	ADDRESS_SPACE_SMBUS\n");
								break;
							case ADDRESS_SPACE_FFHW:
								DbgPrint("	ADDRESS_SPACE_FFHW\n");
								break;
							}
							DbgPrint("	RegisterBitWidth 0x%X\n", pPctData->RegisterBitWidth);
							DbgPrint("	RegisterBitOffset 0x%X\n", pPctData->RegisterBitOffset);
							DbgPrint("	AddressSize 0x%X\n", pPctData->AddressSize);
							DbgPrint("	Register addres %I64d\n", pPctData->RegisterAddress);
							DbgPrint("	AccessSize 0x%X\n", pPctData->AccessSize);
							DbgPrint("	DescriptorName 0x%X\n", pPctData->DescriptorName);
						}
						break;
						}

					case ACPI_METHOD_ARGUMENT_PACKAGE:
						{
						ULONG i = 0;

						DbgPrint("Argument type: PACKAGE \n");

//			DbgPrint("Evaldata  sizein %d sizeout %d count %d size ACPI_EVAL_OUTPUT_BUFFER %d  size ACPI_METHOD_ARGUMENT %d  size data %d size PPSSData %d\n", 
//										outSizeBuf.Length,
//										pOutBuf->Length,
//										pOutBuf->Count,
//										sizeof(ACPI_EVAL_OUTPUT_BUFFER),
//										sizeof(ACPI_METHOD_ARGUMENT),
//										pArg->DataLength,
//										sizeof(PSSData));


						if(strstr(pChild->Name, "_PSS"))
						{
							PPSSData1 pPSSData;

							pPSSData = (PPSSData1)&(pArg->Data[0]);

							
//							DumpFrame(pArg->Data, pArg->DataLength);
							DbgPrint("	Data length %d\n", pPSSData[0].Length);

							DbgPrint("	Frequency: %d\n", pPSSData[0].Data);
							DbgPrint("	Power: %d\n", pPSSData[1].Data);
							DbgPrint("	Latency: %d\n", pPSSData[2].Data);
							DbgPrint("	Bus Master Latency: %d\n", pPSSData[3].Data);
							DbgPrint("	Control: 0x%X\n", pPSSData[4].Data);
							DbgPrint("	Status: 0x%X\n", pPSSData[5].Data);
						}

//						else if(strstr(pChild->Name, "SPSS"))
//						{
///							PPSSData1 pPSSData;
//
//							pPSSData = (PPSSData1)&(pArg->Data[0]);
//
//
//							DumpFrame(pArg->Data, pArg->DataLength);
//
//							DbgPrint("	Frequency: %d\n", pPSSData[0].Data);
//							DbgPrint("	Power: %d\n", pPSSData[1].Data);
//							DbgPrint("	Latency: %d\n", pPSSData[2].Data);
//							DbgPrint("	Bus Master Latency: %d\n", pPSSData[3].Data);
//							DbgPrint("	Control: 0x%X\n", pPSSData[4].Data);
//							DbgPrint("	Status: 0x%X\n", pPSSData[5].Data);
//						}

						break;
						}
				}

				pArg = ACPI_METHOD_NEXT_ARGUMENT(pArg);
			}
		}
	}
	else
	{
		DbgPrint("Failed to evaluate %s  with status 0x%X\n", pChild->Name,   status);
	}

	if(allocd)
		ExFreePool(pOutBuf);
}
*/
