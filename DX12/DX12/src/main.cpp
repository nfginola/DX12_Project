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

#include "Graphics/DX/DXUploadContext.h"

#include "Graphics/DX/DXTextureManager.h"

#include "Graphics/DX/DXBindlessManager.h"


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

	// Initialize WIC
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
		assert(false);

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


		struct PerFrameResource
		{
			DXFence sync;
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
			res.sync = DXFence(dev);

			// init dq ator & cmd list
			constexpr auto q_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			ThrowIfFailed(dev->CreateCommandAllocator(q_type, IID_PPV_ARGS(res.dq_ator.GetAddressOf())), DET_ERR("Failed to create cmd ator"));
			ThrowIfFailed(dev->CreateCommandList(0, q_type, res.dq_ator.Get(), nullptr, IID_PPV_ARGS(res.dq_cmdl.GetAddressOf())), DET_ERR("Failed to create direct cmd list"));
			res.dq_cmdl->Close();
		}

		DXDescriptorHeapCPU cpu_rtv_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		DXDescriptorHeapGPU gpu_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 6000);
		DXDescriptorHeapGPU gpu_dheap_sampler(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2000);


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
		//GPUProfiler gpu_pf_copy(dev, GPUProfiler::QueueType::eCopy, pf_latency);
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



		DXBufferManager buf_mgr(dev, max_FIF);
		
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

		// setup texture manager
		DXTextureManager tex_mgr(dev, dq);

		// allocate space for bindless views
		auto bindless_part = gpu_dheap.allocate_static(2000);
		DXBindlessManager bindless_mgr(dev, std::move(bindless_part), &buf_mgr, &tex_mgr);


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

			D3D12_DESCRIPTOR_RANGE samp_range{};
			samp_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			samp_range.NumDescriptors = 1;
			samp_range.BaseShaderRegister = 0;
			samp_range.RegisterSpace = 0;
			samp_range.OffsetInDescriptorsFromTableStart = 0;

			// testing bindless
			D3D12_DESCRIPTOR_RANGE view_range{};
			view_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			view_range.NumDescriptors = -1;
			view_range.BaseShaderRegister = 0;
			view_range.RegisterSpace = 3;
			view_range.OffsetInDescriptorsFromTableStart = 0;

			D3D12_DESCRIPTOR_RANGE access_range{};
			access_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			access_range.NumDescriptors = -1;
			access_range.BaseShaderRegister = 0;
			access_range.RegisterSpace = 3;
			access_range.OffsetInDescriptorsFromTableStart = 0;

			// setup rootsig
			rsig = RootSigBuilder()
				//.push_table({ cbv_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["my_cbv"])
				.push_constant(7, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL, &params["bindless_index"])
				.push_cbv(0, 0, D3D12_SHADER_VISIBILITY_PIXEL, &params["my_cbv"])
				.push_srv(0, 5, D3D12_SHADER_VISIBILITY_VERTEX, &params["my_vertex"])
				.push_table({ samp_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["my_samp"])
				.push_table({ view_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["bindless_views"])
				.push_table({ access_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["bindless_access"])
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

		DXUploadContext up_ctx(dev, &buf_mgr, max_FIF);

		// create dynamic sampler
		auto samp_desc = gpu_dheap_sampler.allocate_static(1);
		D3D12_SAMPLER_DESC sdesc{};
		sdesc.Filter = D3D12_FILTER_ANISOTROPIC;
		sdesc.AddressU = sdesc.AddressV = sdesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sdesc.MipLODBias = 0;
		sdesc.MaxAnisotropy = 8;
		sdesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		sdesc.BorderColor[0] = sdesc.BorderColor[1] = sdesc.BorderColor[2] = sdesc.BorderColor[3] = 1.f;
		sdesc.MinLOD = 0.f;
		sdesc.MaxLOD = D3D12_FLOAT32_MAX;
		dev->CreateSampler(&sdesc, samp_desc.cpu_handle());



		// load texture
		TextureHandle tex_hdl;
		{
			DXTextureDesc tdesc{};
			tdesc.filepath = "textures/hootle.png";
			tdesc.usage_cpu = UsageIntentCPU::eUpdateNever;
			tdesc.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
			tex_hdl = tex_mgr.create_texture(tdesc);
		}
		TextureHandle tex_hdl2;
		{
			DXTextureDesc tdesc{};
			tdesc.filepath = "textures/scenery.jpg";
			tdesc.usage_cpu = UsageIntentCPU::eUpdateNever;
			tdesc.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
			tex_hdl2 = tex_mgr.create_texture(tdesc);
		}

		/*			
			Material
			{
				std::unordered_map<TextureType, TextureHandle> textures;	// holds the resources
				BindlessHandle bindless;									// holds the views to the resources

				// Optionally, a Pipeline
			}
		
		*/


		// allocate bindless primitive
		// mat1
		DXBindlessDesc bind_d{};
		bind_d.diffuse_tex = tex_hdl;
		auto bindless_hdl = bindless_mgr.create_bindless(bind_d);

		// mat2
		bind_d.diffuse_tex = tex_hdl2;
		auto bindless_hdl2 = bindless_mgr.create_bindless(bind_d);

		// make vbuffer
		static const VertexPullElement verts[] =
		{
			{ { -0.5f, 0.5f, 0.f }, { 0.f, 0.f } },
			{ { 0.5f, -0.5f, 0.f }, { 1.0f, 1.f } },
			{ { -0.5f, -0.5f, 0.f }, { 0.f, 1.f } },
			{ { -0.5f, 0.5f, 0.f }, { 0.f, 0.f } },
			{ { 0.5f, 0.5f, 0.f }, { 1.f, 0.f } },
			{ { 0.5f, -0.5f, 0.f }, { 1.0f, 1.f } }
		};
		DXBufferDesc vbd{};
		vbd.data = (void*)verts;
		vbd.data_size = sizeof(verts);
		vbd.element_count = _countof(verts);
		vbd.element_size = sizeof(VertexPullElement);
		vbd.usage_cpu = UsageIntentCPU::eUpdateNever;
		vbd.usage_gpu = UsageIntentGPU::eReadOncePerFrame;	
		vbd.flag = BufferFlag::eNonConstant;
		auto vbo = buf_mgr.create_buffer(vbd);




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
			//gpu_pf_copy.frame_begin(surface_idx);
			g_gui_ctx->frame_begin();

			gpu_dheap.frame_begin(surface_idx);


			cpu_pf.profile_begin("buf mgr frame begin");
			buf_mgr.frame_begin(surface_idx);


			std::array<DirectX::SimpleMath::Vector3, 3> colors;
			colors[0] = { 1.f, 0.f, 0.f };
			colors[1] = { 0.f, 1.f, 0.f };
			colors[2] = { 0.f, 0.f, 1.f };

			CBData this_data{};
			this_data.color = colors[surface_idx];


			cpu_pf.profile_end("buf mgr frame begin");



			if (show_pf)
			{
				// query profiler results
				{
					std::cout << "GPU:\n";
					const auto& profiles = gpu_pf.get_profiles();
					for (const auto& [_, profile] : profiles)
					{
						std::cout << "(" << profile.name << "):\t elapsed in ms: " << profile.sec_elapsed * 1000.0 << "\n";
					}
				}
				//{
				//	std::cout << "GPU Copy:\n";
				//	const auto& profiles = gpu_pf_copy.get_profiles();
				//	for (const auto& [_, profile] : profiles)
				//	{
				//		std::cout << "(" << profile.name << "):\t elapsed in ms: " << profile.sec_elapsed * 1000.0 << "\n";
				//	}
				//}


				std::cout << "\n";

				{
					std::cout << "CPU:\n";
					const auto& cpu_profiles = cpu_pf.get_profiles();
					for (const auto& [_, profile] : cpu_profiles)
					{
						std::cout << "(" << profile.name << "):\t elapsed in ms: " << profile.sec_elapsed * 1000.0 << "\n";
					}
				}

				std::cout << "========================================\n";
			}

			// reset
			dq_ator->Reset();
			dq_cmdl->Reset(dq_ator, nullptr);

			up_ctx.frame_begin(surface_idx);
			bindless_mgr.frame_begin(surface_idx);

		
			// upload data
			// force direct queue to wait on the GPU for async copy to be done before submitting this frame
			up_ctx.upload_data(&this_data, sizeof(CBData), buf_handle2);
			up_ctx.submit_work(gfx_ctx->get_next_fence_value());
			up_ctx.wait_for_async_copy(dq);

			PIXBeginEvent(dq_cmdl, PIX_COLOR(0, 200, 200), "Direct Queue Main");

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
			ID3D12DescriptorHeap* dheaps[] = { gpu_dheap.get_desc_heap(), gpu_dheap_sampler.get_desc_heap() };
			dq_cmdl->SetDescriptorHeaps(_countof(dheaps), dheaps);

			// main draw
			gpu_pf.profile_begin(dq_cmdl, dq, "main draw setup");
			dq_cmdl->RSSetViewports(1, &main_vp);
			dq_cmdl->RSSetScissorRects(1, &main_scissor);
			dq_cmdl->SetGraphicsRootSignature(rsig.Get());
			dq_cmdl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
			buf_mgr.bind_as_direct_arg(dq_cmdl, buf_handle2, params["my_cbv"], RootArgDest::eGraphics);
			dq_cmdl->SetGraphicsRootDescriptorTable(params["my_samp"], samp_desc.gpu_handle());
			
			// per frame
			dq_cmdl->SetGraphicsRootDescriptorTable(params["bindless_views"], bindless_mgr.get_views_start());
			dq_cmdl->SetGraphicsRootDescriptorTable(params["bindless_access"], bindless_mgr.get_access_start());

			dq_cmdl->SetPipelineState(pipe.Get());
			gpu_pf.profile_end(dq_cmdl, "main draw setup");

			gpu_pf.profile_begin(dq_cmdl, dq, "main draw");

			// bind vertex
			buf_mgr.bind_as_direct_arg(dq_cmdl, vbo, params["my_vertex"], RootArgDest::eGraphics);

			// per material
			dq_cmdl->SetGraphicsRoot32BitConstant(params["bindless_index"], bindless_mgr.index_in_descs(bindless_hdl), 0);
			dq_cmdl->DrawInstanced(6, 1, 0, 0);

			dq_cmdl->SetGraphicsRoot32BitConstant(params["bindless_index"], bindless_mgr.index_in_descs(bindless_hdl2), 0);
			dq_cmdl->DrawInstanced(6, 1, 0, 0);


			gpu_pf.profile_end(dq_cmdl, "main draw");

			// render imgui data
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