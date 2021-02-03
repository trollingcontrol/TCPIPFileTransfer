#pragma once

#include <Windows.h>
#include "Shared.h"
#include "Logging.h"

extern HANDLE SocketBusyMutex;
extern HANDLE ThreadSenderSemaphore;
extern DWORD ThreadsState;

extern LPWSTR FileToSendName;
extern LPWSTR FileToSendNoPathName;
extern DWORD FileToSendNameLen;
extern HANDLE FileToSend;
extern DWORD FileTransferringMode;
extern HANDLE ProcessHeap;

DWORD WINAPI ThreadSender(LPVOID SocketPtr);