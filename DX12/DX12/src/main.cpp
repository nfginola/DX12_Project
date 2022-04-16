#include "pch.h"

#include "Window.h"
#include "DXContext.h"
#include "DXSwapChain.h"
#include "DXCompiler.h"
#include "DXHelper.h"
#include "GPUProfiler.h"
#include "CPUProfiler.h"

// ImGui
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

// Check for memory leaks
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "DXTK/DirectXHelpers.h"

#include "Input.h"

static bool g_app_running = false;
Input* g_input = nullptr;

LRESULT window_procedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int main()
{
	g_app_running = true;
	constexpr auto debug_on = true;
	const UINT CLIENT_WIDTH = 1600;
	const UINT CLIENT_HEIGHT = 900;


	// https://docs.microsoft.com/en-us/visualstudio/debugger/finding-memory-leaks-using-the-crt-library?view=vs-2022
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	try
	{
		// initialize window
		auto win_proc = [](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT { return window_procedure(hwnd, uMsg, wParam, lParam); };
		auto win = std::make_unique<Window>(GetModuleHandle(NULL), win_proc, CLIENT_WIDTH, CLIENT_HEIGHT);
	
		g_input = new Input(win->get_hwnd());

		// initialize dx context
		DXContext::Settings ctx_set{};
		ctx_set.debug_on = debug_on;
		ctx_set.hwnd = win->get_hwnd();
		auto gfx_ctx = std::make_shared<DXContext>(ctx_set);

		// grab associated swapchain
		auto gfx_sc = gfx_ctx->get_sc();
		
		// initialize DX shader compiler
		auto shd_clr = std::make_unique<DXCompiler>();

		// grab LL modules for creating DX12 primitives
		auto dev = gfx_ctx->get_dev();
		auto dq = gfx_ctx->get_direct_queue();

		struct SyncRes
		{
			UINT fence_val_to_wait_for = 0;
			cptr<ID3D12Fence> fence;
			HANDLE fence_event;

			void wait() const
			{
				if (fence->GetCompletedValue() < fence_val_to_wait_for)
				{
					// Raise an event when fence reaches fenceVal
					ThrowIfFailed(fence->SetEventOnCompletion(fence_val_to_wait_for, fence_event), DET_ERR("Failed to couple Fence and Event"));
					// CPU block until event is done
					WaitForSingleObject(fence_event, INFINITE);
				}
			}
		};

		struct PerFrameResource
		{
			SyncRes sync;
			UINT prev_surface_idx = 0;
			cptr<ID3D12CommandAllocator> dq_ator;
			cptr<ID3D12GraphicsCommandList> dq_cmdl;
		};

		// create sync primitives
		UINT running_fence_val = 1;
		const auto& max_FIF = gfx_sc->get_settings().max_FIF;
		std::vector<PerFrameResource> per_frame_res;
		per_frame_res.resize(max_FIF);
		for (auto& res : per_frame_res)
		{
			// init sync res
			res.sync.fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			ThrowIfFailed(dev->CreateFence(res.sync.fence_val_to_wait_for, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(res.sync.fence.GetAddressOf())), DET_ERR("Failed to create fence"));

			// init dq ator & cmd list
			constexpr auto q_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			ThrowIfFailed(dev->CreateCommandAllocator(q_type, IID_PPV_ARGS(res.dq_ator.GetAddressOf())), DET_ERR("Failed to create cmd ator"));
			ThrowIfFailed(dev->CreateCommandList(0, q_type, res.dq_ator.Get(), nullptr, IID_PPV_ARGS(res.dq_cmdl.GetAddressOf())), DET_ERR("Failed to create direct cmd list"));
			res.dq_cmdl->Close();
		}

		// create RTVs
		cptr<ID3D12DescriptorHeap> rtv_dheap;
		D3D12_DESCRIPTOR_HEAP_DESC dheap_d{};
		dheap_d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		dheap_d.NumDescriptors = max_FIF;
		ThrowIfFailed(dev->CreateDescriptorHeap(&dheap_d, IID_PPV_ARGS(rtv_dheap.GetAddressOf())), DET_ERR("Failed to create RTV descriptor heap"));
		const auto rtv_hdl_size = gfx_ctx->get_hdl_sizes().rtv;
		for (auto i = 0; i < max_FIF; ++i)
		{
			auto hdl = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtv_dheap->GetCPUDescriptorHandleForHeapStart())
				.Offset(i, rtv_hdl_size);
			dev->CreateRenderTargetView(gfx_sc->get_backbuffer(i), nullptr, hdl);
		}

		auto main_vp = CD3DX12_VIEWPORT(0.f, 0.f, CLIENT_WIDTH, CLIENT_HEIGHT, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH);
		auto main_scissor = CD3DX12_RECT(0, 0, CLIENT_WIDTH, CLIENT_HEIGHT);

		// Setup profiler
		dev->SetStablePowerState(true);
		uint8_t pf_latency = max_FIF;
		GPUProfiler gpu_pf(dev, GPUProfiler::QueueType::eDirectOrCompute, pf_latency);
		CPUProfiler cpu_pf(pf_latency);

		// Setup ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		//io.ConfigViewportsNoAutoMerge = true;
		//io.ConfigViewportsNoTaskBarIcon = true;

		ImGui::StyleColorsDark();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
			
		// Setup resource view for ImGUI usage
		cptr<ID3D12DescriptorHeap> dheap_for_imgui;
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 1;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			ThrowIfFailed(dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dheap_for_imgui)), DET_ERR("Bazooka"));
		}

		ImGui_ImplWin32_Init(win->get_hwnd());
		ImGui_ImplDX12_Init(dev, max_FIF,
			DXGI_FORMAT_R8G8B8A8_UNORM, dheap_for_imgui.Get(),
			dheap_for_imgui->GetCPUDescriptorHandleForHeapStart(),
			dheap_for_imgui->GetGPUDescriptorHandleForHeapStart());

		/*
			Idea:

			nGUI::init(..)
			nGUI::destroy();

			nGUI::imgui_submit([]()
				{
			
				});
			
			Maybe do same to Mouse/Kb? Don't know yet..

		*/


		MSG msg{};
		while (g_app_running)
		{
			win->pump_messages();

			g_input->begin();

			auto surface_idx = gfx_sc->get_curr_draw_surface();
			auto& frame_res = per_frame_res[surface_idx];
			auto curr_bb = gfx_sc->get_backbuffer(surface_idx);
			auto dq_ator = frame_res.dq_ator.Get();
			auto dq_cmdl = frame_res.dq_cmdl.Get();

			// wait for FIF
			frame_res.sync.wait();
			
			cpu_pf.frame_begin();
			gpu_pf.frame_begin(surface_idx);

			// start imGui
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();


			// query profiler results
			//std::cout << "GPU:\n";
			//const auto& profiles = gpu_pf.get_profiles();
			//for (const auto& [_, profile] : profiles)
			//{
			//	std::cout << "(" << profile.name << "):\t elapsed in ms: " << profile.sec_elapsed * 1000.0 << "\n";
			//}
			//std::cout << "\n";

			//std::cout << "CPU:\n";
			//const auto& cpu_profiles = cpu_pf.get_profiles();
			//for (const auto& [_, profile] : cpu_profiles)
			//{
			//	std::cout << "(" << profile.name << "):\t elapsed in ms: " << profile.sec_elapsed * 1000.0 << "\n";
			//}
			//std::cout << "========================================\n";

			if (g_input->lmb_down())
				std::cout << g_input->get_mouse_position().first << ", " << g_input->get_mouse_position().second << "\n";

			if (g_input->key_released(Keys::A))
				std::cout << "A released\n";

			// reset
			dq_ator->Reset();
			dq_cmdl->Reset(dq_ator, nullptr);

			gpu_pf.profile_begin(dq_cmdl, dq, "frame");
			cpu_pf.profile_begin("cpu frame");

			// transition
			gpu_pf.profile_begin(dq_cmdl, dq, "transition #1");
			auto barr_to_rt = CD3DX12_RESOURCE_BARRIER::Transition(curr_bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			dq_cmdl->ResourceBarrier(1, &barr_to_rt);
			gpu_pf.profile_end(dq_cmdl, "transition #1");

			// clear
			gpu_pf.profile_begin(dq_cmdl, dq, "clear");
			auto rtv_hdl = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtv_dheap->GetCPUDescriptorHandleForHeapStart())
				.Offset(surface_idx, rtv_hdl_size);
			FLOAT clear_color[4] = { 0.5f, 0.2f, 0.2f, 1.f };
			dq_cmdl->ClearRenderTargetView(rtv_hdl, clear_color, 1, &main_scissor);
			gpu_pf.profile_end(dq_cmdl, "clear");


			// draw imgui
			bool show_demo_window = true;
			if (show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);

			// Main menu
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Edit"))
				{
					if (ImGui::MenuItem("Undo", "CTRL+Z")) { std::cout << "haha!\n"; }
					if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
					ImGui::Separator();
					if (ImGui::MenuItem("Cut", "CTRL+X")) {}
					if (ImGui::MenuItem("Copy", "CTRL+C")) {}
					if (ImGui::MenuItem("Paste", "CTRL+V")) {}
					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}


			// end imgui
			ImGui::Render();
			
			// render imgui data
			dq_cmdl->OMSetRenderTargets(1, &rtv_hdl, false, nullptr);
			dq_cmdl->SetDescriptorHeaps(1, dheap_for_imgui.GetAddressOf());		// We should reserve a single element on our main render desc heap
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dq_cmdl);

			// Update and Render additional Platform Windows
			if (g_app_running && io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault(NULL, (void*)dq_cmdl);
			}


			// transition
			gpu_pf.profile_begin(dq_cmdl, dq, "transition #2");
			auto barr_to_present = CD3DX12_RESOURCE_BARRIER::Transition(curr_bb, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
			dq_cmdl->ResourceBarrier(1, &barr_to_present);
			gpu_pf.profile_end(dq_cmdl, "transition #2");

			gpu_pf.profile_end(dq_cmdl, "frame");
			cpu_pf.profile_end("cpu frame");

			// done with profiling
			gpu_pf.frame_end(dq_cmdl);
			cpu_pf.frame_end();

			// done with cmds
			dq_cmdl->Close();

			// exec
			ID3D12CommandList* cmdls[] = { dq_cmdl };
			dq->ExecuteCommandLists(_countof(cmdls), cmdls);


			// present
			constexpr auto vsync = true;
			gfx_sc->present(vsync);

			// signal when this frame is no longer in flight
			frame_res.sync.fence_val_to_wait_for = running_fence_val++;
			ThrowIfFailed(dq->Signal(
				frame_res.sync.fence.Get(), 
				frame_res.sync.fence_val_to_wait_for), 
				DET_ERR("Cant add a signal request to queue"));
			frame_res.prev_surface_idx = surface_idx;

			g_input->end();
		}

		// wait for all FIFs before exiting
		for (const auto& frame_res : per_frame_res)
			frame_res.sync.wait();

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		delete g_input;
	}
	catch (std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
	}
	_CrtDumpMemoryLeaks();

	return 0;
}


// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT window_procedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
		return true;

	switch (uMsg)
	{
		// DirectXTK Mouse and Keyboard (Input)
	case WM_ACTIVATEAPP:
	{
		if (g_input)
		{
			g_input->process_keyboard(uMsg, wParam, lParam);
			g_input->process_mouse(uMsg, wParam, lParam);
		}
	}
	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
	{
		if (g_input)
			g_input->process_mouse(uMsg, wParam, lParam);

		break;
	}

	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			g_app_running = false;
		}
	case WM_KEYUP:
	case WM_SYSKEYUP:
		if (g_input)
			g_input->process_keyboard(uMsg, wParam, lParam);
		break;




	case WM_DESTROY:
		g_app_running = false;
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}