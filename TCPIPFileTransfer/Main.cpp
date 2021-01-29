#include <Winsock2.h>
#include <Windows.h>

#include "Logging.h"
#include "ServerInit.h"
#include "ClientInit.h"
#include "ThreadSender.h"
#include "ThreadReceiver.h"
#include "Shared.h"

// TrollingCont. 23.01.2021

#pragma comment(lib, "Ws2_32.lib")

#define CONNECT_BUTTON_ID 100
#define SWITCH_TO_SERVER_MODE_BUTTON_ID 102
#define SEND_FILE_BUTTON_ID 103

// GUI Elements
HWND MainWindow;
HWND IPEdit;
HWND PortEdit;
HWND StatusStatic;
HWND ConnectButton;
HWND SwitchToServerModeButton;

HWND FileNameEdit;

HWND LogEdit;

HWND SendFileButton;
// GUI Elements

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// IDLE_MODE - not connected
// CLIENT_MODE - connected as client
// SERVER_MODE - connected as server
DWORD ProgramMode;

// IDLE_MODE - file is not being transferred at the time
// RECEIVER_MODE - program is receiving file
// SENDER_MODE - program is sending file
DWORD FileTransferringMode;

HANDLE SocketBusyMutex;
HANDLE ThreadSenderSemaphore;

DWORD ThreadsState = TS_ALIVE;

// Sender mode data
char* FileToSendName = NULL;
char* FileToSendNoPathName = NULL;
DWORD FileToSendNameLen;
HANDLE FileToSend = NULL;
DWORD FileToSendSize;
DWORD FileToSendSectionsCount;
char* FileToSendDataBuf = NULL;
DWORD FileToSendCurrentSection = 0;
// Sender mode data

CONNECTIONPARAMS* ConnectParams;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WSADATA wsaData;

	int Result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (Result != 0)
	{
		MessageBoxA(NULL, "WSAStartup failed", "Error", MB_ICONERROR);
		return 2;
	}

	InitLoggingSystem();

	SocketBusyMutex = CreateMutexA(NULL, FALSE, NULL);
	ThreadSenderSemaphore = CreateSemaphoreA(NULL, 0, 1, NULL);

	LPCSTR WinClass = "FileTransfer";

	WNDCLASSA ws;

	ws.style = CS_HREDRAW;
	ws.lpfnWndProc = WndProc;
	ws.cbClsExtra = 0;
	ws.cbWndExtra = 0;
	ws.hInstance = hInstance;
	ws.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(32512));
	ws.hCursor = LoadCursorA(NULL, MAKEINTRESOURCEA(32512));
	ws.hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
	ws.lpszMenuName = NULL;
	ws.lpszClassName = WinClass;
	RegisterClassA(&ws);

	MainWindow = CreateWindowExA(0, WinClass, "TCP/IP File Transfer by TrollingCont [VS '19]", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 460, 480, NULL, NULL, hInstance, NULL);
	IPEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "edit", "", WS_CHILD | WS_VISIBLE | ES_LEFT, 50, 10, 150, 20, MainWindow, NULL, hInstance, NULL);
	PortEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "edit", "", WS_CHILD | WS_VISIBLE | ES_LEFT, 50, 40, 150, 20, MainWindow, NULL, hInstance, NULL);
	StatusStatic = CreateWindowExA(WS_EX_TRANSPARENT, "static", "Not connected", WS_CHILD | WS_VISIBLE | ES_LEFT, 10, 70, 420, 20, MainWindow, NULL, hInstance, NULL);
	ConnectButton = CreateWindowExA(0, "button", "Connect", WS_CHILD | WS_VISIBLE | ES_LEFT, 230, 10, 200, 20, MainWindow, (HMENU)CONNECT_BUTTON_ID, hInstance, NULL);
	SwitchToServerModeButton = CreateWindowExA(0, "button", "Switch to server mode", WS_CHILD | WS_VISIBLE | ES_LEFT, 230, 40, 200, 20, MainWindow, (HMENU)SWITCH_TO_SERVER_MODE_BUTTON_ID, hInstance, NULL);
	FileNameEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "edit", "", WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 10, 120, 420, 20, MainWindow, NULL, hInstance, NULL);

	LogEdit = CreateWindowExA(WS_EX_TRANSPARENT, "edit", "", WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 10, 180, 420, 250, MainWindow, NULL, hInstance, NULL);

	SendFileButton = CreateWindowExA(0, "button", "Send file", WS_CHILD | WS_VISIBLE | ES_LEFT, 10, 150, 200, 20, MainWindow, (HMENU)SEND_FILE_BUTTON_ID, hInstance, NULL);

	ShowWindow(MainWindow, SW_SHOW);
	MSG msg;
	while (GetMessageA(&msg, MainWindow, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	char StrBuf[256];

	switch (message)
	{
	case WM_DESTROY:
	case WM_CLOSE:
		WSACleanup();
		ExitProcess(0);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(MainWindow, &ps);

		TextOutA(hdc, 10, 10, "IP", 2);
		TextOutA(hdc, 10, 40, "Port", 4);
		TextOutA(hdc, 10, 100, "File name/path", 14);

		EndPaint(MainWindow, &ps);
	}
	break;
	case WM_COMMAND:
	{
		if (wParam == SWITCH_TO_SERVER_MODE_BUTTON_ID)
		{
			if (ProgramMode == SERVER_MODE)
			{
				MessageBoxA(MainWindow, "You're already in server mode", "Warning", MB_ICONWARNING);
				return 0;
			}

			if (ProgramMode == CLIENT_MODE)
			{
				MessageBoxA(MainWindow, "You're in server mode.\r\nRestart program to switch to server mode.", "Warning", MB_ICONWARNING);
				return 0;
			}

			ThreadsState = TS_ALIVE;
			CreateThread(NULL, 0, InitalizeServer, NULL, 0, NULL);
		}
		else if (wParam == CONNECT_BUTTON_ID)
		{
			if (ProgramMode == SERVER_MODE)
			{
				MessageBoxA(MainWindow, "You're in server mode.\r\nTo connect to another server, restart program.", "Warning", MB_ICONWARNING);
				return 0;
			}

			if (ProgramMode == CLIENT_MODE)
			{
				MessageBoxA(MainWindow, "You're already in client mode", "Warning", MB_ICONWARNING);
				return 0;
			}

			ThreadsState = TS_ALIVE;
			DWORD IPLen = GetWindowTextLengthA(IPEdit);
			if (IPLen == 0)
			{
				MessageBoxA(MainWindow, "Enter IP to connect", "Warning", MB_ICONWARNING);
				return 0;
			}
			DWORD PortLen = GetWindowTextLengthA(PortEdit);
			if (PortLen == 0)
			{
				MessageBoxA(MainWindow, "Enter port to connect", "Warning", MB_ICONWARNING);
				return 0;
			}

			char* PortText = (char*)malloc(PortLen + 1);
			char* IPText = (char*)malloc(IPLen + 1);

			GetWindowTextA(IPEdit, IPText, IPLen + 1);
			GetWindowTextA(PortEdit, PortText, PortLen + 1);

			ConnectParams = (CONNECTIONPARAMS*)malloc(sizeof(CONNECTIONPARAMS));
			ConnectParams->IPText = IPText;
			ConnectParams->PortText = PortText;

			ThreadsState = TS_ALIVE;
			CreateThread(NULL, 0, InitalizeClientAndConnect, ConnectParams, 0, NULL);
		}
		else if (wParam == SEND_FILE_BUTTON_ID)
		{
			if (ProgramMode == IDLE_MODE)
			{
				MessageBoxA(MainWindow, "You have to be connected to server or you be a server with connected client", "Warning", MB_ICONWARNING);
				return 0;
			}

			if (FileTransferringMode != IDLE_MODE)
			{
				MessageBoxA(MainWindow, "File is being sent at the moment.\r\nWait until this operation ends.", "Warning", MB_ICONWARNING);
				return 0;
			}

			FileToSendNameLen = GetWindowTextLengthA(FileNameEdit);

			if (FileToSendNameLen == 0)
			{
				MessageBoxA(MainWindow, "Enter file name", "Warning", MB_ICONWARNING);
				return 0;
			}

			FileToSendName = (char*)malloc(FileToSendNameLen + 1);
			GetWindowTextA(FileNameEdit, FileToSendName, FileToSendNameLen + 1);

			FileToSend = CreateFileA(FileToSendName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

			if (FileToSend == INVALID_HANDLE_VALUE)
			{
				sprintf_s(StrBuf, 256, "Can't open file, error %ld", GetLastError());
				MessageBoxA(MainWindow, StrBuf, "Error", MB_ICONERROR);
				free(FileToSendName);
				return 0;
			}

			FileToSendSize = GetFileSize(FileToSend, NULL);

			if (FileToSendSize == 0)
			{
				MessageBoxA(MainWindow, "This file can't be transferred as it's empty", "Warning", MB_ICONWARNING);
				free(FileToSendName);
				CloseHandle(FileToSend);
				return 0;
			}

			ReleaseSemaphore(ThreadSenderSemaphore, 1, NULL);
		}
	}
	break;
	default:
		return DefWindowProcA(hWnd, message, wParam, lParam);
	}
}