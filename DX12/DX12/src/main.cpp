#include "pch.h"

// Check for memory leaks
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>

#include "Window.h"
#include "Graphics/DX/DXContext.h"
#include "Graphics/DX/DXSwapChain.h"
#include "Graphics/DX/DXCompiler.h"
#include "Graphics/DX/GUI/GUIContext.h"
#include "WinPixEventRuntime/pix3.h"
#include "Profiler/GPUProfiler.h"
#include "Profiler/CPUProfiler.h"

#include "DXTK/SimpleMath.h"

#include "Graphics/DX/CompiledShaderBlob.h"

#include "Graphics/DX/DXBuilders.h"

#include "Utilities/Input.h"
#include "Utilities/AssimpLoader.h"
#include "Utilities/HandlePool.h"

#include "Graphics/DX/DXBufferManager.h"

#include "shaders/ShaderInterop_Renderer.h"

#include "Graphics/DX/Descriptor/DXDescriptorHeapCPU.h"
#include "Graphics/DX/Descriptor/DXDescriptorHeapGPU.h"

static bool g_app_running = false;
Input* g_input = nullptr;
GUIContext* g_gui_ctx = nullptr;

LRESULT window_procedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int main()
{
	g_app_running = true;
#if defined(_DEBUG)
	constexpr auto debug_on = true;
#else
	constexpr auto debug_on = false;
#endif
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
		auto shader_compiler = std::make_unique<DXCompiler>();

		// grab LL modules for creating DX12 primitives
		auto dev = gfx_ctx->get_dev();
		auto dq = gfx_ctx->get_direct_queue();

		struct SyncRes
		{
		public:
			SyncRes() = default;
			SyncRes(ID3D12Device* dev)
			{
				fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				auto hr = dev->CreateFence(fence_val_to_wait_for, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
				if (FAILED(hr))
					assert(false);
			}

			void signal(ID3D12CommandQueue* queue, UINT sig_val)
			{
				fence_val_to_wait_for = sig_val;
				queue->Signal(
					fence.Get(),
					fence_val_to_wait_for);
			}

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

		private:
			UINT fence_val_to_wait_for = 0;
			cptr<ID3D12Fence> fence;
			HANDLE fence_event;
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
			//res.sync.fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			//ThrowIfFailed(dev->CreateFence(res.sync.fence_val_to_wait_for, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(res.sync.fence.GetAddressOf())), DET_ERR("Failed to create fence"));
			res.sync = SyncRes(dev);


			// init dq ator & cmd list
			constexpr auto q_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			ThrowIfFailed(dev->CreateCommandAllocator(q_type, IID_PPV_ARGS(res.dq_ator.GetAddressOf())), DET_ERR("Failed to create cmd ator"));
			ThrowIfFailed(dev->CreateCommandList(0, q_type, res.dq_ator.Get(), nullptr, IID_PPV_ARGS(res.dq_cmdl.GetAddressOf())), DET_ERR("Failed to create direct cmd list"));
			res.dq_cmdl->Close();
		}

		DXDescriptorHeapCPU cpu_rtv_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		DXDescriptorHeapGPU gpu_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


		// Allocate and use descriptor for RTVs
		auto rtv_alloc = cpu_rtv_dheap.allocate(max_FIF);
		for (auto i = 0; i < max_FIF; ++i)
		{
			auto hdl = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtv_alloc.cpu_handle())
				.Offset(i, rtv_alloc.descriptor_size());
			dev->CreateRenderTargetView(gfx_sc->get_backbuffer(i), nullptr, hdl);
		}

		auto main_vp = CD3DX12_VIEWPORT(0.f, 0.f, CLIENT_WIDTH, CLIENT_HEIGHT, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH);
		auto main_scissor = CD3DX12_RECT(0, 0, CLIENT_WIDTH, CLIENT_HEIGHT);



		// Setup profiler
		dev->SetStablePowerState(false);
		uint8_t pf_latency = max_FIF;
		GPUProfiler gpu_pf(dev, GPUProfiler::QueueType::eDirectOrCompute, pf_latency);
		CPUProfiler cpu_pf(pf_latency);

		// Allocate from main gpu dheap for ImGUI
		auto imgui_alloc = gpu_dheap.allocate_static(1);
		g_gui_ctx = new GUIContext(win->get_hwnd(), dev, max_FIF,
			gpu_dheap.get_desc_heap(),
			imgui_alloc.cpu_handle(),		// slot 0 reserved for ImGUI
			imgui_alloc.gpu_handle());

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

		
		DXBufferManager buf_mgr(dev);
		
		struct CBData
		{
			DirectX::XMFLOAT3 color;
		};

		CBData init{};
		init.color = { 1.f, 1.f, 1.f };

		DXBufferDesc bd{};
		bd.data = &init;
		bd.data_size = sizeof(CBData);
		bd.element_count = 1;
		bd.element_size = sizeof(CBData);
		bd.usage_cpu = UsageIntentCPU::eUpdateOnce;			// transient
		bd.usage_gpu = UsageIntentGPU::eReadOncePerFrame; 
		bd.flag = BufferFlag::eConstant;
		auto buf_handle = buf_mgr.create_buffer(bd);

		DXBufferDesc bd2{};
		init.color = { 0.3, 0.8, 0.1 };
		bd2.data = &init;
		bd2.data_size = sizeof(CBData);
		bd2.element_count = 1;
		bd2.element_size = sizeof(CBData);
		bd2.usage_cpu = UsageIntentCPU::eUpdateSometimes;	// persistent
		bd2.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
		bd2.flag = BufferFlag::eConstant;
		auto buf_handle2 = buf_mgr.create_buffer(bd2);



		/*	
			Much easier to NOT suballocate memory for structured buffers due to their inherently dynamic element size.
			Rather, make a committed one for each structured buffer and anassociated SRV!

			Same applies to UAV

			We'll take a look into this in more detail (about suballoating here..) during summer!

			DANGER!:

				When allocating persistent constant buffers, we need to make sure the newest copy is updated to all other copies!
				If we have max 3 frames in flight, we will allocate x3 of the requested buffer   ::   [0, 1, 2]

				if we update on 2 --> 0 and 1 are still in flight, but they need to be updated too!
				what to do?

				On update (2):
					- Staging copy to CPU and copy from staging to (2)
					- Schedule a copy from (2) to (0) when (0) is not in flight anymore		ONLY if no updates were requested again for this resource this frame
						Otherwise, discard all and restart process
					- Schedule a copy from (2) to (1) when (1) is not in flight anymore		ONLY if no updates were requested again for this resource this frame

				Should we do on a copy queue?
					Import our UploadContext

					destination = transient_staging.mapped_memory;
					offsetted_buffer_info = transient_staging....

						UploadContext->staging_upload(void* data, size_t size, uint8_t* destination);
						UploadContext->copy_buffer_region(dst..., src...);

						FenceResoure fence_to_wait_for = UploadContext->signal(value);
						// fenceResource contains ID3D12Fence*, fence value to wait for and the WIN event!
						
						// called prior to draw call (just like mentioned below in BufferManager.sync())
						FenceResource.wait(graphicsQueue);

				We can just schedule a copy at frame_begin to keep it simple
				and if there was a new update request, it would mean THAT copy above + a CPU to staging and staging to device-local (extra copy)
				https://asawicki.info/news_1722_secrets_of_direct3d_12_copies_to_the_same_buffer
				This is actually fine! Since the GPU serializes the copy, meaning we will have the latest copy request data up :)

				We should have some:
				BufferManager.sync();		// Prior to draw call invocations to have spot where we can guarantee buffer copies before usage!
				

		*/


		/*
			Setup quick primtives (pipeline)
			using immediate buffer to test our constant buffer
		
		*/

		cptr<ID3D12RootSignature> rsig;
		cptr<ID3D12PipelineState> pipe;
		std::map<std::string, UINT> params;
		{
			// load shaders
			auto vs_blob = shader_compiler->compile_from_file(
				"shaders/vs.hlsl",
				ShaderType::eVertex,
				L"main");
			auto ps_blob = shader_compiler->compile_from_file(
				"shaders/ps.hlsl",
				ShaderType::ePixel,
				L"main");

			D3D12_DESCRIPTOR_RANGE cbv_range{};
			cbv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			cbv_range.NumDescriptors = 1;

			// setup rootsig
			rsig = RootSigBuilder()
				//.push_table({ cbv_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["my_cbv"])
				.push_cbv(0, 0, D3D12_SHADER_VISIBILITY_PIXEL, &params["my_cbv"])
				.build(dev);

			auto ds = DepthStencilDescBuilder()
				.set_depth_enabled(false);

			pipe = PipelineBuilder()
				.set_root_sig(rsig)
				.set_shader_bytecode(*vs_blob, ShaderType::eVertex)
				.set_shader_bytecode(*ps_blob, ShaderType::ePixel)
				.append_rt_format(DXGI_FORMAT_R8G8B8A8_UNORM)
				.set_depth_stencil(ds)
				.build(dev);
		}


		// testing memory coalescing on disjoint deallocs
		{
			DXDescriptorHeapCPU cpu_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


			std::vector<DXDescriptorAllocation> allocs;
			allocs.push_back(cpu_dheap.allocate(100));
			allocs.push_back(cpu_dheap.allocate(100));
			allocs.push_back(cpu_dheap.allocate(100));
			allocs.push_back(cpu_dheap.allocate(100));
			allocs.push_back(cpu_dheap.allocate(100));

			allocs.push_back(cpu_dheap.allocate(100));
			allocs.push_back(cpu_dheap.allocate(100));
			allocs.push_back(cpu_dheap.allocate(100));
			allocs.push_back(cpu_dheap.allocate(100));
			allocs.push_back(cpu_dheap.allocate(100));

			cpu_dheap.deallocate(std::move(allocs[0]));
			cpu_dheap.deallocate(std::move(allocs[2]));
			cpu_dheap.deallocate(std::move(allocs[4]));

			cpu_dheap.deallocate(std::move(allocs[0 + 5]));
			cpu_dheap.deallocate(std::move(allocs[2 + 5]));

			cpu_dheap.deallocate(std::move(allocs[1]));
			cpu_dheap.deallocate(std::move(allocs[3]));

			cpu_dheap.deallocate(std::move(allocs[4 + 5]));
			cpu_dheap.deallocate(std::move(allocs[1 + 5]));
			cpu_dheap.deallocate(std::move(allocs[3 + 5]));

		}

		{
			std::vector<DXDescriptorAllocation> allocs;
			allocs.push_back(gpu_dheap.allocate_static(30));
			allocs.push_back(gpu_dheap.allocate_static(30));
			allocs.push_back(gpu_dheap.allocate_static(30));
			allocs.push_back(gpu_dheap.allocate_static(30));


			allocs.push_back(gpu_dheap.allocate_dynamic(30));
			allocs.push_back(gpu_dheap.allocate_dynamic(30));
			allocs.push_back(gpu_dheap.allocate_dynamic(30));


			gpu_dheap.deallocate_static(std::move(allocs[0]));
			gpu_dheap.deallocate_static(std::move(allocs[1]));
			gpu_dheap.deallocate_static(std::move(allocs[2]));
			gpu_dheap.deallocate_static(std::move(allocs[3]));

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

			gpu_dheap.begin_frame(surface_idx);

			
			cpu_pf.profile_begin("allocate dynamic descs");
			for (int i = 0; i < 1; ++i)
			{
				gpu_dheap.allocate_dynamic(30);
				gpu_dheap.allocate_dynamic(30);
				gpu_dheap.allocate_dynamic(30);
				gpu_dheap.allocate_dynamic(30);
			}

			cpu_pf.profile_end("allocate dynamic descs");
			
			cpu_pf.profile_begin("buf mgr frame begin");
			buf_mgr.frame_begin(surface_idx);


			std::array<DirectX::SimpleMath::Vector3, 3> colors;
			colors[0] = { 1.f, 0.f, 0.f };
			colors[1] = { 0.f, 1.f, 0.f };
			colors[2] = { 0.f, 0.f, 1.f };

			CBData this_data{};
			this_data.color = colors[surface_idx];

			buf_mgr.upload_data(&this_data, sizeof(CBData), buf_handle);

			cpu_pf.profile_end("buf mgr frame begin");


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
			auto rtv_hdl = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtv_alloc.cpu_handle())
				.Offset(surface_idx, rtv_alloc.descriptor_size());
			FLOAT clear_color[4] = { 0.5f, 0.2f, 0.2f, 1.f };
			dq_cmdl->ClearRenderTargetView(rtv_hdl, clear_color, 1, &main_scissor);
			gpu_pf.profile_end(dq_cmdl, "clear");

			// set draw target
			dq_cmdl->OMSetRenderTargets(1, &rtv_hdl, false, nullptr);
	
			// set main desc heap
			//dq_cmdl->SetDescriptorHeaps(1, gpu_main_dheap.GetAddressOf());
			ID3D12DescriptorHeap* dheaps[] = { gpu_dheap.get_desc_heap() };
			dq_cmdl->SetDescriptorHeaps(1, dheaps);

			// main draw
			gpu_pf.profile_begin(dq_cmdl, dq, "main draw setup");
			dq_cmdl->RSSetViewports(1, &main_vp);
			dq_cmdl->RSSetScissorRects(1, &main_scissor);
			dq_cmdl->SetGraphicsRootSignature(rsig.Get());
			dq_cmdl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
			buf_mgr.bind_as_direct_arg(dq_cmdl, buf_handle, params["my_cbv"], RootArgDest::eGraphics);
			
			dq_cmdl->SetPipelineState(pipe.Get());
			gpu_pf.profile_end(dq_cmdl, "main draw setup");

			gpu_pf.profile_begin(dq_cmdl, dq, "main draw");
			dq_cmdl->DrawInstanced(6, 1, 0, 0);
			gpu_pf.profile_end(dq_cmdl, "main draw");

			// render imgui data
			//dq_cmdl->SetDescriptorHeaps(1, dheap_for_imgui.GetAddressOf());		// We should reserve a single descriptor element on our main render desc heap
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
			frame_res.sync.signal(dq, gfx_ctx->get_next_fence_value());

			//frame_res.sync.fence_val_to_wait_for = gfx_ctx->get_next_fence_value();
			//ThrowIfFailed(dq->Signal(
			//	frame_res.sync.fence.Get(), 
			//	frame_res.sync.fence_val_to_wait_for), 
			//	DET_ERR("Cant add a signal request to queue"));

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