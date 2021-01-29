#pragma once

#include <Windows.h>
#include "Shared.h"
#include "Logging.h"

extern HANDLE SocketBusyMutex;
extern HANDLE ThreadSenderSemaphore;
extern DWORD ThreadsState;
extern DWORD FileToSendSize;
extern DWORD FileToSendSectionsCount;
extern HANDLE FileToSend;
extern char* FileToSendName;
extern char* FileToSendNoPathName;
extern char* FileToSendDataBuf;
extern CONNECTIONPARAMS* ConnectParams;
extern HWND StatusStatic;
extern DWORD FileToSendCurrentSection;
extern DWORD FileTransferringMode;

DWORD WINAPI ThreadReceiver(LPVOID SocketPtr);