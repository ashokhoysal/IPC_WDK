//=====================================================================
// IPC- Inter Process Communication Driver
//
// This driver facilitates passing of messages or transfer of packets (i.e reading and writing data) 
// between User mode processes using Buffered IO 
//
//  Last Modified : 01 Jan 2020 ashokh
//=====================================================================

//Include Files
#pragma once
#include <ntddk.h>
#include"IPCDrv_v2.h"


//=====================================================================
// DriverEntry
//
// This routine is called by the Operating System to initialize 
// the driver. It creates the device object, fills in the dispatch 
// entry points and completes the initialization. IO manager provides/fills the 
// Driver Object and Registry Path arguements
//
// Returns STATUS_SUCCESS if initialized; an error otherwise.
//=====================================================================

NTSTATUS DriverEntry(IN OUT PDRIVER_OBJECT pDriverObject,
	IN PUNICODE_STRING pRegistryPath)
{
	//Locals

	NTSTATUS ntStatus;

	PDEVICE_OBJECT pDeviceObject = NULL;  // Pointer to our new device object
	UNICODE_STRING usDeviceName;          // Device Name
	UNICODE_STRING usDosDeviceName;       // DOS Device Name

	DbgPrint("DriverEntry Called\r\n");

	//Initialize Unicode Strings

	RtlInitUnicodeString(&usDeviceName, NT_DEVICE_NAME);
	RtlInitUnicodeString(&usDosDeviceName, DOS_DEVICE_NAME);

	//Create Device Object

	ntStatus = IoCreateDevice(
		pDriverObject,              // Driver Object
		0,                          // No device extension
		&usDeviceName,              // Device name "\Device\IPCDrv"
		FILE_DEVICE_UNKNOWN,        // Device type
		FILE_DEVICE_SECURE_OPEN,    // Device characteristics
		FALSE,                      // Not an exclusive device
		&pDeviceObject);            // Returned pointer to Device Object

	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("Could not create the device object\n");
		return ntStatus;
	}

	//Initialize the entry points in the driver object 

	pDriverObject->MajorFunction[IRP_MJ_CREATE] = IPCDrvCreate;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = IPCDrvClose;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IPCDrvDevIOCTL;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = IPCDrvWrite;
	pDriverObject->MajorFunction[IRP_MJ_READ] = IPCDrvRead;

	pDriverObject->DriverUnload = IPCDrvUnloadDriver;

	//Set Flags

	//With Buffered IO the IO manager creates a System buffer in NonPagedPool for read or write.
	//The AssociatedIRP.SystemBuffer describes the Virtual Address of the System buffer which is passed 
	//to the driver.      

	pDeviceObject->Flags |= DO_BUFFERED_IO;

	//Clearing the following flag is not really necessary
	//because the IO Manager will clear it automatically.

	pDeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);

	//Create a symbolic link 

	ntStatus = IoCreateSymbolicLink(&usDosDeviceName, &usDeviceName);

	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("Could not create the symbolic link\n");
		IoDeleteDevice(pDeviceObject);
		return ntStatus;
	}

	//Allocate and Initialize the global IPC_Port queue ListHead and the global IPC_Port queue Spinlock

	if (!g_IPCPort_Queue)
	{
		//allocate a buffer in NonPagedPool with tag 

		g_IPCPort_Queue = ExAllocatePoolWithTag(NonPagedPool, sizeof(LIST_ENTRY), (LONG)'1CPI');
		if (!g_IPCPort_Queue)
		{
			DbgPrint("Failed to allocate NonPaged pool for global IPC Port queue\n");
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			return ntStatus;
		}

		//initialize list head

		InitializeListHead(g_IPCPort_Queue);
	}

	if (!g_IPCPort_Queue_SpinLock)
	{
		//allocate a buffer in NonPagedPool with tag

		g_IPCPort_Queue_SpinLock = ExAllocatePoolWithTag(NonPagedPool, sizeof(KSPIN_LOCK), (LONG)'1CPI');
		if (!g_IPCPort_Queue_SpinLock)
		{
			DbgPrint("Failed to allocate Nonpaged pool for global IPC Port queue Spinlock \n");
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			return ntStatus;
		}

		//initialize Spin Lock

		KeInitializeSpinLock(g_IPCPort_Queue_SpinLock);
	}

	DbgPrint("DriverEntry Succeeded\r\n");
	return ntStatus;
}



//=====================================================================
// IPCDrvCreate
//
// This routine is called by the IO system when the IPCDrv device is 
// opened  (CreateFile).
//=====================================================================

NTSTATUS IPCDrvCreate(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP           pIrp)
{
	DbgPrint("IPCDrvCreate Called\r\n");

	//locals

	NTSTATUS ntStatus;
	PIO_STACK_LOCATION pIoStackIrp = NULL;  //pointer to IO Stack Location
	PIPC_PORT pIPCPort;						//IPC Port structure for the user process
	PIPC_PACKET_QUEUE pIPC_Pkt_Queue;		//Structure for incoming and outgoing queue of packets

	//Allocate NPP for the user process IPC PORT Structure

	pIPCPort = (PIPC_PORT)ExAllocatePoolWithTag(NonPagedPool, sizeof(IPC_PORT), (LONG)'1CPI');
	if (!pIPCPort)
	{
		DbgPrint("Failed to allocate Nonpaged pool for global IPC Port queue Spinlock \n");
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		return ntStatus;
	}

	//Allocate NPP for the IPC_PACKET_QUEUE structure

	pIPC_Pkt_Queue = (PIPC_PACKET_QUEUE)ExAllocatePoolWithTag(NonPagedPool, sizeof(IPC_PACKET_QUEUE), (LONG)'1CPI');
	if (!pIPC_Pkt_Queue)
	{
		DbgPrint("Failed to allocate Nonpaged pool for global IPC Port queue Spinlock \n");
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		return ntStatus;
	}

	pIoStackIrp = IoGetCurrentIrpStackLocation(pIrp); //Get Current IRP Stack Location

	pIPCPort->dwPID = PsGetCurrentProcessId();  //The PID of the user process which called CreateFile

	pIPCPort->pFileObj = pIoStackIrp->FileObject;  //FileObject acts as the unique port identifier for each process

	pIPCPort->pFileObj->FsContext2 = pIPC_Pkt_Queue; //We use the FsContext2 member of the FileObject for our IPC Packet queues

	//Initialize the List Heads and Spin Locks 

	InitializeListHead(&(pIPC_Pkt_Queue->Ipc_Pkt_In_Queue));
	InitializeListHead(&(pIPC_Pkt_Queue->Ipc_Pkt_Out_Queue));
	KeInitializeSpinLock(&(pIPC_Pkt_Queue->Ipc_Pkt_In_Queue_SpinLock));
	KeInitializeSpinLock(&(pIPC_Pkt_Queue->Ipc_Pkt_Out_Queue_SpinLock));

	//Queue the user process IPCPort structure to our global list of IPC Ports

	ExInterlockedInsertTailList(g_IPCPort_Queue, &(pIPCPort->list_entry), g_IPCPort_Queue_SpinLock);

	//Complete the IRP

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	DbgPrint("IPCDrvCreate Succeeded\r\n");

	return STATUS_SUCCESS;
}



//=====================================================================
// IPCDrvDevIOCTL
//
// This routine is called when a IOCTL is 
// issued on the device handle. This version uses Buffered I/O.
//=====================================================================

NTSTATUS IPCDrvDevIOCTL(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP           pIrp)
{
	DbgPrint("IPCDrvDevIOCTL Called\r\n");

	//locals

	PIO_STACK_LOCATION pIoStackIrp = NULL;
	HANDLE hUevent;
	PKEVENT pKevent = NULL;
	NTSTATUS NtStatus;

	pIoStackIrp = IoGetCurrentIrpStackLocation(pIrp);

	//IOCTL code sent by user mode is present in pIoStackIrp->Parameters.DeviceIoControl.IoControlCode
	//In our case we need to check for the IOCTL_REG_EVENT code

	switch (pIoStackIrp->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_REG_EVENT:    //Read notification IOCTL send from user mode

		//Check the input buffer size, if lesser return Buffer Too Small Error and complete IRP

		if (pIoStackIrp->Parameters.DeviceIoControl.InputBufferLength < sizeof(PHANDLE))
		{
			DbgPrint("Buffer too small\n");
			pIrp->IoStatus.Status = STATUS_FLT_BUFFER_TOO_SMALL;
			pIrp->IoStatus.Information = sizeof(PHANDLE);
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			return STATUS_FLT_BUFFER_TOO_SMALL;
		}
		//Get the user mode handle using buffered IO

		hUevent = *(PHANDLE)pIrp->AssociatedIrp.SystemBuffer;

		//Get reference to the corresponding Kernel event object

		NtStatus = ObReferenceObjectByHandle(hUevent,     //User mode handle
								EVENT_MODIFY_STATE,		//Handle access type
								*ExEventObjectType,		//Notification Event
								pIrp->RequestorMode,	//Processor Mode
								(PVOID*)&pKevent,       //pointer to KEVENT object is copied here
								NULL);

		if (!NT_SUCCESS(NtStatus))
		{
			DbgPrint("Failed to get reference to Kernel Handle object \n");
			return NtStatus;
		}
		break;

	default:
		DbgPrint("Invalid IOCTL code\n");
		NtStatus = STATUS_INVALID_PARAMETER;
		return NtStatus;
	}

	//Now get the User process port and update the Kevent info

	PLIST_ENTRY pTemp_IPCPort_Queue = g_IPCPort_Queue->Flink;
	PIPC_PORT pTemp_IPCPort;

	while (pTemp_IPCPort_Queue != g_IPCPort_Queue)
	{
		pTemp_IPCPort = CONTAINING_RECORD(pTemp_IPCPort_Queue, IPC_PORT, list_entry);
		if (pTemp_IPCPort->pFileObj == pIoStackIrp->FileObject)   //This is our user process port
		{
			pTemp_IPCPort->pKevent = pKevent;  //Save the kevent
			break;
		}
		pTemp_IPCPort_Queue = pTemp_IPCPort_Queue->Flink;
	}

	ObDereferenceObject(pKevent);  //derefernce the object

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	DbgPrint("IPCDrvDevIOCTL Succeeded\r\n");
	return STATUS_SUCCESS;

}



//=====================================================================
// IPCDrvWrite
//
// This routine is called when a write (WriteFile/WriteFileEx) is 
// issued on the device handle. This version uses Buffered I/O.
//=====================================================================

NTSTATUS IPCDrvWrite(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP           pIrp)
{
	//Locals

	size_t uiLength;                           //size of input buffer
	PIO_STACK_LOCATION pIoStackIrp = NULL;	   //IO Stack location
	PIPC_PACKET pTemp_Out_IPCPkt;			   //Send IPC Packet
	PIPC_PKTCPY_WKITEM pIPC_PktCpy_WkItem;     //Work Item context
	KIRQL Irql;								   //Irql (for use with spinlock calls) 

	DbgPrint("IPCDrvWrite Called\r\n");

	//Retrieve Pointer To Current IRP Stack Location    

	pIoStackIrp = IoGetCurrentIrpStackLocation(pIrp);

	//Get Buffer Using SystemAddress Parameter From IRP

	uiLength = pIoStackIrp->Parameters.Write.Length;

	//Check to make sure that the input buffer size is correct

	if (uiLength >= (sizeof(IPC_PACKET) + ((PIPC_PACKET)pIrp->AssociatedIrp.SystemBuffer)->header.sizeofpayload))
	{
		//Allocate NPP for the IPC Packet

		pTemp_Out_IPCPkt = ExAllocatePoolWithTag(NonPagedPool, uiLength, (LONG)'1CPI');
		RtlZeroMemory(pTemp_Out_IPCPkt, uiLength);

		//Copy the user buffer into the device/driver buffer

		RtlCopyMemory(pTemp_Out_IPCPkt, pIrp->AssociatedIrp.SystemBuffer, uiLength);
			
		//Queue the IPC Packet to the Outgoing queue of the IPC Packet queue(Fscontext2)

		KeAcquireSpinLock(&(((PIPC_PACKET_QUEUE)(pIoStackIrp->FileObject->FsContext2))->Ipc_Pkt_Out_Queue_SpinLock), &Irql);
		InsertTailList(&(((PIPC_PACKET_QUEUE)(pIoStackIrp->FileObject->FsContext2))->Ipc_Pkt_Out_Queue), &(pTemp_Out_IPCPkt->list_entry));
		KeReleaseSpinLock(&(((PIPC_PACKET_QUEUE)(pIoStackIrp->FileObject->FsContext2))->Ipc_Pkt_Out_Queue_SpinLock), Irql);
		//we can also do ExInterlockedInsertTailList(&(((PIPC_PACKET_QUEUE)(pIoStackIrp->FileObject->FsContext2))->Ipc_Pkt_Out_Queue), &(pTemp_Out_IPCPkt->list_entry), &(((PIPC_PACKET_QUEUE)(pIoStackIrp->FileObject->FsContext2))->Ipc_Pkt_Out_Queue_SpinLock));

		//Allocate NPP for work item context and queue it. Work item will copy the IPC packet
		//from current process ports outgoing queue to destination process port's incoming queue

		pIPC_PktCpy_WkItem = ExAllocatePoolWithTag(NonPagedPool, sizeof(IPC_PKTCPY_WKITEM), (LONG)'1CPI');
		pIPC_PktCpy_WkItem->pIPC_Pkt = pTemp_Out_IPCPkt;
		pIPC_PktCpy_WkItem->pWorkItem = IoAllocateWorkItem(pDeviceObject);
		IoQueueWorkItem(pIPC_PktCpy_WkItem->pWorkItem, (PIO_WORKITEM_ROUTINE)WorkItemCallback, DelayedWorkQueue, pIPC_PktCpy_WkItem);

		pIrp->IoStatus.Status = STATUS_SUCCESS;
		pIrp->IoStatus.Information = 0;

		IoCompleteRequest(pIrp, IO_NO_INCREMENT);

		DbgPrint("IPCDrvWrite Succeeded\r\n");
		return STATUS_SUCCESS;
	}
	else

	{
		DbgPrint("Incorrect input buffer size\n");
		return STATUS_INVALID_PARAMETER;	
	}
	
}


//=====================================================================
// WorkItemCallback
//
// This is the Work Item Callback function queued by (WriteFile) 
// Worker thread copies the IPC packet from the source process Outgoing queue to
// the destination process incoming queue and sets the read notification event
//=====================================================================

void WorkItemCallback(PDEVICE_OBJECT DeviceObject, PIPC_PKTCPY_WKITEM pIPC_PktCpy_WI)
{
	DbgPrint("Worker Thread Routine Start\n");

	//Locals 

	PLIST_ENTRY pTemp_IPCPort_Queue = g_IPCPort_Queue->Flink;
	PIPC_PORT pTemp_IPCPort = NULL;
	size_t uiLength;
	KIRQL Irql;

	//Search the Global IPC Port queue for the destination process port
	while (pTemp_IPCPort_Queue != g_IPCPort_Queue)
	{
		pTemp_IPCPort = CONTAINING_RECORD(pTemp_IPCPort_Queue, IPC_PORT, list_entry);
		if (pTemp_IPCPort->dwPID == pIPC_PktCpy_WI->pIPC_Pkt->header.dwDestinationPid)
		{
			//We have our destination port now
			//Create a new In IPC Packet and copy the existing IPC Packet
			uiLength = sizeof(IPC_PACKET) + pIPC_PktCpy_WI->pIPC_Pkt->header.sizeofpayload;
			PIPC_PACKET pIPC_In_Pkt = ExAllocatePoolWithTag(NonPagedPool, uiLength, (LONG)'1CPI');
			RtlCopyMemory(pIPC_In_Pkt, pIPC_PktCpy_WI->pIPC_Pkt, uiLength);

			//Queue the In IPC packet to the Incoming queue of the destination process

			KeAcquireSpinLock(&(((PIPC_PACKET_QUEUE)(pTemp_IPCPort->pFileObj->FsContext2))->Ipc_Pkt_In_Queue_SpinLock), &Irql);
			InsertTailList(&(((PIPC_PACKET_QUEUE)(pTemp_IPCPort->pFileObj->FsContext2))->Ipc_Pkt_In_Queue), &(pIPC_In_Pkt->list_entry));
			KeSetEvent(pTemp_IPCPort->pKevent, 0, FALSE);  //Notify the destination process Read Thread
			KeReleaseSpinLock(&(((PIPC_PACKET_QUEUE)(pTemp_IPCPort->pFileObj->FsContext2))->Ipc_Pkt_In_Queue_SpinLock), Irql);
			//We can also do ExInterlockedInsertTailList(&(((PIPC_PACKET_QUEUE)(pTemp_IPCPort->pFileObj->FsContext2))->Ipc_Pkt_In_Queue), &(pIPC_In_Pkt->list_entry), &(((PIPC_PACKET_QUEUE)(pTemp_IPCPort->pFileObj->FsContext2))->Ipc_Pkt_In_Queue_SpinLock)); 

			break;
		}
		pTemp_IPCPort_Queue = pTemp_IPCPort_Queue->Flink;
	}
	
	//Free and Deallocate the Work Item
	IoFreeWorkItem(pIPC_PktCpy_WI->pWorkItem);
	ExFreePoolWithTag(pIPC_PktCpy_WI, (LONG)'1CPI');

	DbgPrint("Worker Thread Routine End\n");
}



//=====================================================================
// IPCDrvRead
//
// This routine is called when a read (ReadFile/ReadFileEx) is 
// issued on the device handle. This version uses Buffered I/O.
//=====================================================================

NTSTATUS IPCDrvRead(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP pIrp)
{
	//Locals
	unsigned int uiLength;
	PIO_STACK_LOCATION pIoStackIrp = NULL;
	KIRQL Irql;

	DbgPrint("IPCDrvRead Called\r\n");

	//Retrieve Pointer to Current IRP Stack Location

	pIoStackIrp = IoGetCurrentIrpStackLocation(pIrp);

	//Get Buffer using AssociatedIRP.SystemBuffer Parameter from IRP

	uiLength = pIoStackIrp->Parameters.Read.Length;

	//Dequeue the IPC Packet from the Incoming queue 
	KeAcquireSpinLock(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue_SpinLock), &Irql);
	PLIST_ENTRY pTemp_ListEntry = RemoveHeadList(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue));
	KeReleaseSpinLock(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue_SpinLock), Irql);
	//PLIST_ENTRY pTemp_ListEntry = ExInterlockedRemoveHeadList(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue), &(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue_SpinLock));
	PIPC_PACKET pTemp_IPC_In_Pkt = CONTAINING_RECORD(pTemp_ListEntry, IPC_PACKET, list_entry);

	//Check if the output buffer sent by ReadFile is correct or not

	if (uiLength < (sizeof(IPC_PACKET) + (pTemp_IPC_In_Pkt->header.sizeofpayload)))
	{
		//Output Buffer is small, in this case we do this
		//1.Calculate the required buffer size
		//2.Copy the buffer size to output buffer
		//3.Return Warning Status - this way IO manager will copy the required size to output buffer and 
		// user mode can reissue ReadFile with correct buffer size

		pIrp->IoStatus.Status = STATUS_FLT_BUFFER_TOO_SMALL;
		int iRequiredBufferSize = sizeof(IPC_PACKET) + (pTemp_IPC_In_Pkt->header.sizeofpayload);
		pIrp->IoStatus.Information = sizeof(int);
		RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer, &iRequiredBufferSize, sizeof(int));
		
		//Queue the packet back 
		KeAcquireSpinLock(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue_SpinLock), &Irql);
		InsertTailList(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue), pTemp_ListEntry);
		KeReleaseSpinLock(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue_SpinLock), Irql);
		//ExInterlockedInsertTailList(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue), pTemp_ListEntry, &(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue_SpinLock));
		
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return STATUS_FLT_BUFFER_TOO_SMALL;
	}

	//If output buffer size is correct proceed with copy

	RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer, pTemp_IPC_In_Pkt, uiLength);

	//If Incoming IPC Packet queue is empty reset the Read Event

	KeAcquireSpinLock(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue_SpinLock), &Irql);
	if (IsListEmpty(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue)))
	{
		PLIST_ENTRY pTemp_IPCPort_Queue = g_IPCPort_Queue->Flink;
		PIPC_PORT pTemp_IPCPort = NULL;

		while (pTemp_IPCPort_Queue != g_IPCPort_Queue)
		{
			pTemp_IPCPort = CONTAINING_RECORD(pTemp_IPCPort_Queue, IPC_PORT, list_entry);
			if (pTemp_IPCPort->pFileObj == pIoStackIrp->FileObject)
			{
				KeClearEvent(pTemp_IPCPort->pKevent);
				break;
			}
			pTemp_IPCPort_Queue = pTemp_IPCPort_Queue->Flink;
		}
	}
	KeReleaseSpinLock(&(((PIPC_PACKET_QUEUE)pIoStackIrp->FileObject->FsContext2)->Ipc_Pkt_In_Queue_SpinLock), Irql);
	
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = sizeof(IPC_PACKET) + (pTemp_IPC_In_Pkt->header.sizeofpayload); //Number of bytes IO manager should copy back to UserBuffer
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}



//=====================================================================
// IPCDrvClose
//
// This routine is called by the IO system when the IPCDrv device is 
// closed (CloseHandle).
//=====================================================================

NTSTATUS IPCDrvClose(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP           pIrp)
{
	DbgPrint("IPCDrvClose Called\r\n");

	PIO_STACK_LOCATION pIoStackIrp;
	pIoStackIrp = IoGetCurrentIrpStackLocation(pIrp);
	
	pIoStackIrp->FileObject;

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}



//=====================================================================
// IPCDrvUnloadDriver
//
// This routine is called by the IO system to unload the driver. Any 
// resources previously allocated must be freed.
//=====================================================================

VOID IPCDrvUnloadDriver(IN PDRIVER_OBJECT  pDriverObject)
{
	//Locals

	PDEVICE_OBJECT pDeviceObject = NULL;  // Pointer to device object
	UNICODE_STRING usDeviceName;          // Device Name
	UNICODE_STRING usDosDeviceName;       // DOS Device Name

	DbgPrint("BasicUnloadDriver Called\r\n");

	//Initialize Unicode Strings for the IoDeleteSymbolicLink call

	RtlInitUnicodeString(&usDeviceName, NT_DEVICE_NAME);
	RtlInitUnicodeString(&usDosDeviceName, DOS_DEVICE_NAME);

	//Delete Symbolic Link

	IoDeleteSymbolicLink(&usDosDeviceName);

	//Delete Device Object

	pDeviceObject = pDriverObject->DeviceObject;

	if (pDeviceObject != NULL)
	{
		IoDeleteDevice(pDeviceObject);
	}

	//Free the buffer

	if (g_IPCPort_Queue)
	{
		ExFreePoolWithTag(g_IPCPort_Queue, (LONG)'1CPI');
	}
}

