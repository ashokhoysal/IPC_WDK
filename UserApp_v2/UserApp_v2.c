/*
UserApp_v2.c
Author:ashokh@microsoft.com
Last modified date: 07-01-2020

This is the testcase User App to demonstrate the behaviour of IPC communication between processes
*/

#include"UserApp_v2.h"
#include<time.h>

//random string generator
char* randstr()
{
	int sz = rand() % 20;
	char* arr = (char*)malloc(sz * sizeof(char));
	int i;
	for (i = 0; i < sz; i++)
	{
		*(arr + i) = ('A' + (rand() % 26));
	}
	arr[sz] = '\0';
	return arr;
}

DWORD WINAPI ProcessReceivedIPCMsg(LPVOID pParam)
{
	_RecvIPCMsg = (MYPROC1)GetProcAddress(hIPCDll, "RecvIPCMsg");

	while (1)
	{
		PIPCMSG pMyMsg = _RecvIPCMsg();
		printf("Received Msg %d from Process %d: %s\n", pMyMsg->uiMsgID,pMyMsg->uiSourcePID, pMyMsg->szMsg);
		HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pMyMsg);
	}
	return 0;
}

int main()
{
	//locals 

	char prompt;				//user choice prompt
	HANDLE hThread;				//Handle to Thread to process the received payload
	PIPCMSG pMyMsg;				//pointer to Sample Message

	//Loading IPC_DLL_v2.dll explicitly and getting the relevant function pointers
	hIPCDll = LoadLibraryExW(L"IPC_DLL_v2", NULL, 0);
	if (hIPCDll == NULL)
	{
		printf("Unable to load IPC_DLL_v2.dll:%d\n", GetLastError());
		return FALSE;
	}

	_InitDeviceforIPC = (MYPROC)GetProcAddress(hIPCDll, "InitDeviceforIPC");
	_CloseDeviceforIPC = (MYPROC)GetProcAddress(hIPCDll, "CloseDeviceforIPC");
	_SendIPCMsg = (MYPROC2)GetProcAddress(hIPCDll, "SendIPCMsg");

	//First initialize the Device/driver for IPC Communication

	if(!_InitDeviceforIPC())
	{
		printf("Unable to Initialize Device for IPC:%d\n", GetLastError());
		return -1;
	}

	printf("Successfully initialized Device for IPC\n");

	//Create Thread to wait for the received message and process it

	hThread = CreateThread(NULL, 0, ProcessReceivedIPCMsg, (LPVOID)NULL,0,NULL);
	if (hThread == NULL)
	{
		printf("Unable to Create Receive Message Thread:%d\n", GetLastError());
		return -1;
	}

	//Send message or quit option

	while (1)
	{
		printf("Press 's' to send 10 sample messages to a process or 'q' to quit.\n\n");
		scanf_s("%c", &prompt, 1);
		getchar();

		if(('s' == prompt) || ('S' == prompt)) //Send Message option
		{
			UINT uiDestPid;
			printf("Enter the pid of the process to which to send the message\n");
			scanf_s("%d", &uiDestPid, sizeof(UINT));
			getchar();

			srand(time(0)); 

			for (int i = 1; i <= 10; i++) //Send 10 Msgs
			{
				//Generate random string
				char * sz = randstr();
				size_t ilen = strlen(sz);

				//Allocate Heap for Msg
				pMyMsg = (PIPCMSG)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IPCMSG)+ilen);
				if (!pMyMsg)
				{
					SetLastError(ERROR_NOT_ENOUGH_MEMORY);
					printf("Unable to create Msg:%d\n", GetLastError());
					return -1;
				}

				//Create Msg
				pMyMsg->uiMsgID = i;
				pMyMsg->bEndofMsg = TRUE;
				pMyMsg->uiDestPID = uiDestPid;
				pMyMsg->uiSourcePID = GetCurrentProcessId();
				pMyMsg->MsgSize = ilen;
				memcpy(pMyMsg->szMsg, sz, ilen);

				//Send Msg
				if (_SendIPCMsg(pMyMsg))
				{
					printf("Sent Msg %d to Process %d: %s\n", i, uiDestPid,sz);
				}
				else
				{
					printf("Sending Msg %d failed with error : %d\n", i, GetLastError());
				}

				//Free alloc for Msg
				HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pMyMsg);
			}
		}

		else if (('q' == prompt) || ('Q' == prompt)) //Quit option
		{
			//Terminate the Read Thread
			TerminateThread(hThread, 0);

			//Closing IPC Device
			if(!_CloseDeviceforIPC())
			{
				printf("Unable to close the Device for IPC:%d\n", GetLastError());
				return -1;
			}

			printf("Successfully closed device for IPC\n");
			
			return 1;
		}

		else
		{
			printf("Only 'w' and 'r' are valid operation. Press 'q' to quit\n");
		}
	}

	getchar();

}


