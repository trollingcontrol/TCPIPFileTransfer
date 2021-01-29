#pragma once

#include <Winsock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include "Logging.h"
#include "Shared.h"
#include "ThreadReceiver.h"
#include "ThreadSender.h"

extern HWND StatusStatic;
extern DWORD ProgramMode;

DWORD WINAPI InitalizeClientAndConnect(LPVOID ConnectionParams);