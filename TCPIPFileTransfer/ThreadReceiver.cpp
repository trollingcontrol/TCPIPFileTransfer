#include "ThreadReceiver.h"

// Receiver mode data
DWORD Command = 0;
char* ReceivedFileName = NULL;
char* ReceivedFileDataBuf = NULL;
HANDLE ReceivedFile = NULL;
DWORD ReceivedFileSectionsCount;
DWORD ResponseBuffer[2] = { 0 };
// Receiver mode data

char StrBuf[256];

void WriteFileSize(char *Buffer, DWORD FileSize)
{
	if (FileSize < 1024) sprintf_s(Buffer, 64, "%d B", FileSize);
	else if (FileSize < 1048576) sprintf_s(Buffer, 64, "%.2lf KB", FileSize / 1024.0);
	else if (FileSize < 1073741824) sprintf_s(Buffer, 64, "%.2lf MB", FileSize / 1048576.0);
	else sprintf_s(Buffer, 64, "%.2lf GB", FileSize / 1073741824.0);

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
		free(ReceivedFileName);
		ReceivedFileName = NULL;
	}

	if (ReceivedFileDataBuf)
	{
		free(ReceivedFileDataBuf);
		ReceivedFileDataBuf = NULL;
	}

	if (FileToSendDataBuf)
	{
		free(FileToSendDataBuf);
		FileToSendDataBuf = NULL;
	}

	FileTransferringMode = IDLE_MODE;
	Command = 0;

	AddLogText("File transfer ended\r\n");
}

void CleanupSenderSession()
{
	if (FileToSendName)
	{
		free(FileToSendName);
		FileToSendName = NULL;
	}

	if (FileToSendDataBuf)
	{
		free(FileToSendDataBuf);
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

	AddLogText("File transfer ended\r\n");
}

BOOL SendNextFileSection(SOCKET Socket)
{
	sprintf_s(StrBuf, 256, "Sending file (%d/%d section)", FileToSendCurrentSection + 1, FileToSendSectionsCount);

	SetWindowTextA(StatusStatic, StrBuf);

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

				AddLogText("File transfer started by connected program\r\n");

				ResponseBuffer[0] = CMD_SENDER_FILENAME;

				DWORD NoPathNameLength;

				recv(Socket, (char*)&NoPathNameLength, 4, 0);

				ReceivedFileName = (char*)malloc(NoPathNameLength + 1);
				recv(Socket, ReceivedFileName, NoPathNameLength + 1, 0);

				sprintf_s(StrBuf, 256, "Name of received file: \"%s\"\r\n", ReceivedFileName);
				AddLogText(StrBuf);

				ReceivedFile = CreateFileA(ReceivedFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

				if (ReceivedFile != INVALID_HANDLE_VALUE)
				{
					ResponseBuffer[1] = TRUE;

					sprintf_s(StrBuf, 256, "File \"%s\" created\r\n", ReceivedFileName);
					AddLogText(StrBuf);
				}
				else
				{
					sprintf_s(StrBuf, 256, "Failed to create file \"%s\", error %ld\r\n", ReceivedFileName, GetLastError());
					AddLogText(StrBuf);

					ResponseBuffer[1] = FALSE;

					CleanupReceiverSession();
				}

				send(Socket, (const char*)ResponseBuffer, 8, 0);
			}

			// Sender side
			else if (Command == CMD_SENDER_FILENAME)
			{
				DWORD ResponseFlag;

				recv(Socket, (char*)&ResponseFlag, 4, 0);

				if (ResponseFlag == TRUE)
				{
					AddLogText("File has successfully been created\r\n");

					FileToSendSectionsCount = FileToSendSize / FILE_SECTION_SIZE + 1;

					AddLogText("File size ");
					WriteFileSize(StrBuf, FileToSendSize);
					sprintf_s(StrBuf, 256, " (%d sections), sending meta data\r\n", FileToSendSectionsCount);
					AddLogText(StrBuf);

					DWORD FileMetaToSend[3] = { CMD_RECEIVER_FILEMETA, FileToSendSize, FileToSendSectionsCount };

					send(Socket, (const char*)FileMetaToSend, 12, 0);
				}
				else
				{
					AddLogText("An serverside error occured\r\n");

					CleanupSenderSession();
				}
			}

			// Receiver side
			else if (Command == CMD_RECEIVER_FILEMETA)
			{
				DWORD FileMetaBuf[2];

				recv(Socket, (char*)FileMetaBuf, 8, 0);

				AddLogText("File meta data got, file size ");
				WriteFileSize(StrBuf, FileMetaBuf[0]);
				ReceivedFileSectionsCount = FileMetaBuf[1];
				sprintf_s(StrBuf, 256, " (%d sections)\r\n", FileMetaBuf[1]);
				AddLogText(StrBuf);

				FileMetaBuf[0] = CMD_SENDER_FILEMETA;
				FileMetaBuf[1] = TRUE;

				ReceivedFileDataBuf = (char*)malloc(FILE_SECTION_SIZE);

				send(Socket, (const char*)FileMetaBuf, 8, 0);
			}

			// Sender side
			else if (Command == CMD_SENDER_FILEMETA)
			{
				DWORD ResponseFlag;

				recv(Socket, (char*)&ResponseFlag, 4, 0);

				if (ResponseFlag == TRUE)
				{
					FileToSendDataBuf = (char*)malloc(FILE_SECTION_SIZE);
					SendNextFileSection(Socket);
				}
				else
				{
					AddLogText("An serverside error occured\r\n");
					CleanupSenderSession();
				}
			}

			// Receiver side
			else if (Command == CMD_RECEIVER_FILEDATA)
			{
				DWORD CommandsBuf[2];

				recv(Socket, (char*)CommandsBuf, 8, 0);
				recv(Socket, ReceivedFileDataBuf, CommandsBuf[0], 0);

				sprintf_s(StrBuf, 256, "Receiving file (%d/%d sections)", CommandsBuf[1], ReceivedFileSectionsCount);
				SetWindowTextA(StatusStatic, StrBuf);

				DWORD Written = 0;

				BOOL WriteResult = WriteFile(ReceivedFile, ReceivedFileDataBuf, CommandsBuf[0], &Written, NULL);

				if (WriteResult && Written == CommandsBuf[0]) CommandsBuf[1] = TRUE;
				else
				{
					sprintf_s(StrBuf, 256, "Failed to write section, WriteResult=%d, Written=%d/%d, ErrorCode=%d\r\n", WriteResult, Written, CommandsBuf[0], GetLastError());
					AddLogText(StrBuf);
					CommandsBuf[1] = FALSE;

					SetWindowTextA(StatusStatic, "Connected");

					CleanupReceiverSession();
				}

				CommandsBuf[0] = CMD_SENDER_FILEDATA;

				send(Socket, (const char*)CommandsBuf, 8, 0);
			}

			// Sender side
			else if (Command == CMD_SENDER_FILEDATA)
			{
				DWORD ResponseFlag;

				recv(Socket, (char*)&ResponseFlag, 4, 0);

				if (ResponseFlag == TRUE)
				{
					if (FileToSendCurrentSection < FileToSendSectionsCount) SendNextFileSection(Socket);
					else
					{
						AddLogText("All sections has been sent, closing file\r\n");

						DWORD CloseFileCommand = CMD_RECEIVER_CLOSEFILE;
						send(Socket, (const char*)&CloseFileCommand, 4, 0);
					}
				}
				else
				{
					AddLogText("An serverside error occured\r\n");

					sprintf_s(StrBuf, 256, "Connected to %s:%s", ConnectParams->IPText, ConnectParams->PortText);
					SetWindowTextA(StatusStatic, StrBuf);

					CleanupSenderSession();
				}
			}

			// Receiver side
			else if (Command == CMD_RECEIVER_CLOSEFILE)
			{
				sprintf_s(StrBuf, 256, "All sections of file \"%s\" has been received, closing file\r\n", ReceivedFileName);
				AddLogText(StrBuf);

				DWORD ResponseBuf[2] = { CMD_SENDER_CLOSEFILE, CloseHandle(ReceivedFile) };

				if (ResponseBuf[1]) AddLogText("File has successfully been received\r\n");
				else
				{
					sprintf_s(StrBuf, 256, "Error %d closing file\r\n", GetLastError());
					AddLogText(StrBuf);
				}

				/*sprintf_s(StrBuf, 256, "Connected to %s:%s", ConnectParams->IPText, ConnectParams->PortText);
				SetWindowTextA(StatusStatic, StrBuf);*/

				CleanupReceiverSession();

				send(Socket, (const char*)ResponseBuf, 8, 0);
			}

			// Sender side
			else if (Command == CMD_SENDER_CLOSEFILE)
			{
				DWORD ResponseFlag;

				recv(Socket, (char*)&ResponseFlag, 4, 0);

				sprintf_s(StrBuf, 256, "File \"%s\" has successfully been sent\r\n", FileToSendNoPathName);
				AddLogText(StrBuf);

				/*SetWindowTextA(StatusStatic, "Connected");*/

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