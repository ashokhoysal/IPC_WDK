#pragma once
#include<stdio.h>
#include<Windows.h>
#include<conio.h>
#include"C:\Users\ashokh\source\repos\IPC_Proj\IPC_Dll_v2\IPC_Dll_v2.h"

typedef BOOL(*MYPROC)();
typedef PIPCMSG(*MYPROC1)();
typedef BOOL(*MYPROC2)(PIPCMSG);

MYPROC _InitDeviceforIPC;
MYPROC _CloseDeviceforIPC;
MYPROC1 _RecvIPCMsg;
MYPROC2 _SendIPCMsg;

HANDLE g_hEvent;
HMODULE hIPCDll;

char* randstr();
DWORD WINAPI ProcessReceivedIPCMsg(LPVOID);
