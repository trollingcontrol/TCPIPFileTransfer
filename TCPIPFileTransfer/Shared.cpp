#include "Shared.h"

void ShowWinFuncError(HWND Window, LPCWSTR FuncName)
{
	WCHAR StrBuf[256];
	swprintf_s(StrBuf, 256, L"%s failed, error %ld", FuncName, GetLastError());

	MessageBoxW(Window, StrBuf, L"Error", MB_ICONERROR);
}

void FlushNetworkBuffer(SOCKET Socket, int Len)
{
	char Buf[1024];

	int BytesToRead = 1024;

	while (Len > 0)
	{
		if (Len < BytesToRead) BytesToRead = Len;
		recv(Socket, Buf, BytesToRead, 0);
		Len -= BytesToRead;
	}
}