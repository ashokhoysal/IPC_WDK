#pragma once
#define IPC_DEVICE_TYPE 40000	//IPC_Device_Type code for creating IOCTL
#define IOCTL_REG_EVENT\
 CTL_CODE(IPC_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_DATA|FILE_WRITE_DATA) // Read notification event IOCTL
#define INITIALRECVBUFSIZE 255  //Initial receive size buffer to be used

//This structure holds data pertaining to each user mode process interacting with the device/drive for IPC

typedef struct _IPC_VAR {
	HANDLE hFile;		//handle to file object
	HANDLE hEvent;		//handle to Read notification event passed to driver
	//HANDLE hThread;		//handle to Read IPC message thread
}IPC_VAR, *PIPC_VAR;

//Global pointer to our IPC_VAR structure

PIPC_VAR pIpc_Var;

//Definition of the IPC_PACKET which is sent to the driver

typedef struct _IPC_PACKET {
	struct _header {
		DWORD32 dwSourcePid;			//Source PID
		HANDLE dwDestinationPid;		//Destination PID	
		size_t sizeofpayload;			//size of payload in bytes
		UINT uiPacketid;				//Packet ID
		BOOL bEndOfPayload;				//End of Payload
	}header;
	LIST_ENTRY list_entry;				//List_Entry structure for queuing IPC Packets
	char szbuffer[];					//Flexible Array Member of structure for variable size payload
}IPC_PACKET, *PIPC_PACKET;