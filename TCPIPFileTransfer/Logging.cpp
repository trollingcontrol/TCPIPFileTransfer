#include "Logging.h"

#define LOGGING_BUFFER_SIZE 32768

LPWSTR LoggingBufferStart;
LPWSTR LoggingBufferEnd;

void AddLogText(LPCWSTR Text)
{
	int Len = wcslen(Text) + 1;

	if (Len > LOGGING_BUFFER_SIZE - (LoggingBufferEnd - LoggingBufferStart))
	{
		memmove(LoggingBufferStart, LoggingBufferStart + 8192, LoggingBufferEnd - (LoggingBufferStart + 8192));
		LoggingBufferEnd -= 8192;
	}

	memcpy(LoggingBufferEnd, Text, Len * sizeof(WCHAR));
	LoggingBufferEnd += Len - 1;

	SetWindowTextW(LogEdit, LoggingBufferStart);
}

void InitLoggingSystem()
{
	LoggingBufferStart = (LPWSTR)malloc(LOGGING_BUFFER_SIZE * sizeof(WCHAR));
	LoggingBufferEnd = LoggingBufferStart;
}