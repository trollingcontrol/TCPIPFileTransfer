#pragma once

#include <Windows.h>
#include "Shared.h"
#include "Logging.h"

extern HANDLE SocketBusyMutex;
extern HANDLE ThreadSenderSemaphore;
extern DWORD ThreadsState;

extern char* FileToSendName;
extern char* FileToSendNoPathName;
extern DWORD FileToSendNameLen;
extern HANDLE FileToSend;
extern DWORD FileTransferringMode;

DWORD WINAPI ThreadSender(LPVOID SocketPtr);