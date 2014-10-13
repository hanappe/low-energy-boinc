
#include "SonyFilter.h"


//
// Bit Flag Macros
//

#define SET_FLAG(Flags, Bit)    ((Flags) |= (Bit))
#define CLEAR_FLAG(Flags, Bit)  ((Flags) &= ~(Bit))
#define TEST_FLAG(Flags, Bit)   (((Flags) & (Bit)) != 0)

ULONG		gCtlDevNum = 0;
NTSTATUS	gStatus = STATUS_SUCCESS;	// status for the CONTROL device create
NTSTATUS	gOpenStatus = STATUS_SUCCESS; // status for the CONTROL device open

//
// Define the sections that allow for discarding (i.e. paging) some of
// the code.
//
/*
#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, FPFilterAddDevice)
#pragma alloc_text (PAGE, FPFilterDispatchPnp)
#pragma alloc_text (PAGE, FPFilterStartDevice)
#pragma alloc_text (PAGE, FPFilterRemoveDevice)
#pragma alloc_text (PAGE, FPFilterUnload)
#endif
*/

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{

    ULONG						ulIndex;
    PDRIVER_DISPATCH			*dispatch;
	ULONG						zero = 0;

	//
    // The default IRP handler is FPFilterGenericIrp (All FILTER DO Irps are passed on, CONTROL DO IRPs are handled and returned)
    //
    for (ulIndex = 0, dispatch = DriverObject->MajorFunction;
         ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
         ulIndex++, dispatch++)
	{
        *dispatch = FPFilterGenericIrp;
    }

    //
    // These IRPs have their own handlers 
	//
    DriverObject->MajorFunction[IRP_MJ_PNP]             = FPFilterDispatchPnp;
    
	DriverObject->MajorFunction[IRP_MJ_POWER]           = FPFilterDispatchPower;

    DriverObject->DriverExtension->AddDevice            = FPFilterAddDevice;
    DriverObject->DriverUnload                          = FPFilterUnload;

    return(STATUS_SUCCESS);

} 

//
//  PnP Add device handler.  This gets notified for each device on the system we 
//  manage, or filter in this case, thus this gets called per CPU
//

NTSTATUS
FPFilterAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )
{
    NTSTATUS                status;
    PDEVICE_OBJECT          filterDeviceObject;
    ULONG                   registrationFlag = 0;
    PDEVICE_EXTENSION       pDevExtFilter;
    PDEVICE_OBJECT          controlDeviceObject;
    PDEVICE_EXTENSION2      pDevExtControl;
	WCHAR					deviceNamestr[32];
	WCHAR					linkNamestr[32];
	UNICODE_STRING			deviceName;
	UNICODE_STRING			linkName;
	ULONG					n;
    PDEVICE_OBJECT          TempDeviceObject;
	ULONG					DeviceType = FILE_DEVICE_UNKNOWN;


    KdPrint(("LPCFILTER: FPFilterAddDevice: Driver %p Device %p\n", DriverObject, PhysicalDeviceObject));


	//
	//  Create a single control device object. This can be opened by user mode apps to do usefull things with
	//

	if(gCtlDevNum == 0)
	{

		RtlStringCbPrintfW(deviceNamestr,   64, L"\\Device\\SFltrl%d", gCtlDevNum);  // the symbollic link that can be opened by CreateFile() in user mode.
		RtlStringCbPrintfW(linkNamestr,   64, L"\\DosDevices\\SFltrl%d", gCtlDevNum);

		RtlInitUnicodeString(&deviceName,	deviceNamestr);

		RtlInitUnicodeString(&linkName,	linkNamestr);

		gStatus = IoCreateDevice(DriverObject,
								sizeof(DEVICE_EXTENSION2), // this allocates a chunk of memory in the device obhect of this size
								&deviceName,
								FILE_DEVICE_UNKNOWN,
								0,
								FALSE,
								&controlDeviceObject);

		if (!NT_SUCCESS(gStatus))
		{
			KdPrint(("LPCFILTER: FPFilterAddDevice: Cannot create control DeviceObject\n"));
			goto EndCONTROLdeviceCreate;
		}


		// point our device extension to the chunk of memory allocated for us
		pDevExtControl					= (PDEVICE_EXTENSION2) controlDeviceObject->DeviceExtension;

		// and set up some usefull values in it...
		RtlZeroMemory(pDevExtControl, sizeof(PDEVICE_EXTENSION2));

		pDevExtControl->DeviceObject	= controlDeviceObject;

		pDevExtControl->Type			= CONTROL;

		pDevExtControl->DeviceObject->Flags |= DO_DIRECT_IO;

		pDevExtControl->DeviceObject->Flags  &= ~DO_POWER_PAGABLE;



		status = IoCreateSymbolicLink(&linkName, &deviceName);

		if (!NT_SUCCESS(status)) 
		{
			KdPrint(("LPCFILTER: FPFilterAddDevice: Cannot create symbolic link\n"));
			IoDeleteDevice(pDevExtControl->DeviceObject);
			goto EndCONTROLdeviceCreate;
		}


		RtlStringCbCopyW(pDevExtControl->linkNamestr, 64, linkNamestr);
		
		RtlInitUnicodeString(&pDevExtControl->linkName, pDevExtControl->linkNamestr);

		KeInitializeSpinLock(&pDevExtControl->AppLock);


		//
		// set some default values
		//

		pDevExtControl->Config.StayAliveFailureLimit	= 50;

		pDevExtControl->Config.ThreadDelay				= 20;

		pDevExtControl->Config.StatsSampleRate			= 1;  // basic sampling rate

		// KeQueryTimeIncrement returns the number of 100 nano seconds intervals per tick,
		// Convert it to milliseconds (normally about 15 ms per tick, but this can be adjusted)
		pDevExtControl->MillisPerTick =  KeQueryTimeIncrement() / 10000;
		

		KdPrint(("LPCFILTER: FPFilterAddDevice: control Device Object: 0x%X \n		Created device %S \n", controlDeviceObject, deviceNamestr));

		// Clear the DO_DEVICE_INITIALIZING flag, if you dont, you cant open the device from user mode.
		// Yep, how many times have I forgotten to do that.... :)
		CLEAR_FLAG(controlDeviceObject->Flags, DO_DEVICE_INITIALIZING);
	}
EndCONTROLdeviceCreate:

	


    //
    // Create a filter device object for this device.
	// This is the device that actually sits in the CPU stack.
	// Because declared this device as a lower filter to the normal CPU driver, 
	// ie, we are above acpi and below intelppmm when we cal IoAttachDeviceToDeviceStack() 
	// we get attached to acpi and get a pointer to its device object for the CPU (actually called a Physical Device Object) 
    //

	// get a temp ref to the lower device so we can copy the device type
	TempDeviceObject = IoGetAttachedDeviceReference(PhysicalDeviceObject);
    DeviceType = TempDeviceObject->DeviceType;
    ObDereferenceObject(TempDeviceObject);

	KdPrint(("LPCFILTER: FPFilterAddDevice: device type is %d\n", DeviceType));

    status = IoCreateDevice(DriverObject,
                            sizeof(DEVICE_EXTENSION),
                            NULL,
                            DeviceType,
                            0,
                            FALSE,
                            &filterDeviceObject);

    if (!NT_SUCCESS(status)) {
        KdPrint(("LPCFILTER: FPFilterAddDevice: Cannot create filterDeviceObject\n"));
		IoDeleteSymbolicLink(&linkName);
		IoDeleteDevice(controlDeviceObject);
        return status;
    }

    pDevExtFilter = (PDEVICE_EXTENSION) filterDeviceObject->DeviceExtension;

    RtlZeroMemory(pDevExtFilter, sizeof(DEVICE_EXTENSION));



    //
    // Attach the device object to the highest device object in the chain and
    // return the previously highest device object, which is passed to
    // IoCallDriver when we pass IRPs down the device stack
    //

	// we use TargetDeviceObject to send IOCTLs to  to get information on ACPI objects
    pDevExtFilter->TargetDeviceObject = IoAttachDeviceToDeviceStack(filterDeviceObject, PhysicalDeviceObject);

    if (pDevExtFilter->TargetDeviceObject == NULL) 
	{
 		IoDeleteSymbolicLink(&linkName);
		IoDeleteDevice(controlDeviceObject);
        IoDeleteDevice(filterDeviceObject);
        KdPrint(("LPCFILTER: FPFilterAddDevice: Unable to attach %X to target %X\n", filterDeviceObject, PhysicalDeviceObject));
        return STATUS_NO_SUCH_DEVICE;
    }

    // Save the filter device object in the device extension
    pDevExtFilter->DeviceObject = filterDeviceObject;

	pDevExtFilter->Type			= FILTER;

	// copy the flags and other stuff off the lower device 
	pDevExtFilter->DeviceObject->Flags |= 
					pDevExtFilter->TargetDeviceObject->Flags &
					(DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE );

	pDevExtFilter->DeviceObject->Characteristics	= pDevExtFilter->TargetDeviceObject->Characteristics;
	pDevExtFilter->DeviceObject->DeviceType			= pDevExtFilter->TargetDeviceObject->DeviceType;

	KdPrint(("LPCFILTER: FPFilterAddDevice: Lowerdo type is %d\n", pDevExtFilter->DeviceObject->DeviceType));
    KdPrint(("LPCFILTER: FPFilterAddDevice: Lowerdo flags are: 0x%X\n",  pDevExtFilter->DeviceObject->Flags));

    // if it is the first FILTER device we created and we sucessfully created a CONTROL device...
	if((gCtlDevNum == 0) && (NT_SUCCESS(gStatus)))
	{
		// reference each other device extensions
		// we do this so when the CONTROL device object gets caled frodm user mode we can interact with the acpi PhsicalDevice Object
		pDevExtControl->pOtherExt = pDevExtFilter;
		pDevExtFilter->pOtherExt = pDevExtControl;
	}
		
    //
    // Initialize the remove lock
    //
    IoInitializeRemoveLock(&pDevExtFilter->RemoveLock,
                           0,
                           0,
                           0);

    // Clear the DO_DEVICE_INITIALIZING flag
    CLEAR_FLAG(filterDeviceObject->Flags, DO_DEVICE_INITIALIZING);

	gCtlDevNum = 1; // we created our control device object, so even though AddDevice 
	// gets called again on a multi processor system, we set this so we dont create any more

    return STATUS_SUCCESS;
} 


//
// General PnP handler
// Some interesting traffic passes through this on starup.
// Notable IRP_MN_QUERY_INTERFACE which is where the upper CPU driver, the FDO (Functional Device Object) 
// gets adresses of ACPI functions with which it can register callbacks. 
//

NTSTATUS
FPFilterDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            status;
    PDEVICE_EXTENSION   pDevExt = DeviceObject->DeviceExtension;
    BOOLEAN				lockHeld;

    KdPrint(("LPCFILTER: FPFilterDispatchPnp: Device %X Irp %X\n", DeviceObject, Irp));

    //
    // Acquire the remove lock so that device will not be removed while
    // processing this irp.
    //
    status = IoAcquireRemoveLock(&pDevExt->RemoveLock, Irp);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("LPCFILTER: FPFilterDispatchPnp: Remove lock failed PNP Irp type [%#02x]\n", irpSp->MinorFunction));
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    lockHeld		= TRUE;


    switch(irpSp->MinorFunction) 
	{
		case IRP_MN_QUERY_INTERFACE:
		{
            KdPrint(("LPCFILTER: FPFilterDispatchPnp: Schedule completion for QUERY_INTERFACE\n"));

			status = FPFilterForwardIrpSynchronous(DeviceObject, Irp);
			status = Irp->IoStatus.Status;
			IoCompleteRequest(Irp,IO_NO_INCREMENT);
			break;
		}
		case IRP_MN_QUERY_CAPABILITIES:
		{
            KdPrint(("LPCFILTER: FPFilterDispatchPnp: Sending on QUERY_CAPABILITIES\n"));
			status = FPFilterSendToNextDriver(DeviceObject, Irp);

			break;
		}

        case IRP_MN_START_DEVICE:
        {
			KdPrint(("LPCFILTER: FPFilterDispatchPnp: Sending on START_DEVICE\n"));
            status = FPFilterSendToNextDriver(DeviceObject, Irp);
	
            break;
        }

		case IRP_MN_STOP_DEVICE:
        {
			KdPrint(("LPCFILTER: FPFilterDispatchPnp: Sending on  STOP_DEVICE\n"));
            status = FPFilterSendToNextDriver(DeviceObject, Irp);

			// if this filter device is the one with an associated control device then stop the thread and tidy up
			if(pDevExt->pOtherExt != NULL)
			{
				pDevExt->pOtherExt->ThreadGo = FALSE;

				// wait for the thread to end
				if(pDevExt->pOtherExt->DispatcherThreadObject)
				{
					KeWaitForSingleObject(pDevExt->pOtherExt->DispatcherThreadObject, Executive, KernelMode, FALSE, NULL);
					ObDereferenceObject(pDevExt->pOtherExt->DispatcherThreadObject);
					pDevExt->pOtherExt->DispatcherThreadObject = NULL;
				}

				if(pDevExt->pOtherExt->pStats)
				{
					ExFreePool(pDevExt->pOtherExt->pStats);
					pDevExt->pOtherExt->pStats = NULL;
					pDevExt->pOtherExt->StatsDataSize = 0;
				}

				if(pDevExt->pOtherExt->pPCTData)
				{
					ExFreePool(pDevExt->pOtherExt->pPCTData);
					pDevExt->pOtherExt->pPCTData = NULL;
				}
	
				if(pDevExt->pOtherExt->pPSSData)
				{
					ExFreePool(pDevExt->pOtherExt->pPSSData);
					pDevExt->pOtherExt->pPSSData = NULL;
				}

				if(pDevExt->pOtherExt->pChildEnumData)
				{
					ExFreePool(pDevExt->pOtherExt->pChildEnumData);
					pDevExt->pOtherExt->pChildEnumData = NULL;
				}
				
				if(pDevExt->pOtherExt->pCPUDB)
				{
					ExFreePool(pDevExt->pOtherExt->pCPUDB);
					pDevExt->pOtherExt->pCPUDB = NULL;
				}

				pDevExt->pOtherExt->IsDataObtained = FALSE;
			}
            break;
        }

	    case IRP_MN_REMOVE_DEVICE:
        {
            //
            // Delete our device object(s) 
            //
            KdPrint(( "FPFilterDispatchPnp: Sending on  REMOVE_DEVICE\n"));
            status = FPFilterSendToNextDriver(DeviceObject, Irp);

			IoReleaseRemoveLockAndWait(&pDevExt->RemoveLock, Irp);

			IoDetachDevice(pDevExt->TargetDeviceObject);

			// if this filter device is the one with an associated control device then tidy up
			// stop the thread and delete that too device too
			if(pDevExt->pOtherExt != NULL)
			{
				pDevExt->pOtherExt->ThreadGo = FALSE;

				// wait for the thread to end
				if(pDevExt->pOtherExt->DispatcherThreadObject)
				{
					KeWaitForSingleObject(pDevExt->pOtherExt->DispatcherThreadObject, Executive, KernelMode, FALSE, NULL);
					ObDereferenceObject(pDevExt->pOtherExt->DispatcherThreadObject);
					pDevExt->pOtherExt->DispatcherThreadObject = NULL;
				}

				if(pDevExt->pOtherExt->pStats)
				{
					ExFreePool(pDevExt->pOtherExt->pStats);
					pDevExt->pOtherExt->pStats = NULL;
					pDevExt->pOtherExt->StatsDataSize = 0;
				}

				if(pDevExt->pOtherExt->pPCTData)
				{
					ExFreePool(pDevExt->pOtherExt->pPCTData);
					pDevExt->pOtherExt->pPCTData = NULL;
				}
	
				if(pDevExt->pOtherExt->pPSSData)
				{
					ExFreePool(pDevExt->pOtherExt->pPSSData);
					pDevExt->pOtherExt->pPSSData = NULL;
				}

				if(pDevExt->pOtherExt->pChildEnumData)
				{
					ExFreePool(pDevExt->pOtherExt->pChildEnumData);
					pDevExt->pOtherExt->pChildEnumData = NULL;
				}
				
				if(pDevExt->pOtherExt->pCPUDB)
				{
					ExFreePool(pDevExt->pOtherExt->pCPUDB);
					pDevExt->pOtherExt->pCPUDB = NULL;
				}

				IoDeleteSymbolicLink(&pDevExt->pOtherExt->linkName);

				IoDeleteDevice(pDevExt->pOtherExt->DeviceObject);

				gCtlDevNum = 0;
			}
		
			IoDeleteDevice(pDevExt->DeviceObject);

			lockHeld = FALSE;

            break;
        }
        
        case IRP_MN_DEVICE_USAGE_NOTIFICATION:
        {
            KdPrint(("LPCFILTER: FPFilterDispatchPnp: Sending on DEVICE_USAGE_NOTIFICATION\n"));
            status = FPFilterSendToNextDriver(DeviceObject, Irp);
            break; 
        }
        default:
        {
            KdPrint(("LPCFILTER: FPFilterDispatchPnp: Forwarding irp\n"));
	        status = FPFilterSendToNextDriver(DeviceObject, Irp);
        }
    }

    if (lockHeld)
    {
        IoReleaseRemoveLock(&pDevExt->RemoveLock, Irp);
    }

    return status;
} 

NTSTATUS
FPFilterDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION deviceExtension;

    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);

    deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    return PoCallDriver(deviceExtension->TargetDeviceObject, Irp);
} 


//
// Basic Irp forwarding func, isnt interested in what comes back.
//

NTSTATUS
FPFilterSendToNextDriver(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   deviceExtension;

    IoSkipCurrentIrpStackLocation(Irp);
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);
} 







//
// This Irp Forwarding func is interested im what comes back, so it is symchronous and has a cmpletion routine (above)
//
NTSTATUS
FPFilterForwardIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   deviceExtension;
    KEVENT event;
    NTSTATUS status;

    KeInitializeEvent(&event, NotificationEvent, FALSE);
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    // copy the irpstack for the next device
    IoCopyCurrentIrpStackLocationToNext(Irp);

    // set a completion routine
    IoSetCompletionRoutine(Irp, FPFilterIrpCompletion,  &event, TRUE, TRUE, TRUE);

	// FPFilterIrpCOmpletion returns MORE_PROCESSING_REQUIRED
	// so the code that calls FPFilterForwardIrpSynchronous needs to call IoCompleteRequest()

    // call the next lower device
    status = IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

    // wait for the actual completion
    if (status == STATUS_PENDING) 
	{
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = Irp->IoStatus.Status;
    }

    return status;
} 

NTSTATUS
FPFilterIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PKEVENT Event = (PKEVENT) Context;

	if (Irp->PendingReturned)
		IoMarkIrpPending(Irp);


    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);

    return(STATUS_MORE_PROCESSING_REQUIRED);
}  







//
//  Generic Irp handler
//	Irps for all device objects created by this driver are handled here, driver entry can only set one Irp handler.
//
//

NTSTATUS
FPFilterGenericIrp(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
   )
{
    PDEVICE_EXTENSION	pDevExtension		= DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION	irpStack			= IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS			status				= STATUS_SUCCESS;


	
	if(pDevExtension->Type == FILTER)
	{
		//
		//  So this is the FILTER device object, ie the one between the CPU driver and ACPI.sys
		//

/*
	    KdPrint(("LPCFILTER: FPFilterGenericIrp: Filter DeviceObject %X Irp %X\n",  DeviceObject, Irp));


		switch(irpStack->MajorFunction)
		{	
		case IRP_MJ_DEVICE_CONTROL:
			{
				switch(irpStack->Parameters.DeviceIoControl.IoControlCode)
				{
				PACPI_ENUM_CHILDREN_OUTPUT_BUFFER	pACPIChild;
				PACPI_EVAL_OUTPUT_BUFFER			pACPIEval;

				//
				//  Thes are handled only so the driver can trace out the ACPI queries running
				//  between the CPU driver and ACPI.sys
				//

				case IOCTL_ACPI_ENUM_CHILDREN:
					{
						ULONG	inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
						ULONG	outlen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

						KdPrint(("LPCFILTER: 		IOCTL_ACPI_ENUM_CHILDREN Filter DeviceObject %X Irp %X\n",  DeviceObject, Irp));

						status = FPFilterForwardIrpSynchronous(DeviceObject, Irp);

						KdPrint(("LPCFILTER: 			%d bytes of data in %d bytes of data out %d bytes of data returned status 0x%X\n\n", 
											inlen,
											outlen,
											Irp->IoStatus.Information,
											status)); 	



						if((status == STATUS_SUCCESS) && (Irp->IoStatus.Information > sizeof(ACPI_ENUM_CHILDREN_INPUT_BUFFER)))
						{
							pACPIChild = (PACPI_ENUM_CHILDREN_OUTPUT_BUFFER) Irp->AssociatedIrp.SystemBuffer;

							if(pACPIChild->Signature != ACPI_ENUM_CHILDREN_OUTPUT_BUFFER_SIGNATURE) 
							{
								DbgPrint("			Invalid signature in evaluate method out buffer\n");
							}
							else
							{
 //                               DumpKids(pACPIChild);
							}
						}
						status = Irp->IoStatus.Status;
						IoCompleteRequest(Irp,IO_NO_INCREMENT);
						return status;
					}
	
				case IOCTL_ACPI_EVAL_METHOD:
					{
						ULONG					inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
						ULONG					outlen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
						PACPI_EVAL_INPUT_BUFFER	pBuf;
							

						if(inlen)
						{
							pBuf = ExAllocatePoolWithTag(PagedPool, inlen+10, 'ynos');
							if(pBuf)
							{
								memset(pBuf, 0, inlen+10);
								memcpy(pBuf, (PUCHAR)Irp->AssociatedIrp.SystemBuffer, inlen);
							}
						}
						else
						{
							pBuf = ExAllocatePoolWithTag(PagedPool, 10, 'ynos');
							if(pBuf)
							{
								memset(pBuf, 0, 10);
							}
						}

						KdPrint(("LPCFILTER: 		IOCTL_ACPI_EVAL_METHOD Filter DeviceObject %X Irp %X\n",  DeviceObject, Irp));


						status = FPFilterForwardIrpSynchronous(DeviceObject, Irp);
						
						KdPrint(("LPCFILTER: 			%c%c%c%c     %d bytes of data in %d bytes of data out %d bytes of data returned status 0x%X\n\n", 
													pBuf->MethodName[0],
													pBuf->MethodName[1],
													pBuf->MethodName[2],
													pBuf->MethodName[3],
													inlen,
													outlen,
													Irp->IoStatus.Information,
													status)); 	

						if((status == STATUS_SUCCESS) && (Irp->IoStatus.Information > sizeof(ACPI_EVAL_OUTPUT_BUFFER)))
						{
							pACPIEval = (PACPI_EVAL_OUTPUT_BUFFER) Irp->AssociatedIrp.SystemBuffer;

							if(pACPIEval->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE)
							{
								DbgPrint("			Invalid signature in evaluate method out buffer\n");
							}
							else
							{
//								DumpEval(pACPIEval, pBuf);
							}
						}
						ExFreePool(pBuf);
						status = Irp->IoStatus.Status;
						IoCompleteRequest(Irp,IO_NO_INCREMENT);
						return status;
					}
					
					
				case IOCTL_ACPI_EVAL_METHOD_EX:
					{
						ULONG					inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
						ULONG					outlen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
						PACPI_EVAL_INPUT_BUFFER	pBuf;

						if(inlen)
						{
							pBuf = ExAllocatePoolWithTag(PagedPool, inlen+10, 'ynos');
							if(pBuf)
							{
								memset(pBuf, 0, inlen+10);
								memcpy(pBuf, (PUCHAR)Irp->AssociatedIrp.SystemBuffer, inlen);
							}
						}
						else
						{
							pBuf = ExAllocatePoolWithTag(PagedPool, 10, 'ynos');
							if(pBuf)
							{
								memset(pBuf, 0, 10);
							}
						}


						KdPrint(("LPCFILTER: 		IOCTL_ACPI_EVAL_METHOD_EX Filter DeviceObject %X Irp %X\n",  DeviceObject, Irp));


						status = FPFilterForwardIrpSynchronous(DeviceObject, Irp);
						
						KdPrint(("LPCFILTER: 			%c%c%c%c     %d bytes of data in %d bytes of data out  status 0x%X\n\n", 
													pBuf->MethodName[0],
													pBuf->MethodName[1],
													pBuf->MethodName[2],
													pBuf->MethodName[3],
													inlen,
													outlen,
													status)); 	


						if((status == STATUS_SUCCESS) && (Irp->IoStatus.Information > sizeof(ACPI_EVAL_OUTPUT_BUFFER)))
						{
							pACPIEval = (PACPI_EVAL_OUTPUT_BUFFER) Irp->AssociatedIrp.SystemBuffer;
//							DumpEval(pACPIEval, pBuf);
						}
						ExFreePool(pBuf);
						status = Irp->IoStatus.Status;
						IoCompleteRequest(Irp,IO_NO_INCREMENT);
						return status;
					}
				}
			}
		}
*/
	    // FILTER DO Irps are just passed down, we dont care what they are 
		return FPFilterSendToNextDriver(DeviceObject, Irp);
	}
	else
	{
		//
		//  This is the CONTROl device object, ie the one opened by the application
		//

		PDEVICE_EXTENSION2 pDevExt = DeviceObject->DeviceExtension;

		KdPrint(("LPCFILTER: FPFilterGenericIrp: Control DeviceObject %X Irp %X\n",  DeviceObject, Irp));

	
		switch(irpStack->MajorFunction)
		{
			case IRP_MJ_CREATE:
			{
				KIRQL	irql;

				if(!NT_SUCCESS(gOpenStatus))
				{
					status = gOpenStatus;
					KdPrint(("LPCFILTER: Previous open failed so failing this one\n"));
					break;
				}

				KeAcquireSpinLock(&pDevExt->AppLock, &irql);

				if(pDevExt->IsDataObtained == FALSE)
				{
					KAFFINITY	ActiveAffinity;
					ULONG		x = 0;

					pDevExt->IsDataObtained = TRUE;

					KeReleaseSpinLock(&pDevExt->AppLock, irql);

					// get the number of CPUs
					pDevExt->NumProcs = KeQueryActiveProcessorCount(&ActiveAffinity);

					// allocate and initialise our CPU state array (CPUDB)
					pDevExt->pCPUDB = ExAllocatePoolWithTag(NonPagedPool, pDevExt->NumProcs * sizeof(CPUDataBase), 'ynos');
					if(!pDevExt->pCPUDB)
					{
						status = gOpenStatus = STATUS_INSUFFICIENT_RESOURCES;
						KdPrint(("LPCFILTER: Allocating CPUDB failed\n"));
						break;
					}

					for(x = 0 ; x < pDevExt->NumProcs ; x++)
					{
						pDevExt->pCPUDB[x].PState			= 0xFFFFFFFF;
						pDevExt->pCPUDB[x].StayAliveCount	= 0;
					}

					// get the ACPI data for CPU Pstate
					gOpenStatus = GetACPIPstateData(pDevExt, pDevExt->pOtherExt->TargetDeviceObject);  
					if(!NT_SUCCESS(gOpenStatus))
					{
						KdPrint(("LPCFILTER: GetACPIPstateData failed\n"));
						status = gOpenStatus;
						break; 	// we dont free allocated data here, it is done at IRP_MJ_STOP_DEVICE
					}
						

					// Check whether the CPU supports MSRs
					if(pDevExt->PCTAccessType == ADDRESS_SPACE_FFHW)
					{
						pDevExt->CpuManuf = GetProcessorManufAndMSRSupport();
						if(pDevExt->CpuManuf == UNSUPPORTED)
						{
							// the data allocated in GetACPIPstateData will be freed by subsequent calls to GetACPIPstateData
							// and finally freed at Stop Device
							KdPrint(("LPCFILTER: GetProcessorManufAndMSRSupport failed\n"));
							status = gOpenStatus = STATUS_NOT_IMPLEMENTED;
							break; // we dont free allocated data here, it is done at IRP_MJ_STOP_DEVICE
						}
					}

					// if we ever see a CPU support ADDRESS_SPACE_SYSIO space we need to add some code here....
					// But it is likely we wont since the will be old CPUs ot used and too slow to run windows 7+


					
				}
				else // IsDataObtained is TRUE so we can allow the open
				{
					KeReleaseSpinLock(&pDevExt->AppLock, irql);
					status = STATUS_SUCCESS;
					KdPrint(("LPCFILTER: Previous open succeeded so succeeding this request\n"));
					break;
				}





				// If the thread was not already started...
				// We could not call this code at IRP_MJ_START_DEVICE since the lower PDO was not ready
				// and returned different ACPI child data (IO ADDRESS SPACE instead of FFHW)
				// However we do handle thread and data clean up at IRP_MJ_REMOVE_DEVICE
				if(pDevExt->DispatcherThreadObject == NULL)
				{
					pDevExt->ThreadGo = TRUE;

					gOpenStatus = PsCreateSystemThread(&pDevExt->DispatcherThreadHandle, 
											THREAD_ALL_ACCESS, 
											NULL, 
											NULL, 
											NULL,
											(PKSTART_ROUTINE) PerfCtlThread, 
											pDevExt);

					if (!NT_SUCCESS(gOpenStatus)) //  if we cant start the thread fail the CreateFile call
					{
						status = gOpenStatus;
						KdPrint(("LPCFILTER: PsCreateSystemThread failed\n"));
						break; // we dont free allocated data here, it is done at IRP_MJ_STOP_DEVICE
					}

					status = ObReferenceObjectByHandle(pDevExt->DispatcherThreadHandle, 
													THREAD_ALL_ACCESS, 
													NULL,
													KernelMode, 
													(PVOID*) &pDevExt->DispatcherThreadObject, 
													NULL);

					ZwClose(pDevExt->DispatcherThreadHandle);
				}
			}
			break;

			case IRP_MJ_CLOSE:
			{
		
			}
			break;


			case IRP_MJ_DEVICE_CONTROL:
			{
				switch(irpStack->Parameters.DeviceIoControl.IoControlCode)
				{

				case IOCTL_LPC_FLTR_SET_CPU_PSTATE:  // app is telling the CPU to run at a certain speed
					{
						ULONG	x = 0;
						ULONG	ProcNum;
						ULONG	PState;
						KIRQL	irql;

						if(irpStack->Parameters.DeviceIoControl.InputBufferLength != (2 * sizeof(ULONG)))
						{
							status = STATUS_INVALID_PARAMETER;
							break;
						}

						ProcNum		= *(PULONG)Irp->AssociatedIrp.SystemBuffer;
						PState	= *(((PULONG)Irp->AssociatedIrp.SystemBuffer) +1);

						if(ProcNum > pDevExt->NumProcs)
						{
							status = STATUS_INVALID_PARAMETER;
							break;
						}

						KeAcquireSpinLock(&pDevExt->AppLock, &irql);

						// if the processor isnt being used, set its state
						if(pDevExt->pCPUDB[ProcNum].PState == 0xFFFFFFFF)
						{
							InterlockedExchange(&pDevExt->pCPUDB[ProcNum].PState, PState);
							
							// reset the stay alive count for that CPU
							InterlockedExchange(&pDevExt->pCPUDB[ProcNum].StayAliveCount, 0);

							// leave status set to success
						}
						// else if it is being used, and is the same state as requested, then OK
						else
						{
							if(pDevExt->pCPUDB[ProcNum].PState == PState)
							{
								// reset the stay alive count for that CPU
								InterlockedExchange(&pDevExt->pCPUDB[ProcNum].StayAliveCount, 0);

								// leave status set to success
							}
							else
							{
								// else we couldnt set the PState for that CPU so fail the request
								status = STATUS_INVALID_DEVICE_REQUEST;
							}
						}

						KeReleaseSpinLock(&pDevExt->AppLock, irql);


					}
					break;

				case IOCTL_LPC_FLTR_SETNOW_CPU_PSTATE:
					{
						ULONG	x = 0;
						ULONG	ProcNum;
						ULONG	PState;
						KIRQL	irql;

						if(irpStack->Parameters.DeviceIoControl.InputBufferLength != (2 * sizeof(ULONG)))
						{
							status = STATUS_INVALID_PARAMETER;
							break;
						}

						ProcNum		= *(PULONG)Irp->AssociatedIrp.SystemBuffer;
						PState	= *(((PULONG)Irp->AssociatedIrp.SystemBuffer) +1);

						if(ProcNum > pDevExt->NumProcs)
						{
							status = STATUS_INVALID_PARAMETER;
							break;
						}

						KeAcquireSpinLock(&pDevExt->AppLock, &irql);

						InterlockedExchange(&pDevExt->pCPUDB[ProcNum].PState, PState);
							
						// reset the stay alive count for that CPU
						InterlockedExchange(&pDevExt->pCPUDB[ProcNum].StayAliveCount, 0);

						KeReleaseSpinLock(&pDevExt->AppLock, irql);


					}
					break;

				case IOCTL_LPC_FLTR_SET_CFG:
					{
						KIRQL	irql;

						if(irpStack->Parameters.DeviceIoControl.InputBufferLength != sizeof(LPCConfiguration))
						{
							status = STATUS_INVALID_PARAMETER;
							break;
						}

						KeAcquireSpinLock(&pDevExt->AppLock, &irql);

						memcpy(&pDevExt->Config, Irp->AssociatedIrp.SystemBuffer, sizeof(LPCConfiguration));

						KeReleaseSpinLock(&pDevExt->AppLock, irql);
					}
					break;

				case IOCTL_LPC_FLTR_GET_CFG:
					{
						KIRQL	irql;

						if(irpStack->Parameters.DeviceIoControl.OutputBufferLength != sizeof(LPCConfiguration))
						{
							status = STATUS_INVALID_PARAMETER;
							break;
						}
						KeAcquireSpinLock(&pDevExt->AppLock, &irql);

						memcpy(Irp->AssociatedIrp.SystemBuffer, &pDevExt->Config, sizeof(LPCConfiguration));

						KeReleaseSpinLock(&pDevExt->AppLock, irql);

						Irp->IoStatus.Information = sizeof(LPCConfiguration);

					}
					break;

				case IOCTL_LPC_FLTR_GET_STATS:
					{
						if(irpStack->Parameters.DeviceIoControl.OutputBufferLength == pDevExt->StatsDataSize)
						{
							KIRQL	oldirql;
							ULONG	x, y;

							memcpy(Irp->AssociatedIrp.SystemBuffer, pDevExt->pStats, pDevExt->StatsDataSize);

							Irp->IoStatus.Information = pDevExt->StatsDataSize;
						}
						else if(irpStack->Parameters.DeviceIoControl.OutputBufferLength == sizeof(ULONG))
						{
							memcpy(Irp->AssociatedIrp.SystemBuffer, &pDevExt->StatsDataSize, sizeof(ULONG));

							Irp->IoStatus.Information = sizeof(ULONG);

							status = STATUS_BUFFER_OVERFLOW;
						}
					}
					break;
				}
			}
		}

		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return(status);
	}
} 




VOID
FPFilterUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
    return;
}



