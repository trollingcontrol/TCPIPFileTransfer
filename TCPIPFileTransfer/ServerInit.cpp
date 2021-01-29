#include "ServerInit.h"

// Another thread [2]
// Server mode
// Initalization as server & Waiting for connection
// After disconnecting waits for new connection

const char* ListeningPort = "80";

void DisconnectServer(SOCKET ListenSocket)
{
	shutdown(ListenSocket, SD_BOTH);
	closesocket(ListenSocket);
	SetWindowTextA(StatusStatic, "Not connected");
	AddLogText("Connection closed\r\n");
}

DWORD WINAPI InitalizeServer(LPVOID P)
{
	ProgramMode = SERVER_MODE;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	addrinfo* result, * ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	int Result = getaddrinfo(NULL, ListeningPort, &hints, &result);
	if (Result != 0)
	{
		AddLogText("Error: getaddrinfo failed\r\n");
		return 3;
	}

	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET)
	{
		AddLogText("Error: socket failed\r\n");
		freeaddrinfo(result);
		return 4;
	}

	if (bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
	{
		AddLogText("Error: bind failed\r\n");
		freeaddrinfo(result);
		closesocket(ListenSocket);
		return 5;
	}

	freeaddrinfo(result);

	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		AddLogText("Error: listen failed\r\n");
		DisconnectServer(ListenSocket);
		return 6;
	}

	while (TRUE)
	{
		char ConnectingMessage[64];
		sprintf_s(ConnectingMessage, 64, "Waiting for connection, port: %s", ListeningPort);
		SetWindowTextA(StatusStatic, ConnectingMessage);

		AddLogText("Waiting for connection\r\n");

		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET)
		{
			AddLogText("Error: accept failed\r\n");
			DisconnectServer(ClientSocket);
			closesocket(ListenSocket);
			return 7;
		}

		SetWindowTextA(StatusStatic, "Connected");
		AddLogText("Connection established\r\n");

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
					AddLogText("Error sending handshake response\r\n");
					DisconnectServer(ClientSocket);
				}
			}
			else
			{
				AddLogText("Wrong handshake from client\r\n");
				DisconnectServer(ClientSocket);
			}
		}
		else
		{
			AddLogText("Error receiving handshake\r\n");
			DisconnectServer(ClientSocket);
		}
	}

	return 0;
}