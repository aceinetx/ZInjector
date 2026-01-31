#include "imgui.h"
#include "config.h"
#include "process.h"
#include "json.hpp"
#include "app.h"

#include <string>
#include <format>
#include <Windows.h>
#include <shobjidl.h> 
#include <filesystem>
#include <fstream>
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <d3d9.h>
#include <tchar.h>
#include "font.h"
#include "style.h"
#include "imgui_stdlib.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using json = nlohmann::json;

namespace ZInjector {
	Application::Application() {
		InitWindow();
	}

	Application::~Application() = default;

	int Application::InitWindow() {
		// Create application window
		//ImGui_ImplWin32_EnableDpiAwareness();
		wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ZInjector", nullptr };
		::RegisterClassExW(&wc);
		hwnd = ::CreateWindowW(wc.lpszClassName, L"ZInjector", WS_OVERLAPPEDWINDOW, 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, nullptr, nullptr, wc.hInstance, this);

		// Initialize Direct3D
		if (!CreateDeviceD3D(hwnd))
		{
			CleanupDeviceD3D();
			::UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return 1;
		}

		// Show the window
		::ShowWindow(hwnd, SW_SHOWDEFAULT);
		::UpdateWindow(hwnd);

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		ImFontConfig font;
		font.FontDataOwnedByAtlas = false;
		font.RasterizerDensity = 1.0f;


		static const ImWchar ranges[] =
		{
			0x0020, 0x00FF, // Basic Latin + Latin Supplement
			0x0400, 0x044F, // Cyrillic
			0,
		};
		//io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 15.0f, NULL, ranges);
		io.Fonts->AddFontFromMemoryTTF(fontData, sizeof(fontData), 20.5f, &font, ranges);

		// Setup Dear ImGui style
		SetupImGuiStyle();

		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX9_Init(g_pd3dDevice);

		InitDirectories();

		// Our state
		bool show_demo_window = true;
		bool show_another_window = false;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		// Main loop
		bool done = false;
		while (!done)
		{
			// Poll and handle messages (inputs, window resize, etc.)
			// See the WndProc() function below for our to dispatch events to the Win32 backend.
			MSG msg;
			while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
					done = true;
			}
			if (done)
				break;

			// Handle lost D3D9 device
			if (g_DeviceLost)
			{
				HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
				if (hr == D3DERR_DEVICELOST)
				{
					::Sleep(10);
					continue;
				}
				if (hr == D3DERR_DEVICENOTRESET)
					ResetDevice();
				g_DeviceLost = false;
			}

			// Handle window resize (we don't resize directly in the WM_SIZE handler)
			if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
			{
				g_d3dpp.BackBufferWidth = g_ResizeWidth;
				g_d3dpp.BackBufferHeight = g_ResizeHeight;
				g_ResizeWidth = g_ResizeHeight = 0;
				ResetDevice();
			}

			// Start the Dear ImGui frame
			ImGui_ImplDX9_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			ImGui_Draw();

			// Rendering
			ImGui::EndFrame();
			g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
			g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
			D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
			g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
			if (g_pd3dDevice->BeginScene() >= 0)
			{
				ImGui::Render();
				ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
				g_pd3dDevice->EndScene();
			}
			HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
			if (result == D3DERR_DEVICELOST)
				g_DeviceLost = true;
		}

		// Cleanup
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		CleanupDeviceD3D();
		::DestroyWindow(hwnd);
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);

		return 0;
	}

	LRESULT WINAPI Application::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Application* pThis{ nullptr };
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;

		if (msg == WM_NCCREATE) {
			CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
			pThis = reinterpret_cast<Application*>(cs->lpCreateParams);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
		}
		else {
			pThis = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		}

		assert(pThis);

		switch (msg)
		{
		case WM_SIZE:
			if (wParam == SIZE_MINIMIZED)
				return 0;
			pThis->g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
			pThis->g_ResizeHeight = (UINT)HIWORD(lParam);
			return 0;
		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
				return 0;
			break;
		case WM_DESTROY:
			::PostQuitMessage(0);
			return 0;
		}
		return ::DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	bool Application::CreateDeviceD3D(HWND hWnd)
	{
		if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
			return false;

		// Create the D3DDevice
		ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
		g_d3dpp.Windowed = TRUE;
		g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
		g_d3dpp.EnableAutoDepthStencil = TRUE;
		g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
		g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
		//g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
		if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
			return false;

		return true;
	}

	void Application::CleanupDeviceD3D()
	{
		if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
		if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
	}

	void Application::ResetDevice()
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
		if (hr == D3DERR_INVALIDCALL)
			IM_ASSERT(0);
		ImGui_ImplDX9_CreateDeviceObjects();
	}

	std::string Application::GetLocalDir() {
		char username[50];
		GetEnvironmentVariableA("username", username, sizeof(username));
		std::string path = std::format("C:\\Users\\{}\\AppData\\Local\\ZInjector", username);
		return path;
	}

	void Application::InitDirectories() {
		std::string path = GetLocalDir();
		std::filesystem::create_directory(path);
		std::filesystem::create_directory(path + "\\presets");
	}

	void Application::ImGui_Draw() {
		ImGui::SetNextWindowPos({ 0, 0 });
		ImGui::SetNextWindowSize({ WINDOW_WIDTH - 15, WINDOW_HEIGHT - 38 });
		ImGui::Begin("ZInjector", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

		ImGui::Text("ZInjector %s", APP_VERSION);

		ImGui::InputText("Process name", &proc_name);
		ImGui::InputText("DLL Path", &dll_path);
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
							std::wstring pwsFilePath(pszFilePath);
							dll_path = std::string(pwsFilePath.begin(), pwsFilePath.end());
						}
						pItem->Release();
					}
				}
				pFileOpen->Release();
			}
			CoUninitialize();
		}

		if (ImGui::Button("Inject")) {
			DWORD pid = GetProcId(proc_name.c_str());

			HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

			LPVOID pDllPath = VirtualAllocEx(hProcess, 0, dll_path.size() + 1,
				MEM_COMMIT, PAGE_READWRITE);

			if (pDllPath != 0) {
				WriteProcessMemory(hProcess, pDllPath, (LPVOID)dll_path.c_str(),
					dll_path.size() + 1, 0);

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
				if (ImGui::Button(name.c_str())) {
					preset_name = name;
				}
			}

			ImGui::InputText("Name", &preset_name);

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

				dll_path = data["dll_path"].template get<std::string>();
				proc_name = data["process_name"].template get<std::string>();
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