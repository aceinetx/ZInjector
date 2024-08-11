#include "imgui.h"
#include "config.h"
#include "process.h"

#include <string>
#include <format>
#include <Windows.h>
#include <shobjidl.h> 

char dll_path[1024];
char proc_name[128];

void ImGui_Draw() {
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WINDOW_WIDTH-15, WINDOW_HEIGHT-38 });
	ImGui::Begin("ZInjector", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

	ImGui::Text("ZInjector %s", APP_VERSION);

	ImGui::InputText("Process name", proc_name, sizeof(proc_name));
	ImGui::InputText("DLL Path", dll_path, sizeof(dll_path));
	ImGui::SameLine();
	if (ImGui::Button("Browse")) {
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
			COINIT_DISABLE_OLE1DDE);
		IFileOpenDialog* pFileOpen;

		hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
			IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr))
		{
			hr = pFileOpen->Show(NULL);

			if (SUCCEEDED(hr))
			{
				IShellItem* pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

					if (SUCCEEDED(hr))
					{
						char new_dll_path[sizeof(dll_path)] = { 0 };
						memset(new_dll_path, 0, sizeof(dll_path));
						size_t outSize;
						wcstombs_s(&outSize, new_dll_path, pszFilePath, sizeof(dll_path));
						strncpy_s(dll_path, new_dll_path, sizeof(dll_path));
						CoTaskMemFree(pszFilePath);
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}

	if (ImGui::Button("Inject")) {
		DWORD pid = GetProcId(proc_name);

		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

		LPVOID pDllPath = VirtualAllocEx(hProcess, 0, strlen(dll_path) + 1,
			MEM_COMMIT, PAGE_READWRITE);
		
		if (pDllPath != 0) {
			WriteProcessMemory(hProcess, pDllPath, (LPVOID)dll_path,
				strlen(dll_path) + 1, 0);

			HMODULE krnl32 = GetModuleHandleA("kernel32.dll");
			if (krnl32 != 0) {

				HANDLE hLoadThread = CreateRemoteThread(hProcess, 0, 0,
					(LPTHREAD_START_ROUTINE)GetProcAddress(krnl32,
						"LoadLibraryA"), pDllPath, 0, 0);

				if (hLoadThread != 0) {
					WaitForSingleObject(hLoadThread, INFINITE);
				}
			}
		}
	}
		
	ImGui::End();
}