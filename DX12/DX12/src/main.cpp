#include "pch.h"

#include "Window.h"
#include "DXContext.h"
#include "DXSwapChain.h"
#include "DXCompiler.h"

// Check for memory leaks
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

static bool g_app_running = false;

LRESULT window_procedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int main()
{
	g_app_running = true;
	UINT CLIENT_WIDTH = 1280;
	UINT CLIENT_HEIGHT = 720;

	// https://docs.microsoft.com/en-us/visualstudio/debugger/finding-memory-leaks-using-the-crt-library?view=vs-2022
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	try
	{
		// initialize window
		auto win_proc = [](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT { return window_procedure(hwnd, uMsg, wParam, lParam); };
		auto win = std::make_unique<Window>(GetModuleHandle(NULL), win_proc, CLIENT_WIDTH, CLIENT_HEIGHT);
	
		// initialize dx context
		DXContext::Settings ctx_set{};
		ctx_set.debug_on = true;
		ctx_set.hwnd = win->get_hwnd();
		auto gfx_ctx = std::make_shared<DXContext>(ctx_set);

		// grab associated swapchain
		auto gfx_sc = gfx_ctx->get_sc();
		
		// initialize dxc compiler
		auto shd_clr = std::make_unique<DXCompiler>();

		MSG msg{};
		while (g_app_running)
		{
			win->pump_messages();

			
		}

	}
	catch (std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
	}
	_CrtDumpMemoryLeaks();

	/*
			Init DXDevice	
			Set Debug Layer accordingly
			Get Adapter
			Get Device
				
			Init DX12SwapChain
			Returns a SwapChain to user	

		Init DXCompiler
			
	*/


	return 0;
}

LRESULT window_procedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		g_app_running = false;
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}