#pragma once

#include <Windows.h>
#include "Shared.h"
#include "Logging.h"

extern DWORD ProgramMode;
extern HANDLE SocketBusyMutex;
extern HANDLE ThreadSenderSemaphore;
extern DWORD ThreadsState;
extern LARGE_INTEGER FileToSendSize;
extern DWORD FileToSendSectionsCount;
extern HANDLE FileToSend;
extern LPWSTR FileToSendName;
extern LPWSTR FileToSendNoPathName;
extern char *FileToSendDataBuf;
extern CONNECTIONPARAMS* ConnectParams;
extern HWND StatusStatic;
extern DWORD FileToSendCurrentSection;
extern DWORD FileTransferringMode;
extern HANDLE ProcessHeap;

DWORD WINAPI ThreadReceiver(LPVOID SocketPtr);