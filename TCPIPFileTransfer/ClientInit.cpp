#include "ClientInit.h"

void DisconnectClient(SOCKET ConnectSocket)
{
	closesocket(ConnectSocket);
	SetWindowTextA(StatusStatic, "Not connected");
	AddLogText("Connection closed\r\n");
	ProgramMode = IDLE_MODE;
}

DWORD WINAPI InitalizeClientAndConnect(LPVOID ConnectionParams)
{
	char StrBuf[256];

	ProgramMode = CLIENT_MODE;

	SOCKET ConnectSocket = INVALID_SOCKET;

	addrinfo* result = NULL, * ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int Result = getaddrinfo(((CONNECTIONPARAMS*)ConnectionParams)->IPText, ((CONNECTIONPARAMS*)ConnectionParams)->PortText, &hints, &result);
	if (Result)
	{
		AddLogText("Error: getaddrinfo failed\r\n");
		ProgramMode = IDLE_MODE;
		return 1;
	}

	for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET)
		{
			AddLogText("Error: socket failed\r\n");
			ProgramMode = IDLE_MODE;
			return 2;
		}

		Result = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (Result == SOCKET_ERROR)
		{
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET)
	{
		AddLogText("Failed to connect to server\r\n");
		ProgramMode = IDLE_MODE;
		return 3;
	}

	AddLogText("Connection established, performing handshake\r\n");

	sprintf_s(StrBuf, 256, "Connected to %s:%s", ((CONNECTIONPARAMS*)ConnectionParams)->IPText, ((CONNECTIONPARAMS*)ConnectionParams)->PortText);
	SetWindowTextA(StatusStatic, StrBuf);

	sprintf_s(StrBuf, 256, "Connected to %s:%s\r\n", ((CONNECTIONPARAMS*)ConnectionParams)->IPText, ((CONNECTIONPARAMS*)ConnectionParams)->PortText);
	AddLogText(StrBuf);

	DWORD CommandsBuf[2] = { CMD_HANDSHAKE };

	if (send(ConnectSocket, (const char*)CommandsBuf, 4, 0) == SOCKET_ERROR)
	{
		AddLogText("Failed to send handshake\r\n");
		DisconnectClient(ConnectSocket);
		return 4;
	}

	Result = recv(ConnectSocket, (char*)CommandsBuf, 8, 0);

	if (Result > 0)
	{
		if (CommandsBuf[0] == CMD_HANDSHAKE && CommandsBuf[1] == TRUE)
		{
			HANDLE DataExchangeThreads[] = {
				CreateThread(NULL, 0, ThreadReceiver, &ConnectSocket, 0, NULL),
				CreateThread(NULL, 0, ThreadSender, &ConnectSocket, 0, NULL)
			};

			WaitForMultipleObjects(2, DataExchangeThreads, TRUE, INFINITE);
			DisconnectClient(ConnectSocket);

			free(((CONNECTIONPARAMS*)ConnectionParams)->IPText);
			free(((CONNECTIONPARAMS*)ConnectionParams)->PortText);
			free(ConnectionParams);
		}
		else
		{
			AddLogText("Error: wrong answer from server\r\n");
			DisconnectClient(ConnectSocket);
		}
	}
	else
	{
		AddLogText("Error: failed to receive answer from server\r\n");
		DisconnectClient(ConnectSocket);
	}

	return 0;
}