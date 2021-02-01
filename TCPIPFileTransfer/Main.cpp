#include <Winsock2.h>
#include <Windows.h>

#include "Logging.h"
#include "ServerInit.h"
#include "ClientInit.h"
#include "ThreadSender.h"
#include "ThreadReceiver.h"
#include "Shared.h"

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
LPWSTR FileToSendName = NULL;
LPWSTR FileToSendNoPathName = NULL;
DWORD FileToSendNameLen;
HANDLE FileToSend = NULL;
DWORD FileToSendSize;
DWORD FileToSendSectionsCount;
char *FileToSendDataBuf = NULL;
DWORD FileToSendCurrentSection = 0;
// Sender mode data

CONNECTIONPARAMS* ConnectParams;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WSADATA wsaData;

	int Result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (Result != 0)
	{
		MessageBoxW(NULL, L"WSAStartup failed", L"Error", MB_ICONERROR);
		return 2;
	}

	InitLoggingSystem();

	SocketBusyMutex = CreateMutexW(NULL, FALSE, NULL);
	ThreadSenderSemaphore = CreateSemaphoreW(NULL, 0, 1, NULL);

	LPCWSTR WinClass = L"FileTransfer";

	WNDCLASSW ws;

	ws.style = CS_HREDRAW;
	ws.lpfnWndProc = WndProc;
	ws.cbClsExtra = 0;
	ws.cbWndExtra = 0;
	ws.hInstance = hInstance;
	ws.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(32512));
	ws.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
	ws.hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
	ws.lpszMenuName = NULL;
	ws.lpszClassName = WinClass;
	RegisterClassW(&ws);

	MainWindow = CreateWindowExW(WS_EX_ACCEPTFILES, WinClass, L"TCP/IP File Transfer", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 460, 480, NULL, NULL, hInstance, NULL);
	IPEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"edit", L"", WS_CHILD | WS_VISIBLE | ES_LEFT, 50, 10, 150, 20, MainWindow, NULL, hInstance, NULL);
	PortEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"edit", L"", WS_CHILD | WS_VISIBLE | ES_LEFT, 50, 40, 150, 20, MainWindow, NULL, hInstance, NULL);
	StatusStatic = CreateWindowExW(WS_EX_TRANSPARENT, L"static", L"Not connected", WS_CHILD | WS_VISIBLE | ES_LEFT, 10, 70, 420, 20, MainWindow, NULL, hInstance, NULL);
	ConnectButton = CreateWindowExW(0, L"button", L"Connect", WS_CHILD | WS_VISIBLE | ES_LEFT, 230, 10, 200, 20, MainWindow, (HMENU)CONNECT_BUTTON_ID, hInstance, NULL);
	SwitchToServerModeButton = CreateWindowExW(0, L"button", L"Switch to server mode", WS_CHILD | WS_VISIBLE | ES_LEFT, 230, 40, 200, 20, MainWindow, (HMENU)SWITCH_TO_SERVER_MODE_BUTTON_ID, hInstance, NULL);
	FileNameEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"edit", L"", WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 10, 120, 420, 20, MainWindow, NULL, hInstance, NULL);

	LogEdit = CreateWindowExW(WS_EX_TRANSPARENT, L"edit", L"", WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 10, 180, 420, 250, MainWindow, NULL, hInstance, NULL);

	SendFileButton = CreateWindowExW(0, L"button", L"Send file", WS_CHILD | WS_VISIBLE | ES_LEFT, 10, 150, 200, 20, MainWindow, (HMENU)SEND_FILE_BUTTON_ID, hInstance, NULL);

	ShowWindow(MainWindow, SW_SHOW);
	MSG msg;
	while (GetMessageW(&msg, MainWindow, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WCHAR StrBuf[256];

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

		TextOutW(hdc, 10, 10, L"IP", 2);
		TextOutW(hdc, 10, 40, L"Port", 4);
		TextOutW(hdc, 10, 100, L"File name/path", 14);

		EndPaint(MainWindow, &ps);
	}
	break;
	case WM_DROPFILES:
	{
		DWORD NameSize = DragQueryFileW((HDROP)wParam, 0, NULL, 0);
		LPWSTR DroppedFileName = (LPWSTR)malloc((NameSize + 1) * sizeof(WCHAR));
		DragQueryFileW((HDROP)wParam, 0, (LPWSTR)DroppedFileName, NameSize + 1);
		SetWindowTextW(FileNameEdit, DroppedFileName);
		free(DroppedFileName);
	}
	break;
	case WM_COMMAND:
	{
		if (wParam == SWITCH_TO_SERVER_MODE_BUTTON_ID)
		{
			if (ProgramMode == SERVER_MODE)
			{
				MessageBoxW(MainWindow, L"You're already in server mode", L"Warning", MB_ICONWARNING);
				return 0;
			}

			if (ProgramMode == CLIENT_MODE)
			{
				MessageBoxW(MainWindow, L"You're in server mode.\r\nRestart program to switch to server mode.", L"Warning", MB_ICONWARNING);
				return 0;
			}

			ThreadsState = TS_ALIVE;
			CreateThread(NULL, 0, InitalizeServer, NULL, 0, NULL);
		}
		else if (wParam == CONNECT_BUTTON_ID)
		{
			if (ProgramMode == SERVER_MODE)
			{
				MessageBoxW(MainWindow, L"You're in server mode.\r\nTo connect to another server, restart program.", L"Warning", MB_ICONWARNING);
				return 0;
			}

			if (ProgramMode == CLIENT_MODE)
			{
				MessageBoxW(MainWindow, L"You're already in client mode", L"Warning", MB_ICONWARNING);
				return 0;
			}

			ThreadsState = TS_ALIVE;
			DWORD IPLen = GetWindowTextLengthW(IPEdit);
			if (IPLen == 0)
			{
				MessageBoxW(MainWindow, L"Enter IP to connect", L"Warning", MB_ICONWARNING);
				return 0;
			}
			DWORD PortLen = GetWindowTextLengthW(PortEdit);
			if (PortLen == 0)
			{
				MessageBoxW(MainWindow, L"Enter port to connect", L"Warning", MB_ICONWARNING);
				return 0;
			}

			LPWSTR PortText = (LPWSTR)malloc((PortLen + 1) * sizeof(WCHAR));
			LPWSTR IPText = (LPWSTR)malloc((IPLen + 1) * sizeof(WCHAR));

			GetWindowTextW(IPEdit, IPText, IPLen + 1);
			GetWindowTextW(PortEdit, PortText, PortLen + 1);

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
				MessageBoxW(MainWindow, L"You have to be connected to server or you be a server with connected client", L"Warning", MB_ICONWARNING);
				return 0;
			}

			if (FileTransferringMode != IDLE_MODE)
			{
				MessageBoxW(MainWindow, L"File is being sent at the moment.\r\nWait until this operation ends.", L"Warning", MB_ICONWARNING);
				return 0;
			}

			FileToSendNameLen = GetWindowTextLengthW(FileNameEdit);

			if (FileToSendNameLen == 0)
			{
				MessageBoxW(MainWindow, L"Enter file name", L"Warning", MB_ICONWARNING);
				return 0;
			}

			FileToSendName = (LPWSTR)malloc((FileToSendNameLen + 1) * sizeof(WCHAR));
			GetWindowTextW(FileNameEdit, FileToSendName, FileToSendNameLen + 1);

			FileToSend = CreateFileW(FileToSendName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

			if (FileToSend == INVALID_HANDLE_VALUE)
			{
				swprintf_s(StrBuf, 256, L"Can't open file, error %ld", GetLastError());
				MessageBoxW(MainWindow, StrBuf, L"Error", MB_ICONERROR);
				free(FileToSendName);
				return 0;
			}

			FileToSendSize = GetFileSize(FileToSend, NULL);

			if (FileToSendSize == 0)
			{
				MessageBoxW(MainWindow, L"This file can't be transferred as it's empty", L"Warning", MB_ICONWARNING);
				free(FileToSendName);
				CloseHandle(FileToSend);
				return 0;
			}

			ReleaseSemaphore(ThreadSenderSemaphore, 1, NULL);
		}
	}
	break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
}