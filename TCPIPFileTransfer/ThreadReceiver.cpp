#include "ThreadReceiver.h"

// Receiver mode data
DWORD Command = 0;
LPWSTR ReceivedFileName = NULL;
char *ReceivedFileDataBuf = NULL;
HANDLE ReceivedFile = NULL;
DWORD ReceivedFileSectionsCount;
DWORD ResponseBuffer[2] = { 0 };
// Receiver mode data

WCHAR StrBuf[256];

void WriteFileSize(LPWSTR Buffer, LONGLONG FileSize)
{
	if (FileSize < 1024) swprintf_s(Buffer, 64, L"%llu B", FileSize);
	else if (FileSize < 1048576) swprintf_s(Buffer, 64, L"%.2lf KB", FileSize / 1024.0);
	else if (FileSize < 1073741824) swprintf_s(Buffer, 64, L"%.2lf MB", FileSize / 1048576.0);
	else swprintf_s(Buffer, 64, L"%.2lf GB", FileSize / 1073741824.0);

	AddLogText(Buffer);
}

void CleanupReceiverSession()
{
	if (ReceivedFile != NULL && ReceivedFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(ReceivedFile);
		ReceivedFile = NULL;
	}
	else if (ReceivedFile == INVALID_HANDLE_VALUE) ReceivedFile = NULL;

	if (ReceivedFileName)
	{
		HeapFree(ProcessHeap, 0, ReceivedFileName);
		ReceivedFileName = NULL;
	}

	if (ReceivedFileDataBuf)
	{
		HeapFree(ProcessHeap, 0, ReceivedFileDataBuf);
		ReceivedFileDataBuf = NULL;
	}

	if (FileToSendDataBuf)
	{
		HeapFree(ProcessHeap, 0, FileToSendDataBuf);
		FileToSendDataBuf = NULL;
	}

	FileTransferringMode = IDLE_MODE;
	Command = 0;

	AddLogText(L"File transfer ended\r\n");
}

void CleanupSenderSession()
{
	if (FileToSendName)
	{
		HeapFree(ProcessHeap, 0, FileToSendName);
		FileToSendName = NULL;
	}

	if (FileToSendDataBuf)
	{
		HeapFree(ProcessHeap, 0, FileToSendDataBuf);
		FileToSendDataBuf = NULL;
	}

	if (FileToSend != NULL && FileToSend != INVALID_HANDLE_VALUE)
	{
		CloseHandle(FileToSend);
		FileToSend = NULL;
	}
	else if (FileToSend == INVALID_HANDLE_VALUE) FileToSend = NULL;

	FileToSendNoPathName = NULL;
	FileToSendCurrentSection = 0;
	FileTransferringMode = IDLE_MODE;

	AddLogText(L"File transfer ended\r\n");
}

BOOL SendNextFileSection(SOCKET Socket)
{
	swprintf_s(StrBuf, 256, L"Sending file (%d/%d sections)", FileToSendCurrentSection + 1, FileToSendSectionsCount);
	SetWindowTextW(StatusStatic, StrBuf);

	DWORD ReadBytes;

	if (ReadFile(FileToSend, FileToSendDataBuf, FILE_SECTION_SIZE, &ReadBytes, NULL))
	{
		FileToSendCurrentSection++;
		
		DWORD CommandsBuf[3] = { CMD_RECEIVER_FILEDATA, ReadBytes, FileToSendCurrentSection };

		send(Socket, (const char*)CommandsBuf, 12, 0);
		send(Socket, FileToSendDataBuf, ReadBytes, 0);

		return TRUE;
	}
	else return FALSE;
}

DWORD WINAPI ThreadReceiver(LPVOID SocketPtr)
{
	SOCKET Socket = *(SOCKET*)SocketPtr;

	AddLogText(L"Handshake performed successfully\r\nReady to transfer files\r\n");

	while (TRUE)
	{
		int Result = recv(Socket, (char*)&Command, 4, 0);

		WaitForSingleObject(SocketBusyMutex, INFINITE);

		if (Result > 0)
		{
			// Receiver side
			if (Command == CMD_RECEIVER_FILENAME)
			{
				FileTransferringMode = RECEIVER_MODE;

				AddLogText(L"File transfer started by connected program\r\n");

				ResponseBuffer[0] = CMD_SENDER_FILENAME;

				DWORD NoPathNameLength;

				recv(Socket, (char*)&NoPathNameLength, sizeof(DWORD), 0);

				ReceivedFileName = (LPWSTR)HeapAlloc(ProcessHeap, 0, (NoPathNameLength + 1) * sizeof(WCHAR));
				if (!ReceivedFileName)
				{
					FlushNetworkBuffer(Socket, (NoPathNameLength + 1) * sizeof(WCHAR));
					ResponseBuffer[1] = FALSE;
					send(Socket, (const char*)ResponseBuffer, sizeof(DWORD) * 2, 0);
					CleanupReceiverSession();
					ShowWinFuncError(NULL, L"HeapAlloc");
					continue;
				}

				recv(Socket, (char *)ReceivedFileName, (NoPathNameLength + 1) * sizeof(WCHAR), 0);

				swprintf_s(StrBuf, 256, L"Name of received file: %s\r\n", ReceivedFileName);
				AddLogText(StrBuf);

				ReceivedFile = CreateFileW(ReceivedFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

				if (ReceivedFile != INVALID_HANDLE_VALUE)
				{
					ResponseBuffer[1] = TRUE;

					swprintf_s(StrBuf, 256, L"File %s created\r\n", ReceivedFileName);
					AddLogText(StrBuf);
				}
				else
				{
					swprintf_s(StrBuf, 256, L"Failed to create file %s, error %ld\r\n", ReceivedFileName, GetLastError());
					AddLogText(StrBuf);

					ResponseBuffer[1] = FALSE;

					CleanupReceiverSession();
				}

				send(Socket, (const char*)ResponseBuffer, sizeof(DWORD) * 2, 0);
			}

			// Sender side
			else if (Command == CMD_SENDER_FILENAME)
			{
				DWORD ResponseFlag;

				recv(Socket, (char*)&ResponseFlag, sizeof(DWORD), 0);

				if (ResponseFlag == TRUE)
				{
					AddLogText(L"File has successfully been created\r\n");

					FileToSendSectionsCount = FileToSendSize.QuadPart / FILE_SECTION_SIZE + 1;

					AddLogText(L"File size ");
					WriteFileSize(StrBuf, FileToSendSize.QuadPart);
					swprintf_s(StrBuf, 256, L" (%d sections), sending meta data\r\n", FileToSendSectionsCount);
					AddLogText(StrBuf);

					DWORD FileMetaToSend[4] = { CMD_RECEIVER_FILEMETA };
					*(LONGLONG *)&FileMetaToSend[1] = FileToSendSize.QuadPart;
					FileMetaToSend[3] = FileToSendSectionsCount;

					send(Socket, (const char*)FileMetaToSend, sizeof(FileMetaToSend), 0);
				}
				else
				{
					AddLogText(L"A receiver side error\r\n");

					CleanupSenderSession();
				}
			}

			// Receiver side
			else if (Command == CMD_RECEIVER_FILEMETA)
			{
				DWORD FileMetaBuf[3];

				recv(Socket, (char*)FileMetaBuf, sizeof(FileMetaBuf), 0);

				AddLogText(L"File meta data got, file size ");
				WriteFileSize(StrBuf, *(LONGLONG *)FileMetaBuf);
				ReceivedFileSectionsCount = FileMetaBuf[2];
				swprintf_s(StrBuf, 256, L" (%d sections)\r\n", FileMetaBuf[2]);
				AddLogText(StrBuf);

				FileMetaBuf[0] = CMD_SENDER_FILEMETA;

				ReceivedFileDataBuf = (char*)HeapAlloc(ProcessHeap, 0, FILE_SECTION_SIZE);
				if (ReceivedFileDataBuf) FileMetaBuf[1] = TRUE;
				else
				{
					FileMetaBuf[1] = FALSE;
					CleanupReceiverSession();
				}

				send(Socket, (const char*)FileMetaBuf, sizeof(DWORD) * 2, 0);

				if (!ReceivedFileDataBuf) ShowWinFuncError(NULL, L"HeapAlloc");
			}

			// Sender side
			else if (Command == CMD_SENDER_FILEMETA)
			{
				DWORD ResponseFlag;

				recv(Socket, (char*)&ResponseFlag, 4, 0);

				if (ResponseFlag == TRUE)
				{
					FileToSendDataBuf = (char*)HeapAlloc(ProcessHeap, 0, FILE_SECTION_SIZE);
					if (!FileToSendDataBuf)
					{
						DWORD ErrorBlock[2] = { CMD_RECEIVER_FILEDATA, 0 };
						send(Socket, (const char*)ErrorBlock, sizeof(ErrorBlock), 0);
						ShowWinFuncError(NULL, L"HeapAlloc");
					}
					else SendNextFileSection(Socket);
				}
				else
				{
					AddLogText(L"A receiver side error\r\n");
					CleanupSenderSession();
				}
			}

			// Receiver side
			else if (Command == CMD_RECEIVER_FILEDATA)
			{
				DWORD CommandsBuf[2];

				recv(Socket, (char*)CommandsBuf, sizeof(DWORD), 0);

				if (CommandsBuf[0] == 0)
				{
					AddLogText(L"A client error\r\n");
					CleanupReceiverSession();
					CommandsBuf[0] = CMD_SENDER_FILEDATA;
					CommandsBuf[1] = FALSE;
					send(Socket, (const char*)CommandsBuf, sizeof(CommandsBuf), 0);

					continue;
				}

				recv(Socket, (char*)&CommandsBuf[1], sizeof(DWORD), 0);
				recv(Socket, ReceivedFileDataBuf, CommandsBuf[0], 0);

				swprintf_s(StrBuf, 256, L"Receiving file (%d/%d sections)", CommandsBuf[1], ReceivedFileSectionsCount);
				SetWindowTextW(StatusStatic, StrBuf);

				DWORD Written = 0;

				BOOL WriteResult = WriteFile(ReceivedFile, ReceivedFileDataBuf, CommandsBuf[0], &Written, NULL);

				if (WriteResult && Written == CommandsBuf[0]) CommandsBuf[1] = TRUE;
				else
				{
					swprintf_s(StrBuf, 256, L"Failed to write section, WriteResult=%d, Written=%d/%d, ErrorCode=%d\r\n", WriteResult, Written, CommandsBuf[0], GetLastError());
					AddLogText(StrBuf);
					CommandsBuf[1] = FALSE;

					if (ProgramMode == SERVER_MODE) SetWindowTextW(StatusStatic, L"Connected");
					else
					{
						swprintf_s(StrBuf, 256, L"Connected to %s:%s", ConnectParams->IPText, ConnectParams->PortText);
						SetWindowTextW(StatusStatic, StrBuf);
					}

					CleanupReceiverSession();
				}

				CommandsBuf[0] = CMD_SENDER_FILEDATA;

				send(Socket, (const char*)CommandsBuf, sizeof(CommandsBuf), 0);
			}

			// Sender side
			else if (Command == CMD_SENDER_FILEDATA)
			{
				DWORD ResponseFlag;

				recv(Socket, (char*)&ResponseFlag, sizeof(DWORD), 0);

				if (ResponseFlag == TRUE)
				{
					if (FileToSendCurrentSection < FileToSendSectionsCount) SendNextFileSection(Socket);
					else
					{
						AddLogText(L"All sections has been sent, closing file\r\n");

						DWORD CloseFileCommand = CMD_RECEIVER_CLOSEFILE;
						send(Socket, (const char*)&CloseFileCommand, sizeof(DWORD), 0);
					}
				}
				else
				{
					AddLogText(L"A receiver side error\r\n");

					if (ProgramMode == SERVER_MODE) SetWindowTextW(StatusStatic, L"Connected");
					else
					{
						swprintf_s(StrBuf, 256, L"Connected to %s:%s", ConnectParams->IPText, ConnectParams->PortText);
						SetWindowTextW(StatusStatic, StrBuf);
					}

					CleanupSenderSession();
				}
			}

			// Receiver side
			else if (Command == CMD_RECEIVER_CLOSEFILE)
			{
				swprintf_s(StrBuf, 256, L"All sections of file %s has been received, closing file\r\n", ReceivedFileName);
				AddLogText(StrBuf);

				DWORD ResponseBuf[2] = { CMD_SENDER_CLOSEFILE, CloseHandle(ReceivedFile) };

				ReceivedFile = NULL;

				if (ResponseBuf[1]) AddLogText(L"File has successfully been received\r\n");
				else
				{
					swprintf_s(StrBuf, 256, L"Error %ld closing file\r\n", GetLastError());
					AddLogText(StrBuf);
				}

				if (ProgramMode == SERVER_MODE) SetWindowTextW(StatusStatic, L"Connected");
				else
				{
					swprintf_s(StrBuf, 256, L"Connected to %s:%s", ConnectParams->IPText, ConnectParams->PortText);
					SetWindowTextW(StatusStatic, StrBuf);
				}

				CleanupReceiverSession();

				send(Socket, (const char*)ResponseBuf, sizeof(ResponseBuf), 0);
			}

			// Sender side
			else if (Command == CMD_SENDER_CLOSEFILE)
			{
				DWORD ResponseFlag;

				recv(Socket, (char*)&ResponseFlag, sizeof(DWORD), 0);

				swprintf_s(StrBuf, 256, L"File %s has successfully been sent\r\n", FileToSendNoPathName);
				AddLogText(StrBuf);

				if (ProgramMode == SERVER_MODE) SetWindowTextW(StatusStatic, L"Connected");
				else
				{
					swprintf_s(StrBuf, 256, L"Connected to %s:%s", ConnectParams->IPText, ConnectParams->PortText);
					SetWindowTextW(StatusStatic, StrBuf);
				}

				CleanupSenderSession();
			}
		}
		else
		{
			if (ReceivedFileName) CleanupReceiverSession();
			if (FileToSendName) CleanupSenderSession();

			ThreadsState = TS_STOPPING;
			ReleaseMutex(SocketBusyMutex);
			ReleaseSemaphore(ThreadSenderSemaphore, 1, NULL);
			return 0;
		}

		ReleaseMutex(SocketBusyMutex);
	}

	return 0;
}