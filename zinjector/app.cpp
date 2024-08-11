#include "imgui.h"
#include "config.h"
#include "process.h"
#include "json.hpp"

#include <string>
#include <format>
#include <Windows.h>
#include <shobjidl.h> 
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

char dll_path[1024];
char proc_name[128];

char preset_name[64];

namespace ZInjector {

	std::string GetLocalDir() {
		char username[50];
		GetEnvironmentVariableA("username", username, sizeof(username));
		std::string path = std::format("C:\\Users\\{}\\AppData\\Local\\ZInjector", username);
		return path;
	}

	void Init() {
		std::string path = GetLocalDir();
		std::filesystem::create_directory(path);
		std::filesystem::create_directory(path+"\\presets");
	}

	void ImGui_Draw() {
		ImGui::SetNextWindowPos({ 0, 0 });
		ImGui::SetNextWindowSize({ WINDOW_WIDTH - 15, WINDOW_HEIGHT - 38 });
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

		if (ImGui::CollapsingHeader("Presets")) {
			
			for (const auto& entry : std::filesystem::directory_iterator(GetLocalDir() + "\\presets")) {
				std::string name = entry.path().filename().string();
				const char* name_c_str = name.c_str();
				if (ImGui::Button(name_c_str)) {
					strncpy_s(preset_name, name_c_str, sizeof(preset_name));
				}
			}

			ImGui::InputText("Name", preset_name, sizeof(preset_name));

			std::string preset_path = std::format("{}\\presets\\{}", GetLocalDir(), preset_name);

			if (ImGui::Button("New")) {
				std::ofstream f;
				f.open(preset_path);
				f << "{\"dll_path\": \"\", \"process_name\": \"\"}";
				f.close();
			}
			if (ImGui::Button("Load")) {
				std::ifstream f(preset_path);
				json data = json::parse(f);

				strncpy_s(dll_path, data["dll_path"].template get<std::string>().c_str(), sizeof(dll_path));
				strncpy_s(proc_name, data["process_name"].template get<std::string>().c_str(), sizeof(proc_name));
			}
			if (ImGui::Button("Save")) {
				json data;
				data["dll_path"] = dll_path;
				data["process_name"] = proc_name;

				std::ofstream f;
				f.open(preset_path);
				f.clear();
				f << data.dump();
				f.close();
			}
			if (ImGui::Button("Delete")) {
				std::filesystem::remove(preset_path);
			}
		}

		ImGui::End();
	}
}