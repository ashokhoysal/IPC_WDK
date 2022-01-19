
//=====================================================================
// IPC- Inter Process Communication Driver Header File
//
// This file contains Macro Definitions, structure definitions and Function declarations
// used in the IPCDrv.c file
//
//  Last Modified : 01 Jan 2020 ashokh
//=====================================================================

#pragma once

//Include Files

#include <ntddk.h>

//Constants

#define NT_DEVICE_NAME      L"\\Device\\IPCDrv"          //Our Device Name(NT)
#define DOS_DEVICE_NAME     L"\\DosDevices\\IPCDrv"      //Our Device Name(DOS)
#define IPC_DEVICE_TYPE 40000							 //DeviceType used in CTL_CODE Macro
#define IOCTL_REG_EVENT\
 CTL_CODE(IPC_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_DATA|FILE_WRITE_DATA) //Read Notification Event IOCTL


//Structure definitions

//The IPC_PORT structure acts as a port which the driver maintains 
//for every User mode process which interfaces with the driver

typedef struct _IPC_PORT
{
	HANDLE dwPID;			//PID of the User mode process which calls CreateFile to get a handle to the Device
	PKEVENT pKevent;		//Read Notification event which is registered by the User mode process
	LIST_ENTRY list_entry;  //Doubly linked List Entry, the driver maintains a list of User mode Ports
	PFILE_OBJECT pFileObj;  //Pointer to File object which is unique to every User mode process, our driver uses this to maintain packet queues for this process
}IPC_PORT, *PIPC_PORT;

//The IPC_PACKET_QUEUE structure contains the ListHead for the Incoming and Outgoing Packet queues
//It also contains the Spin Lock used for Synchronizing List Access

typedef struct _IPC_PACKET_QUEUE
{
	LIST_ENTRY Ipc_Pkt_In_Queue;			//ListHead for Incoming Packet Queue
	LIST_ENTRY Ipc_Pkt_Out_Queue;			//ListHead for Outgoing Packet Queue
	KSPIN_LOCK Ipc_Pkt_In_Queue_SpinLock;	//Spinlock for synchronizing Incoming Packet Queue Access
	KSPIN_LOCK Ipc_Pkt_Out_Queue_SpinLock;	//Spinlock for synchronizing Outgoing Packet Queue Access
}IPC_PACKET_QUEUE, *PIPC_PACKET_QUEUE;

//The IPC_PACKET struct definition of the actual message/packet
//passed between 2 UserMode processes

typedef struct _IPC_PACKET {
	struct _header {					//Packet Header which contains some metadata about the message
		DWORD32 dwSourcePid;			//Source process PID which initiated the message
		HANDLE dwDestinationPid;		//Destination process PID to which the message is targetted
		size_t sizeofpayload;			//Size of the payload(buffer)
		UINT32 nPacketid;				//Packet ID
		UINT32 EndofPacket;				//1 indicates End of this Packet
	}header;
	LIST_ENTRY list_entry;				//List entry used to queue the packets
	char szbuffer[];					//Flexible Array Member buffer,can contain variable size of chars
}IPC_PACKET, *PIPC_PACKET;

//The IPC_PKTCPY_WKITEM structure definition of the context 
//passed to the Worker Thread Callback routine

typedef struct _IPC_PKTCPY_WKITEM
{
	PIPC_PACKET pIPC_Pkt;				//IPC Packet which needs to be copied
	PIO_WORKITEM pWorkItem;				//WorkItem
}IPC_PKTCPY_WKITEM, *PIPC_PKTCPY_WKITEM;

PLIST_ENTRY g_IPCPort_Queue;			//Global IPCPort queue maintained by our driver which is a Doubly linked list of Ports for every User mode process
PKSPIN_LOCK g_IPCPort_Queue_SpinLock;   //Global Spinlock to synchronize access to the global IPCPort queue

//Function Prototypes

//Called when driver is initialized/loaded
NTSTATUS DriverEntry(IN OUT PDRIVER_OBJECT pDriverObject,
	IN PUNICODE_STRING    pRegistryPath);

//Called when driver is unloaded
VOID IPCDrvUnloadDriver(IN PDRIVER_OBJECT  pDriverObject);

//Called when a Create IRP is sent to the driver
NTSTATUS IPCDrvCreate(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP           pIrp);

//Called when a Close IRP is sent to the driver
NTSTATUS IPCDrvClose(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP           pIrp);

//Called when a IOCTL is sent to the driver
NTSTATUS IPCDrvDevIOCTL(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP           pIrp);

//Called when a Write IRP is sent to the driver
NTSTATUS IPCDrvWrite(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP           pIrp);

//Called when a Read IRP is sent to the driver
NTSTATUS IPCDrvRead(IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP           pIrp);

//System Worker Thread Workitem callback routine
IO_WORKITEM_ROUTINE WorkItemCallback;

/*Compiler Directives
* These compiler directives tell the OS how to load the driver into memory.
* INIT is for onetime initialization code that can be permanently discarded after the driver is loaded.
* PAGE code must run at passive and not raise IRQL >= dispatch.*/

#pragma alloc_text( INIT, DriverEntry )
#pragma alloc_text( PAGE, IPCDrvUnloadDriver)
#pragma alloc_text( PAGE, IPCDrvCreate)
#pragma alloc_text( PAGE, IPCDrvClose)
#pragma alloc_text( PAGE, IPCDrvDevIOCTL)
#pragma alloc_text( PAGE, IPCDrvWrite)
#pragma alloc_text( PAGE, IPCDrvRead)
#pragma alloc_text( PAGE, WorkItemCallback)

