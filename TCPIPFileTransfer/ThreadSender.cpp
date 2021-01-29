#include "ThreadSender.h"

DWORD WINAPI ThreadSender(LPVOID SocketPtr)
{
	SOCKET Socket = *(SOCKET*)SocketPtr;

	char StrBuf[256];

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

			AddLogText("File transfer started\r\n");

			int NoPathNameLength;

			char* NameStartPtr = strrchr(FileToSendName, '\\');
			if (!NameStartPtr) NameStartPtr = strrchr(FileToSendName, '/');
			
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

			DWORD* BufferToSend = (DWORD*)malloc(4 + 4 + NoPathNameLength + 1);

			BufferToSend[0] = CMD_RECEIVER_FILENAME;
			BufferToSend[1] = NoPathNameLength;

			sprintf_s(StrBuf, "File \"%s\", sending file name\r\n", NameStartPtr);
			AddLogText(StrBuf);

			memcpy(BufferToSend + 2, NameStartPtr, NoPathNameLength + 1);

			send(Socket, (const char*)BufferToSend, 4 + 4 + NoPathNameLength + 1, 0);

			free(BufferToSend);
		}

		ReleaseMutex(SocketBusyMutex);
	}

	return 0;
}