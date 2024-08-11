#include <Windows.h>
#include <TlHelp32.h>
#include <string>

wchar_t* GetWC(const char* c)
{
	const size_t cSize = strlen(c) + 1;
	size_t outSize;
	wchar_t* wc = new wchar_t[cSize];
	mbstowcs_s(&outSize, wc, cSize, c, cSize-1);

	return wc;
}

DWORD GetProcId(const char* processname) {
	std::wstring processName;
	processName.append(GetWC(processname));
	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(processInfo);

	HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (processesSnapshot == INVALID_HANDLE_VALUE)
		return 0;

	Process32First(processesSnapshot, &processInfo);

	if (!processName.compare(processInfo.szExeFile))
		//if (!processName.compare(GetWC(processInfo.szExeFile)))
	{
		CloseHandle(processesSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processesSnapshot, &processInfo))
	{
		if (!processName.compare(processInfo.szExeFile))
			//if (!processName.compare(GetWC(processInfo.szExeFile)))
		{
			CloseHandle(processesSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processesSnapshot);
	return 0;
}