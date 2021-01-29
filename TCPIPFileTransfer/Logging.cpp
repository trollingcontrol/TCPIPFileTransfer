#include "Logging.h"

#define LOGGING_BUFFER_SIZE 32768

char* LoggingBufferStart;
char* LoggingBufferEnd;

void AddLogText(LPCSTR Text)
{
	int Len = strlen(Text) + 1;

	if (Len > LOGGING_BUFFER_SIZE - (LoggingBufferEnd - LoggingBufferStart))
	{
		memmove(LoggingBufferStart, LoggingBufferStart + 8192, LoggingBufferEnd - (LoggingBufferStart + 8192));
		LoggingBufferEnd -= 8192;
	}

	memcpy(LoggingBufferEnd, Text, Len);
	LoggingBufferEnd += Len - 1;

	SetWindowTextA(LogEdit, LoggingBufferStart);
}

void InitLoggingSystem()
{
	LoggingBufferStart = (char*)malloc(LOGGING_BUFFER_SIZE);
	LoggingBufferEnd = LoggingBufferStart;
}