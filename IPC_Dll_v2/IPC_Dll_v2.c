/*
IPC_DLL_v2.c
Author:ashokh@microsoft.com
Last modified date: 07-01-2020

This file contains the various functions for the client side dll of the IPCDrv driver
*/

#pragma once
#include"IPC_Dll_v2.h"
#include<Windows.h>

/*
User Mode process first needs to call this function to initialize the IPC driver.
The function performs the following:
1.Calls CreateFile to get the handle to the file object of the device
2.Calls DeviceIoControl to register Read notification event with the driver
3.Creates Read thread which wait on the above event to be signalled

Function accepts PIPC_VAR as input and returns TRUE if above tasks complete successfully,
else returns FALSE. Call GetLastError() to get more info about failure
*/

BOOL InitDeviceforIPC()
{
	//Alloc memory for the IPC_VAR structure

	pIpc_Var = (PIPC_VAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IPC_VAR));

	//Open DOS Device Name and get handle to file object

	pIpc_Var->hFile = CreateFile("\\\\.\\IPCDrv",              // Name of object
		GENERIC_READ | GENERIC_WRITE, // Desired Access
		0,                            // Share Mode
		NULL,                         // reserved
		OPEN_EXISTING,                // Fail if object does not exist
		0,                            // Flags
		NULL);                        // reserved

	if (pIpc_Var->hFile == INVALID_HANDLE_VALUE)
	{
		LOG_ERROR("OpenDeviceforIPC() failed to open handle to IPC Device Object:%d\n", GetLastError());
		HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pIpc_Var);
		return FALSE;
	}

	LOG_INFO("OpenDeviceforIPC() succeeded\n");

	//Local for DeviceIoControl bytes returned

	DWORD dwBytesReturned;

	//Create Read notification event

	pIpc_Var->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (pIpc_Var->hEvent == NULL) //if it fails return NULL
	{
		LOG_ERROR("Unable to Create Read Notification Event:%d\n", GetLastError());
		CloseHandle(pIpc_Var->hFile);
		HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pIpc_Var);
		return FALSE;
	}

	//Sent IOCTL to register Read notification event to driver

	if (!DeviceIoControl(pIpc_Var->hFile,   //handle to our file object
				IOCTL_REG_EVENT,			//IOCTL
				&(pIpc_Var->hEvent),		//Input buffer
				sizeof(pIpc_Var->hEvent),	//input buffer size
				NULL,						//Output buffer
				0,							//Output buffer size
				&dwBytesReturned,			//size returned
				NULL))
	{
		printf("RegRecvNotificationEvent() failed :%d\n", GetLastError());
		return FALSE;
	}

	LOG_INFO("RegRecvNotificationEvent() succeeded\n");

	/*Create Read thread which waits on the above read event to be signalled by driver.

	pIpc_Var->hThread = CreateThread(NULL, 0, RecvIPCMsg, pIpc_Var, 0, 0);
	if (!pIpc_Var->hThread)
	{
		LOG_ERROR("Unable to create Read Thread:%d\n", GetLastError());
		return FALSE;
	}

	LOG_INFO("Read Thread creation successful\n");
	*/

	return TRUE;
}


PIPCMSG RecvIPCMsg()
{
	//Locals 

	int iRecvBufSize = sizeof(IPC_PACKET) + (INITIALRECVBUFSIZE * sizeof(char)); //Initial Read buffer size
	DWORD dwNumOfBytesRead;  //Number of Bytes Read
	BOOL bReadStatus;		 //Read Status
	DWORD dwError;			 //Error code of ReadFile operation

	//Wait on Read Notification Event
	WaitForSingleObject(pIpc_Var->hEvent, INFINITE);

	//Read Notification Event Signalled
	LOG_INFO("Received notification for Read\n");

	//Reading from Driver

	LOG_INFO("Sending read request with default buffer\n");		
	PIPC_PACKET pReceivePacket = (PIPC_PACKET)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, iRecvBufSize);
	bReadStatus = ReadFile(pIpc_Var->hFile, pReceivePacket, iRecvBufSize, &dwNumOfBytesRead, NULL);
	
	if (!bReadStatus)
	{
		dwError = GetLastError();
		LOG_ERROR("Read with default buffer failed with error %d\n", dwError);

		if (dwError == ERROR_INSUFFICIENT_BUFFER)
		{
			//if we fail with insufficient buffer, driver returns size of correct buffer size in user buffer
				
			iRecvBufSize = (int)(*(int*)pReceivePacket);

			//Free the Packet with old receive buffer size
			HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pReceivePacket);

			//Now read again with the correct buffer size
			LOG_INFO("Trying Read again with correct buffer\n");
			pReceivePacket = (PIPC_PACKET)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, iRecvBufSize);
			bReadStatus = ReadFile(pIpc_Var->hFile, pReceivePacket, iRecvBufSize, &dwNumOfBytesRead, NULL);

			if (!bReadStatus)
			{
				dwError = GetLastError();
				LOG_ERROR("Read with correct buffer size also failed with error %d\n", dwError);
				ResetEvent(pIpc_Var->hEvent);
				HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pReceivePacket);
			}
			else
			{
				PIPCMSG	pMsg = (PIPCMSG)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IPCMSG) + (pReceivePacket->header.sizeofpayload));
				pMsg->bEndofMsg = pReceivePacket->header.bEndOfPayload;
				pMsg->MsgSize = pReceivePacket->header.sizeofpayload;
				pMsg->uiMsgID = pReceivePacket->header.uiPacketid;
				pMsg->uiDestPID = (UINT)pReceivePacket->header.dwDestinationPid;
				pMsg->uiSourcePID = (UINT)pReceivePacket->header.dwSourcePid;
				memcpy(pMsg->szMsg, pReceivePacket->szbuffer, pReceivePacket->header.sizeofpayload);
				HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pReceivePacket);

				return pMsg;
			}
		}
		
	}
	PIPCMSG	pMsg = (PIPCMSG)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IPCMSG) + (pReceivePacket->header.sizeofpayload));
	pMsg->bEndofMsg = pReceivePacket->header.bEndOfPayload;
	pMsg->MsgSize = pReceivePacket->header.sizeofpayload;
	pMsg->uiMsgID = pReceivePacket->header.uiPacketid;
	pMsg->uiDestPID = (UINT)pReceivePacket->header.dwDestinationPid;
	pMsg->uiSourcePID = (UINT)pReceivePacket->header.dwSourcePid;
	memcpy(pMsg->szMsg, pReceivePacket->szbuffer, pReceivePacket->header.sizeofpayload);
	HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pReceivePacket);

	return pMsg;
}



BOOL SendIPCMsg(PIPCMSG pMsg)
{
	if (!pMsg)
	{
		LOG_ERROR("Invalid pointer\n");
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	//Locals

	BOOL fSuccess;
	DWORD dwNumofBytesWritten;
	size_t payloadbytes = pMsg->MsgSize;
	//Create IPC Packet

	PIPC_PACKET pSendPacket = (PIPC_PACKET)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IPC_PACKET) + payloadbytes);

	if (pSendPacket == NULL) //if it fails return NULL
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		LOG_ERROR("Unable to create IPC Packet:%d\n", GetLastError());
		return FALSE;
	}

	pSendPacket->header.dwSourcePid = pMsg->uiSourcePID;      //Source PID
	pSendPacket->header.dwDestinationPid = (HANDLE)pMsg->uiDestPID;	  //Destination PID
	pSendPacket->header.uiPacketid = pMsg->uiMsgID;			  //Packet ID
	pSendPacket->header.bEndOfPayload = pMsg->bEndofMsg;	  //EndofPayload
	pSendPacket->header.sizeofpayload = payloadbytes;		  //Size in bytes of payload

	memcpy(pSendPacket->szbuffer, pMsg->szMsg, payloadbytes); //Mem Copy  

	LOG_INFO("IPC Packet created and ready to be sent\n");

	//Send Write IRP to our device/driver

	fSuccess = WriteFile(pIpc_Var->hFile,						//handle to file object
		pSendPacket,											//Buffer to write
		sizeof(IPC_PACKET) + payloadbytes,						//size of buffer
		&dwNumofBytesWritten,									//Num of bytes written
		NULL);

	if (!fSuccess)
	{
		LOG_ERROR("Sending IPC message/WriteFile failed:%d\n", GetLastError());
	}
	else
	{
		LOG_INFO("Sent IPC Message to driver\n");
	}

	//Free Heap for the IPC Packet
	HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pSendPacket);

	return fSuccess;
}

BOOL CloseDeviceforIPC()
{
	CloseHandle(pIpc_Var->hEvent);
	CloseHandle(pIpc_Var->hFile);
	HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pIpc_Var);
	return TRUE;
}



