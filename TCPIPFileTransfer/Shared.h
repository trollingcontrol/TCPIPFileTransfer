#pragma once

#define IDLE_MODE   0

// Program modes
#define SERVER_MODE 1
#define CLIENT_MODE 2

// Whether file is being received or sent at the time
#define RECEIVER_MODE  1
#define SENDER_MODE    2

// Commands used in communication between client and server
#define CMD_HANDSHAKE 0x40109800  // Client: none; Server: BOOL
#define CMD_RECEIVER_FILENAME  3  // DWORD FileNameLength; char *FileName, length of array: FileNameLength+1
#define CMD_SENDER_FILENAME    4  // BOOL Result
#define CMD_RECEIVER_FILEMETA  5  // DWORD FileSize; DWORD NumberOfSections
#define CMD_SENDER_FILEMETA    6  // BOOL Result
#define CMD_RECEIVER_FILEDATA  7  // DWORD FileDataSize; DWORD SectionNumber; char *FileData, length of array: FileDataSize
#define CMD_SENDER_FILEDATA    8  // BOOL Result
#define CMD_RECEIVER_CLOSEFILE 9  // --
#define CMD_SENDER_CLOSEFILE  10  // BOOL Result

#define TS_ALIVE    0
#define TS_STOPPING 1

#define FILE_SECTION_SIZE 32768

struct CONNECTIONPARAMS
{
	char* IPText;
	char* PortText;
};