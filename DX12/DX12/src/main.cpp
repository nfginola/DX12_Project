#include "pch.h"

// Check for memory leaks
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>

#include "Window.h"
#include "DXContext.h"
#include "DXSwapChain.h"
#include "DXCompiler.h"
#include "GPUProfiler.h"
#include "CPUProfiler.h"
#include "Input.h"
#include "GUIContext.h"
#include "AssimpLoader.h"
#include "HandlePool.h"
#include "WinPixEventRuntime/pix3.h"
#include "DXBufferMemPool.h"
#include "DXConstantRingBuffer.h"

#include "../shaders/ShaderInterop_Renderer.h"

static bool g_app_running = false;
Input* g_input = nullptr;
GUIContext* g_gui_ctx = nullptr;

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

			
		// Setup resource view for ImGUI usage
		cptr<ID3D12DescriptorHeap> dheap_for_imgui;
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 1;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			ThrowIfFailed(dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dheap_for_imgui)), DET_ERR("Bazooka"));
		}


		// Setup ImGUI
		g_gui_ctx = new GUIContext(win->get_hwnd(), dev, max_FIF,
			dheap_for_imgui.Get(),
			dheap_for_imgui->GetCPUDescriptorHandleForHeapStart(),
			dheap_for_imgui->GetGPUDescriptorHandleForHeapStart());

		// Setup test UI
		bool show_pf = true;
		auto ui_cb = [&]()
		{
			ImGui::Begin("HelloBox");
			ImGui::Checkbox("Show Profiler Data", &show_pf);
			ImGui::End();

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
		};

		g_gui_ctx->add_persistent_ui("test", ui_cb);

		// designed to be consumed by some higher level algorithm for allocation
		// e.g dynamic ring buffer
		DXBufferMemPool mem_pool(dev, 256, 100, D3D12_HEAP_TYPE_UPLOAD);
		auto alloc = mem_pool.allocate();
		mem_pool.deallocate(alloc);
		// detect free-after-free
		//mem_pool.deallocate(*alloc);

		{
			DXConstantRingBuffer ring_buf(dev);

			ring_buf.frame_begin(0);	// emulate start of frame

			// 2 requests this frame
			auto alloc1 = ring_buf.allocate(200);
			auto alloc2 = ring_buf.allocate(350);

			ring_buf.frame_begin(1);
			ring_buf.frame_begin(2);

			// should clear alloc 1 & 2
			ring_buf.frame_begin(0);

			auto alloc11 = ring_buf.allocate(200);
			auto alloc22 = ring_buf.allocate(350);


		}

	




		MSG msg{};
		while (g_app_running)
		{
			win->pump_messages();
			if (!g_app_running)		// Early exit if WMs see exit request
				break;

			g_input->frame_begin();

			auto surface_idx = gfx_sc->get_curr_draw_surface();
			auto& frame_res = per_frame_res[surface_idx];
			auto curr_bb = gfx_sc->get_backbuffer(surface_idx);
			auto dq_ator = frame_res.dq_ator.Get();
			auto dq_cmdl = frame_res.dq_cmdl.Get();

			// wait for FIF
			frame_res.sync.wait();
			
			cpu_pf.frame_begin();
			gpu_pf.frame_begin(surface_idx);
			g_gui_ctx->frame_begin();


			if (show_pf)
			{

				// query profiler results
				std::cout << "GPU:\n";
				const auto& profiles = gpu_pf.get_profiles();
				for (const auto& [_, profile] : profiles)
				{
					std::cout << "(" << profile.name << "):\t elapsed in ms: " << profile.sec_elapsed * 1000.0 << "\n";
				}
				std::cout << "\n";

				std::cout << "CPU:\n";
				const auto& cpu_profiles = cpu_pf.get_profiles();
				for (const auto& [_, profile] : cpu_profiles)
				{
					std::cout << "(" << profile.name << "):\t elapsed in ms: " << profile.sec_elapsed * 1000.0 << "\n";
				}
				std::cout << "========================================\n";
			}

			// reset
			dq_ator->Reset();
			dq_cmdl->Reset(dq_ator, nullptr);

			PIXBeginEvent(dq_cmdl, PIX_COLOR(0, 200, 200), "Hey");

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

			dq_cmdl->OMSetRenderTargets(1, &rtv_hdl, false, nullptr);

			// render imgui data
			dq_cmdl->SetDescriptorHeaps(1, dheap_for_imgui.GetAddressOf());		// We should reserve a single descriptor element on our main render desc heap
			g_gui_ctx->render(dq_cmdl);

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


			PIXEndEvent(dq_cmdl);

			// done with cmds
			dq_cmdl->Close();

			// exec
			ID3D12CommandList* cmdls[] = { dq_cmdl };
			dq->ExecuteCommandLists(_countof(cmdls), cmdls);

			// present
			constexpr auto vsync = true;
			gfx_sc->present(vsync);

			// signal when this frame is no longer in flight
			frame_res.sync.fence_val_to_wait_for = gfx_ctx->get_next_fence_value();
			ThrowIfFailed(dq->Signal(
				frame_res.sync.fence.Get(), 
				frame_res.sync.fence_val_to_wait_for), 
				DET_ERR("Cant add a signal request to queue"));
			frame_res.prev_surface_idx = surface_idx;

			g_input->frame_end();
			g_gui_ctx->frame_end();
		}

		// wait for all FIFs before exiting
		for (const auto& frame_res : per_frame_res)
			frame_res.sync.wait();

		delete g_input;
		delete g_gui_ctx;

		g_input = nullptr;
		g_gui_ctx = nullptr;
	}
	catch (std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
	}




	_CrtDumpMemoryLeaks();

	return 0;
}




LRESULT window_procedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (g_gui_ctx && g_gui_ctx->win_proc(hwnd, uMsg, wParam, lParam))
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