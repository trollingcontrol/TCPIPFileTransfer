#pragma once

#include <Windows.h>
#include <stdio.h>

extern HWND LogEdit;

void AddLogText(LPCSTR Text);
void InitLoggingSystem();
