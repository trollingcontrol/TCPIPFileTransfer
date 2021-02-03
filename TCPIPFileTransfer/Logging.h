#pragma once

#include <Windows.h>
#include <stdio.h>

extern HWND LogEdit;
extern HANDLE ProcessHeap;

void AddLogText(LPCWSTR Text);
BOOL InitLoggingSystem();
