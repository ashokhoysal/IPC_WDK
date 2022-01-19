#pragma once
#include<stdio.h>
#include<Windows.h>
#include"IPC_Dll_v2_Private.h"
#include"IPC_Dll_v2_Debug.h"

//IPCMSG structure to be used by the client for sending messages
typedef struct _IPCMSG
{
	UINT uiMsgID;		//Message ID
	UINT uiSourcePID;	//uiSourcePID
	UINT uiDestPID;		//Destination process PID
	size_t MsgSize;		//Message Size
	BOOL bEndofMsg;		//End of Message Flag
	char szMsg[];		//Message in the form of string
}IPCMSG, *PIPCMSG;

BOOL InitDeviceforIPC();
BOOL SendIPCMsg(PIPCMSG);
PIPCMSG RecvIPCMsg();
BOOL CloseDeviceforIPC();

