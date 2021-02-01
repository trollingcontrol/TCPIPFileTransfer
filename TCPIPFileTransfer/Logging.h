#pragma once

#include <Windows.h>
#include <stdio.h>

extern HWND LogEdit;

void AddLogText(LPCWSTR Text);
void InitLoggingSystem();
