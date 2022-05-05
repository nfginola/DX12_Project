#include "pch.h"

// https://devblogs.microsoft.com/directx/directx12agility/
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 602; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8"..\\..\\DX12\\vendor\\AgilitySDK\\build\\native\\bin\\x64\\"; }


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

#include "Graphics/MeshManager.h"
#include "Graphics/ModelManager.h"

#include "Camera/FPCController.h"
#include "Camera/FPPCamera.h"



static bool g_app_running = false;
Input* g_input = nullptr;
GUIContext* g_gui_ctx = nullptr;

LRESULT window_procedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int main()
{
	g_app_running = true;
#if defined(_DEBUGWITHOUTVALIDATIONLAYER)
	constexpr auto debug_on = false;
#elif defined(_DEBUG)
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
		auto shader_compiler = std::make_unique<DXCompiler>(ShaderModel::e6_6);

		// grab LL modules for creating DX12 primitives
		auto dev = gfx_ctx->get_dev();
		auto dq = gfx_ctx->get_direct_queue();

		const auto& max_FIF = gfx_sc->get_settings().max_FIF;

		DXDescriptorHeapCPU cpu_rtv_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		DXDescriptorHeapCPU cpu_dsv_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		DXDescriptorHeapGPU gpu_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 50000);
		DXDescriptorHeapGPU gpu_dheap_sampler(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2040);

		// setup profiler
		dev->SetStablePowerState(false);
		uint8_t pf_latency = max_FIF;
		GPUProfiler gpu_pf(dev, GPUProfiler::QueueType::eDirectOrCompute, pf_latency);
		CPUProfiler cpu_pf(pf_latency);

		// setup imgui
		auto imgui_alloc = gpu_dheap.allocate_static(1);
		g_gui_ctx = new GUIContext(win->get_hwnd(), dev, max_FIF,
			gpu_dheap.get_desc_heap(),
			imgui_alloc.cpu_handle(),		// slot 0 reserved for ImGUI
			imgui_alloc.gpu_handle());

		// Create camera
		auto cam = std::make_unique<FPPCamera>(90.f, (float)CLIENT_WIDTH / CLIENT_HEIGHT, 0.1f, 2000.f);
		auto cam_zoom = std::make_unique<FPPCamera>(28.f, (float)CLIENT_WIDTH / CLIENT_HEIGHT, 0.1f, 2000.f);		// Zoomed in secondary camera

		// Create a First-Person Camera Controller and attach a First-Person Perspective camera
		auto cam_ctrl = std::make_unique<FPCController>(g_input, g_gui_ctx);
		cam_ctrl->set_camera(cam.get());
		cam_ctrl->set_secondary_camera(cam_zoom.get());

		// allocate gpu visible desc heaps for bindless + dynamic descriptor access managemenet
		auto bindless_part = gpu_dheap.allocate_static(5000);

		// setup various managers
		DXBufferManager buf_mgr(dev, max_FIF);
		DXUploadContext up_ctx(dev, &buf_mgr, max_FIF);
		DXTextureManager tex_mgr(dev, dq);
		DXBindlessManager bindless_mgr(dev, std::move(bindless_part), &buf_mgr, &tex_mgr);
		MeshManager mesh_mgr(&buf_mgr);
		ModelManager model_mgr(&mesh_mgr, &tex_mgr, &bindless_mgr);

		struct PerFrameResource
		{
			DXFence sync;
			UINT prev_surface_idx = 0;
			cptr<ID3D12CommandAllocator> dq_ator;
			cptr<ID3D12GraphicsCommandList> dq_cmdl;
		};

		// create sync primitives
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

		// setup backbuffer render targets
		auto rtv_alloc = cpu_rtv_dheap.allocate(max_FIF);
		for (auto i = 0; i < max_FIF; ++i)
		{
			auto hdl = rtv_alloc.cpu_handle(i);
			dev->CreateRenderTargetView(gfx_sc->get_backbuffer(i), nullptr, hdl);
		}

		// setup depth buffers
		std::vector<cptr<ID3D12Resource>> depth_targets;
		depth_targets.resize(max_FIF);
		auto dsv_alloc = cpu_dsv_dheap.allocate(max_FIF);
		{
			// create textures
			const auto format = DXGI_FORMAT_D32_FLOAT;

			D3D12_RESOURCE_DESC resd{};
			resd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resd.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resd.Width = CLIENT_WIDTH;
			resd.Height = CLIENT_HEIGHT;
			resd.DepthOrArraySize = 1;
			resd.MipLevels = 1;
			resd.Format = format;
			resd.SampleDesc.Count = 1;
			resd.SampleDesc.Quality = 0;

			// Driver chooses most efficient layout
			// https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_texture_layout
			resd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

			D3D12_HEAP_PROPERTIES heap_prop{};
			heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_CLEAR_VALUE clear{};
			clear.Format = format;
			clear.DepthStencil.Depth = 1.0f;

			for (auto i = 0; i < max_FIF; ++i)
			{
				dev->CreateCommittedResource(
					&heap_prop,
					D3D12_HEAP_FLAG_NONE,
					&resd,
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					&clear,
					IID_PPV_ARGS(depth_targets[i].GetAddressOf()));

				auto hdl = dsv_alloc.cpu_handle(i);
				dev->CreateDepthStencilView(depth_targets[i].Get(), nullptr, hdl);
			}
		}

		auto main_vp = CD3DX12_VIEWPORT(0.f, 0.f, CLIENT_WIDTH, CLIENT_HEIGHT, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH);
		auto main_scissor = CD3DX12_RECT(0, 0, CLIENT_WIDTH, CLIENT_HEIGHT);

		// setup test UI
		bool show_pf = true;
		bool copy_bogus_data = false;
		bool instanced = false;
		bool instanced_grid = false;
		g_gui_ctx->add_persistent_ui("test", [&]()
			{
				ImGui::Begin("HelloBox");
				ImGui::Checkbox("Show Profiler Data", &show_pf);
				ImGui::Checkbox("Copy Bogus Data", &copy_bogus_data);
				ImGui::Checkbox("Instanced", &instanced);
				ImGui::Checkbox("Instanced Grid", &instanced_grid);
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
			});


		// setup pipeline
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

			// bindless diffuse
			D3D12_DESCRIPTOR_RANGE view_range{};
			view_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			view_range.NumDescriptors = -1;
			view_range.BaseShaderRegister = 0;
			view_range.RegisterSpace = 3;
			view_range.OffsetInDescriptorsFromTableStart = 0;

			// setup rootsig
			rsig = RootSigBuilder()
				.push_constant(7, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL, &params["bindless_index"])
				.push_constant(8, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX, &params["vert_offset"])

				.push_cbv(0, 0, D3D12_SHADER_VISIBILITY_VERTEX, &params["per_object"])

				.push_srv(0, 5, D3D12_SHADER_VISIBILITY_VERTEX, &params["my_pos"])
				.push_srv(1, 5, D3D12_SHADER_VISIBILITY_VERTEX, &params["my_uv"])
				.push_srv(2, 5, D3D12_SHADER_VISIBILITY_VERTEX, &params["my_normal"])
				.push_srv(3, 5, D3D12_SHADER_VISIBILITY_VERTEX, &params["my_tangent"])
				.push_srv(4, 5, D3D12_SHADER_VISIBILITY_VERTEX, &params["my_bitangent"])

				.push_cbv(7, 7, D3D12_SHADER_VISIBILITY_VERTEX, &params["camera_data"])
				.push_table({ samp_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["my_samp"])
				.push_table({ view_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["bindless_views"])
				.build(dev, D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);						// use dynamic descriptor indexing

			auto ds = DepthStencilDescBuilder()
				.set_depth_enabled(true);

			auto bd = BlendDescBuilder()
				.SetAlphaToCoverage(true);

			pipe = PipelineBuilder()
				.set_root_sig(rsig)
				.set_shader_bytecode(*vs_blob, ShaderType::eVertex)
				.set_shader_bytecode(*ps_blob, ShaderType::ePixel)
				.append_rt_format(DXGI_FORMAT_R8G8B8A8_UNORM)
				.set_depth_format(DepthFormat::eD32)
				.set_depth_stencil(ds)
				.set_blend(bd)
				.build(dev);
		}


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
			
		// load sponza
		ModelDesc modeld{};
		//modeld.rel_path = "models/sponza/sponza.obj";
		modeld.rel_path = "models/Sponza_gltf/glTF/Sponza.gltf";

		modeld.pso = pipe;		// 'material'
		auto sponza_model = model_mgr.load_model(modeld);


		// camera (persistent, on default heap)
		InterOp_CameraData cam_data{};
		auto view = DirectX::XMMatrixLookAtLH({ 0.f, 0.f, -2.f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });
		auto proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.f), (float)CLIENT_WIDTH / CLIENT_HEIGHT, 0.1f, 3000.f);
		cam_data.view_mat = DirectX::SimpleMath::Matrix(view);
		cam_data.proj_mat = DirectX::SimpleMath::Matrix(proj);

		DXBufferDesc cdb{};
		cdb.data = &cam_data;
		cdb.data_size = sizeof(InterOp_CameraData);
		cdb.element_count = 1;
		cdb.element_size = cdb.data_size;
		cdb.flag = BufferFlag::eConstant;
		cdb.usage_cpu = UsageIntentCPU::eUpdateSometimes;
		cdb.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
		auto cam_buf = buf_mgr.create_buffer(cdb);

		// object cbuf (ring buffered dynamic)
		DXBufferDesc dyn_cbd{};
		dyn_cbd.element_count = 1;
		dyn_cbd.element_size = sizeof(DirectX::XMFLOAT4X4);
		dyn_cbd.flag = BufferFlag::eConstant;
		dyn_cbd.usage_cpu = UsageIntentCPU::eUpdateOnce;
		dyn_cbd.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
		auto dyn_cb = buf_mgr.create_buffer(dyn_cbd);

		/*
			do bogus copies to give work to async copy
		*/
		static constexpr uint32_t num_pbufs = 3000;
		static constexpr uint32_t payload_size = 1024;
		std::vector<BufferHandle> persistent_bufs;
		persistent_bufs.reserve(num_pbufs);
		for (uint32_t i = 0; i < num_pbufs; ++i)
		{
			DXBufferDesc pbdesc{};
			pbdesc.element_count = 1;
			pbdesc.element_size = payload_size;
			pbdesc.flag = BufferFlag::eConstant;
			pbdesc.usage_cpu = UsageIntentCPU::eUpdateSometimes;
			pbdesc.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
			persistent_bufs.push_back(buf_mgr.create_buffer(pbdesc));
		}

		void* some_data = std::calloc(payload_size, 1);


		double prev_dt = 0.0;
		Stopwatch frame_stopwatch;
		uint64_t frame_count = 0;
		MSG msg{};
		while (g_app_running)
		{
			frame_stopwatch.start();

			++frame_count;
			win->pump_messages();
			if (!g_app_running)		// Early exit if WMs picked up by this frames pump messages
				break;

			g_input->frame_begin();

			auto surface_idx = gfx_sc->get_curr_draw_surface();
			auto& frame_res = per_frame_res[surface_idx];
			auto curr_bb = gfx_sc->get_backbuffer(surface_idx);
			auto dq_ator = frame_res.dq_ator.Get();
			auto dq_cmdl = frame_res.dq_cmdl.Get();


			cpu_pf.frame_begin();
			// CPU side updataes

			cam_ctrl->update(prev_dt);



			// Waiting for main Graphics frame in flight
			frame_res.sync.wait();

			// use copy queue
			up_ctx.frame_begin(surface_idx);

			// buffer copy work on async copy
			InterOp_CameraData cam_dat{};
			cam_dat.view_mat = cam_ctrl->get_active_camera()->get_view_mat();
			cam_dat.proj_mat = cam_ctrl->get_active_camera()->get_proj_mat();
			up_ctx.upload_data(&cam_dat, sizeof(InterOp_CameraData), cam_buf);

			// buffer extra bogus data for copy async
			if (copy_bogus_data)
			{
				for (const auto& buf : persistent_bufs)
				{
					up_ctx.upload_data(some_data, payload_size, buf);
				}
			}

			// submit buffered work
			up_ctx.submit_work(gfx_ctx->get_next_fence_value());




			// Reset
			dq_ator->Reset();
			dq_cmdl->Reset(dq_ator, nullptr);

			gpu_pf.frame_begin(surface_idx);
			g_gui_ctx->frame_begin();
			gpu_dheap.frame_begin(surface_idx);
			buf_mgr.frame_begin(surface_idx);

			bindless_mgr.frame_begin(surface_idx);

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

			// clear depth
			auto curr_dsv = dsv_alloc.cpu_handle(surface_idx);
			dq_cmdl->ClearDepthStencilView(curr_dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

			gpu_pf.profile_end(dq_cmdl, "clear");

			// set draw target
			dq_cmdl->OMSetRenderTargets(1, &rtv_hdl, false, &curr_dsv);
	
			// set main desc heap

			// https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html
			/*
				SetDescriptorHeaps must be called, passing the corresponding heaps, before a call to SetGraphicsRootSignature or 
				SetComputeRootSignature that uses either CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED or SAMPLER_HEAP_DIRECTLY_INDEXED flags
			*/
			ID3D12DescriptorHeap* dheaps[] = { gpu_dheap.get_desc_heap(), gpu_dheap_sampler.get_desc_heap() };
			dq_cmdl->SetDescriptorHeaps(_countof(dheaps), dheaps);


			// GPU-GPU sync: blocks this frames submission until this frames copies are done
			cpu_pf.profile_begin("waiting for async copy");
			up_ctx.wait_for_async_copy(dq);
			cpu_pf.profile_end("waiting for async copy");


			// main draw
			gpu_pf.profile_begin(dq_cmdl, dq, "main draw setup");
			dq_cmdl->RSSetViewports(1, &main_vp);
			dq_cmdl->RSSetScissorRects(1, &main_scissor);
			dq_cmdl->SetGraphicsRootSignature(rsig.Get());
			dq_cmdl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
			// per frame
			dq_cmdl->SetGraphicsRootDescriptorTable(params["my_samp"], samp_desc.gpu_handle());
			dq_cmdl->SetGraphicsRootDescriptorTable(params["bindless_views"], bindless_mgr.get_views_start());
			buf_mgr.bind_as_direct_arg(dq_cmdl, cam_buf, params["camera_data"], RootArgDest::eGraphics);


			dq_cmdl->SetPipelineState(pipe.Get());
			gpu_pf.profile_end(dq_cmdl, "main draw setup");

			gpu_pf.profile_begin(dq_cmdl, dq, "main draw");


			// draw geometry
			{
				const auto& model = model_mgr.get_model(sponza_model);
				const auto& mats = model->mats;

				auto sponza_mesh = mesh_mgr.get_mesh(model->mesh);
				auto sponza_ibv = buf_mgr.get_ibv(sponza_mesh->ib);

				buf_mgr.bind_as_direct_arg(dq_cmdl, sponza_mesh->vbs[0], params["my_pos"], RootArgDest::eGraphics);
				buf_mgr.bind_as_direct_arg(dq_cmdl, sponza_mesh->vbs[1], params["my_uv"], RootArgDest::eGraphics);
				buf_mgr.bind_as_direct_arg(dq_cmdl, sponza_mesh->vbs[2], params["my_normal"], RootArgDest::eGraphics);
				buf_mgr.bind_as_direct_arg(dq_cmdl, sponza_mesh->vbs[3], params["my_tangent"], RootArgDest::eGraphics);
				buf_mgr.bind_as_direct_arg(dq_cmdl, sponza_mesh->vbs[4], params["my_bitangent"], RootArgDest::eGraphics);
				dq_cmdl->IASetIndexBuffer(&sponza_ibv);
				assert(sponza_mesh->parts.size() == mats.size());
				ID3D12PipelineState* prev_pipe = nullptr;

				int dim = instanced_grid ? 5 : 1;
				for (int i = -dim; i < dim; ++i)
				{
					for (int x = -dim; x < dim; ++x)
					{
						// per object
						auto wm = DirectX::SimpleMath::Matrix::CreateScale(0.07) * DirectX::SimpleMath::Matrix::CreateTranslation(x * 350.f, 0.f, i * 200.f);
						up_ctx.upload_data(&wm, sizeof(wm), dyn_cb);
						buf_mgr.bind_as_direct_arg(dq_cmdl, dyn_cb, params["per_object"], RootArgDest::eGraphics);
						for (int i = 0; i < sponza_mesh->parts.size(); ++i)
						{
							const auto& part = sponza_mesh->parts[i];
							const auto& mat = mats[i];

							if (mat.pso.Get() != prev_pipe)
								dq_cmdl->SetPipelineState(mat.pso.Get());

							// set material arg
							dq_cmdl->SetGraphicsRoot32BitConstant(params["bindless_index"], bindless_mgr.access_index(mat.resource), 0);
							// declare geometry part and draw
							dq_cmdl->SetGraphicsRoot32BitConstant(params["vert_offset"], part.vertex_start, 0);
							dq_cmdl->DrawIndexedInstanced(part.index_count, instanced ? 10 : 1, part.index_start, 0, 0);

							prev_pipe = mat.pso.Get();
						}
					}
				}

			}

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

			frame_stopwatch.stop();
			prev_dt = frame_stopwatch.elapsed();
		}

		// wait for all FIFs before exiting
		for (const auto& frame_res : per_frame_res)
			frame_res.sync.wait();

		delete g_input;
		delete g_gui_ctx;

		std::free(some_data);


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