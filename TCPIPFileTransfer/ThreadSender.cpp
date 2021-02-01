#include "ThreadSender.h"

DWORD WINAPI ThreadSender(LPVOID SocketPtr)
{
	SOCKET Socket = *(SOCKET*)SocketPtr;

	WCHAR StrBuf[256];

	while (TRUE)
	{
		WaitForSingleObject(ThreadSenderSemaphore, INFINITE);

		if (ThreadsState == TS_STOPPING)
		{
			return 0;
		}

		WaitForSingleObject(SocketBusyMutex, INFINITE);

		if (FileToSendName)
		{
			FileTransferringMode = SENDER_MODE;

			AddLogText(L"File transfer started\r\n");

			int NoPathNameLength;

			wchar_t* NameStartPtr = wcsrchr(FileToSendName, L'\\');
			if (!NameStartPtr) NameStartPtr = wcsrchr(FileToSendName, L'/');
			
			if (!NameStartPtr)
			{
				NameStartPtr = FileToSendName;
				NoPathNameLength = FileToSendNameLen;
			}
			else
			{
				NameStartPtr++;
				NoPathNameLength = FileToSendNameLen - (NameStartPtr - FileToSendName);
			}

			FileToSendNoPathName = NameStartPtr;

			DWORD* BufferToSend = (DWORD*)malloc(sizeof(DWORD) * 2 + (NoPathNameLength + 1) * sizeof(WCHAR));

			BufferToSend[0] = CMD_RECEIVER_FILENAME;
			BufferToSend[1] = NoPathNameLength;

			swprintf_s(StrBuf, 256, L"File \"%s\", sending file name\r\n", NameStartPtr);
			AddLogText(StrBuf);

			memcpy(BufferToSend + 2, NameStartPtr, (NoPathNameLength + 1) * sizeof(WCHAR));

			send(Socket, (const char*)BufferToSend, sizeof(DWORD) * 2 + (NoPathNameLength + 1) * sizeof(WCHAR), 0);

			free(BufferToSend);
		}

		ReleaseMutex(SocketBusyMutex);
	}

	return 0;
}