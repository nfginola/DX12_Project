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
	const UINT CLIENT_WIDTH = 1280;
	const UINT CLIENT_HEIGHT = 720;

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

			// init allocator
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

		MSG msg{};
		while (g_app_running)
		{
			win->pump_messages();

			auto surface_idx = gfx_sc->get_curr_draw_surface();
			auto& frame_res = per_frame_res[surface_idx];
			auto dq_ator = frame_res.dq_ator.Get();
			auto dq_cmdl = frame_res.dq_cmdl.Get();

			// wait for FIF
			frame_res.sync.wait();
			
			// reset
			dq_ator->Reset();
			dq_cmdl->Reset(dq_ator, nullptr);

			// transition
			auto barr_to_rt = CD3DX12_RESOURCE_BARRIER::Transition(gfx_sc->get_backbuffer(surface_idx), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			dq_cmdl->ResourceBarrier(1, &barr_to_rt);

			// clear
			auto rtv_hdl = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtv_dheap->GetCPUDescriptorHandleForHeapStart())
				.Offset(surface_idx, rtv_hdl_size);
			FLOAT clear_color[4] = { 0.5f, 0.2f, 0.2f, 1.f };
			dq_cmdl->ClearRenderTargetView(rtv_hdl, clear_color, 1, &main_scissor);

			// transition
			auto barr_to_present = CD3DX12_RESOURCE_BARRIER::Transition(gfx_sc->get_backbuffer(surface_idx), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
			dq_cmdl->ResourceBarrier(1, &barr_to_present);

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
				DET_ERR("Cant add a signal request on queue"));
			frame_res.prev_surface_idx = surface_idx;
		}

		// wait for all FIFs
		for (const auto& frame_res : per_frame_res)
			frame_res.sync.wait();

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