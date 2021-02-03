#include "ServerInit.h"

// Another thread [2]
// Server mode
// Initalization as server & Waiting for connection
// After disconnecting waits for new connection

LPCWSTR ListeningPort = L"80";

void DisconnectServer(SOCKET ListenSocket)
{
	shutdown(ListenSocket, SD_BOTH);
	closesocket(ListenSocket);
	SetWindowTextW(StatusStatic, L"Not connected");
	AddLogText(L"Connection closed\r\n");
}

DWORD WINAPI InitalizeServer(LPVOID P)
{
	WCHAR StrBuf[256];

	ProgramMode = SERVER_MODE;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	ADDRINFOW* result, * ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	int Result = GetAddrInfoW(NULL, ListeningPort, &hints, &result);
	if (Result != 0)
	{
		swprintf_s(StrBuf, 256, L"GetAddrInfoW failed, WSA error %ld\r\n", WSAGetLastError());
		AddLogText(StrBuf);
		ProgramMode = IDLE_MODE;
		return 3;
	}

	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET)
	{
		swprintf_s(StrBuf, 256, L"socket failed, WSA error %ld\r\n", WSAGetLastError());
		AddLogText(StrBuf);
		FreeAddrInfoW(result);
		ProgramMode = IDLE_MODE;
		return 4;
	}

	if (bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
	{
		swprintf_s(StrBuf, 256, L"bind failed, WSA error %ld\r\n", WSAGetLastError());
		AddLogText(StrBuf);
		FreeAddrInfoW(result);
		closesocket(ListenSocket);
		ProgramMode = IDLE_MODE;
		return 5;
	}

	FreeAddrInfoW(result);

	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		swprintf_s(StrBuf, 256, L"listen failed, WSA error %ld\r\n", WSAGetLastError());
		AddLogText(StrBuf);
		DisconnectServer(ListenSocket);
		return 6;
	}

	while (TRUE)
	{
		swprintf_s(StrBuf, 256, L"Waiting for connection, port: %s", ListeningPort);
		SetWindowTextW(StatusStatic, StrBuf);

		AddLogText(L"Waiting for connection\r\n");

		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET)
		{
			swprintf_s(StrBuf, 256, L"accept failed, WSA error %ld\r\n", WSAGetLastError());
			AddLogText(StrBuf);
			DisconnectServer(ClientSocket);
			closesocket(ListenSocket);
			return 7;
		}

		SetWindowTextW(StatusStatic, L"Connected");
		AddLogText(L"Connection established\r\n");

		// Handshake
		DWORD TmpBuf[2];

		Result = recv(ClientSocket, (char*)TmpBuf, 4, 0);

		if (Result > 0)
		{
			if (TmpBuf[0] == CMD_HANDSHAKE)
			{
				TmpBuf[1] = TRUE;
				Result = send(ClientSocket, (const char*)TmpBuf, 8, 0);
				if (Result != SOCKET_ERROR)
				{
					HANDLE DataExchangeThreads[] = {
						CreateThread(NULL, 0, ThreadReceiver, &ClientSocket, 0, NULL),
						CreateThread(NULL, 0, ThreadSender, &ClientSocket, 0, NULL)
					};

					WaitForMultipleObjects(2, DataExchangeThreads, TRUE, INFINITE);
					DisconnectServer(ClientSocket);
				}
				else
				{
					swprintf_s(StrBuf, 256, L"Error sending handshake response (send WSA error %ld)\r\n", WSAGetLastError());
					AddLogText(StrBuf);
					DisconnectServer(ClientSocket);
				}
			}
			else if (TmpBuf[0] == CMD_OLD_HANDSHAKE)
			{
				AddLogText(L"Unable to establish connection\r\nVersion of client is older and incompatiable\r\n");
				DisconnectServer(ClientSocket);
			}
			else
			{
				AddLogText(L"Wrong handshake from client\r\n");
				DisconnectServer(ClientSocket);
			}
		}
		else
		{
			swprintf_s(StrBuf, 256, L"Error receiving handshake (recv WSA error %ld)\r\n", WSAGetLastError());
			AddLogText(StrBuf);
			DisconnectServer(ClientSocket);
		}
	}

	return 0;
}