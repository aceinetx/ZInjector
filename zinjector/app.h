#pragma once
#include <string>
#include "Windows.h"
#include <d3d9.h>

namespace ZInjector {
	class Application {
	public:
		Application();
		~Application();

	private:
		int InitWindow();
		static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		bool CreateDeviceD3D(HWND hWnd);
		void CleanupDeviceD3D();
		void ResetDevice();

		std::string GetLocalDir();
		void InitDirectories();
		void ImGui_Draw();

		LPDIRECT3D9              g_pD3D = nullptr;
		LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
		bool                     g_DeviceLost = false;
		UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
		D3DPRESENT_PARAMETERS    g_d3dpp = {};
		HWND hwnd;
		WNDCLASSEX wc;


		std::string dll_path;
		std::string proc_name;
		std::string preset_name;
	};
}