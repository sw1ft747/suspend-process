// Suspend/Resume Process

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <TlHelp32.h>

#include <stdio.h>
#include <string.h>

#include "ini-parser/ini_parser.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

typedef LONG (NTAPI *NtSuspendProcessFn)(IN HANDLE processHandle);
typedef LONG (NTAPI *NtResumeProcessFn)(IN HANDLE processHandle);

//-----------------------------------------------------------------------------
// Settings from configuration file
//-----------------------------------------------------------------------------

const char *g_pszProcessName = NULL;
const wchar_t *g_pwcProcessName = NULL;

bool g_bAutoSuspend = false;
DWORD g_ToggleKey = 0x2D;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

HANDLE g_hProcess = NULL;
DWORD g_dwProcessID = 0;

//-----------------------------------------------------------------------------
// Purpose: get process id
//-----------------------------------------------------------------------------

bool GetProcessID(const wchar_t *pwcProcessName, DWORD &dwProcessID)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	PROCESSENTRY32 modEntry;
	modEntry.dwSize = sizeof(PROCESSENTRY32);

	do
	{
		if (!_wcsicmp(modEntry.szExeFile, pwcProcessName))
		{
			dwProcessID = (DWORD)modEntry.th32ProcessID;
			CloseHandle(hSnapshot);

			return true;
		}
	} while (Process32Next(hSnapshot, &modEntry));

	CloseHandle(hSnapshot);
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: get process handle
//-----------------------------------------------------------------------------

bool GetProcessHandle(HANDLE &hProcess, DWORD dwProcessID, DWORD dwAccess)
{
	hProcess = OpenProcess(dwAccess, FALSE, dwProcessID);
	return hProcess != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: suspend process
//-----------------------------------------------------------------------------

void SuspendProcess(HANDLE hProcess)
{
	static HMODULE hModule = NULL;
	static NtSuspendProcessFn NtSuspendProcess = NULL;

	if (!hModule)
		hModule = GetModuleHandle(L"ntdll.dll");

	if (!NtSuspendProcess)
		NtSuspendProcess = (NtSuspendProcessFn)GetProcAddress(hModule, "NtSuspendProcess");

	NtSuspendProcess(hProcess);
}

//-----------------------------------------------------------------------------
// Purpose: resume process
//-----------------------------------------------------------------------------

void ResumeProcess(HANDLE hProcess)
{
	static HMODULE hModule = NULL;
	static NtResumeProcessFn NtResumeProcess = NULL;

	if (!hModule)
		hModule = GetModuleHandle(L"ntdll.dll");

	if (!NtResumeProcess)
		NtResumeProcess = (NtResumeProcessFn)GetProcAddress(hModule, "NtResumeProcess");

	NtResumeProcess(hProcess);
}

//-----------------------------------------------------------------------------
// Purpose: parse configuration file
//-----------------------------------------------------------------------------

bool ParseFile()
{
	printf("Trying to find the config file...\n");

	ini_data data;
	ini_datatype datatype;

	if (!ini_parse_data("suspend_process.ini", &data))
	{
		if (ini_get_last_error() == INI_MISSING_FILE)
			printf("Missing file suspend_process.ini to parse\n");
		else
			printf("Syntax error: %s in line %d\n", ini_get_last_error_msg(), ini_get_last_line());

		return false;
	}

	INI_FIELDTYPE_CSTRING(datatype);

	if (ini_read_data(&data, "SETTINGS", "ProcessName", &datatype))
	{
		g_pszProcessName = datatype.m_pszString;
	}

	INI_FIELDTYPE_BOOL(datatype);

	if (ini_read_data(&data, "SETTINGS", "AutoSuspend", &datatype))
	{
		g_bAutoSuspend = datatype.m_bool;
	}

	INI_FIELDTYPE_UINT32(datatype, 16);

	if (ini_read_data(&data, "CONTROLS", "ToggleKey", &datatype))
	{
		g_ToggleKey = datatype.m_uint32;
	}

	printf("Parsed the config file\n");

	ini_free_data(&data, 0);

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

int main()
{
	bool bSuspended = false;
	bool bKeyPressed = false;


	if (!ParseFile())
	{
		printf("Failed to parse the config file\n");
		Sleep(3000);

		return 1;
	}


	const size_t length = strlen(g_pszProcessName) + 1;

	g_pwcProcessName = new wchar_t[length];
	mbstowcs((wchar_t *)g_pwcProcessName, g_pszProcessName, length);


	printf("Trying to get process called %s\n", g_pszProcessName);


	while (!g_hProcess)
	{
		if (GetProcessID(g_pwcProcessName, g_dwProcessID))
			GetProcessHandle(g_hProcess, g_dwProcessID, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SUSPEND_RESUME);

		Sleep(50);
	}


	printf("Connected to the process\n");


	if (g_bAutoSuspend)
		goto FORCE_SUSPEND;


	while (true)
	{
		DWORD dwExitCode;
		GetExitCodeProcess(g_hProcess, &dwExitCode);

		if (!dwExitCode)
		{
			printf("The process has been closed\nExiting...\n");
			break;
		}

		if (GetAsyncKeyState(g_ToggleKey))
		{
			if (!bKeyPressed)
			{
				if (bSuspended)
				{
					ResumeProcess(g_hProcess);
					bSuspended = false;

					printf("Process is resumed\n");
				}
				else
				{
				FORCE_SUSPEND:
					SuspendProcess(g_hProcess);
					bSuspended = true;

					printf("Process is suspended\n");
				}
			}

			bKeyPressed = true;
		}
		else
		{
			bKeyPressed = false;
		}

		Sleep(10);
	}


	Sleep(3000);
	return 0;
}