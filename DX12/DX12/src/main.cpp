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
		auto shader_compiler = std::make_unique<DXCompiler>(ShaderModel::e6_6);

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
		DXDescriptorHeapCPU cpu_dsv_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
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

				auto hdl = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsv_alloc.cpu_handle())
					.Offset(i, rtv_alloc.descriptor_size());
				dev->CreateDepthStencilView(depth_targets[i].Get(), nullptr, hdl);
			}
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
			DirectX::XMFLOAT3 offset;
		};

		CBData init{};
		init.offset = { 0.f, 0.f, 0.f };

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
		init.offset = { 0.f, 0.f, 0.f };
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
				.push_constant(8, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX, &params["vert_offset"])
				.push_cbv(0, 0, D3D12_SHADER_VISIBILITY_VERTEX, &params["my_cbv"])
				.push_srv(0, 5, D3D12_SHADER_VISIBILITY_VERTEX, &params["my_pos"])
				.push_srv(1, 5, D3D12_SHADER_VISIBILITY_VERTEX, &params["my_uv"])
				.push_cbv(7, 7, D3D12_SHADER_VISIBILITY_VERTEX, &params["camera_data"])
				.push_table({ samp_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["my_samp"])
				.push_table({ view_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["bindless_views"])
				.push_table({ access_range }, D3D12_SHADER_VISIBILITY_PIXEL, &params["bindless_access"])
				.build(dev, D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);						// use dynamic descriptor indexing

			auto ds = DepthStencilDescBuilder()
				.set_depth_enabled(true);

			pipe = PipelineBuilder()
				.set_root_sig(rsig)
				.set_shader_bytecode(*vs_blob, ShaderType::eVertex)
				.set_shader_bytecode(*ps_blob, ShaderType::ePixel)
				.append_rt_format(DXGI_FORMAT_R8G8B8A8_UNORM)
				.set_depth_format(DepthFormat::eD32)
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

		static const VertexPullPosition positions1[] =
		{
			{ { -0.75f, 0.25f, 0.f } },
			{ { 0.25f, -0.75f, 0.f } },
			{ { -0.75f, -0.75f, 0.f } },
			{ { -0.75f, 0.25f, 0.f } },
			{ { 0.25f, 0.25f, 0.f } },
			{ { 0.25f, -0.75f, 0.f } }
		};

		DXBufferDesc vbd{};
		vbd.data = (void*)positions1;
		vbd.data_size = sizeof(positions1);
		vbd.element_count = _countof(positions1);
		vbd.element_size = sizeof(VertexPullPosition);
		vbd.usage_cpu = UsageIntentCPU::eUpdateNever;
		vbd.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
		vbd.flag = BufferFlag::eNonConstant;
		auto vbo1_pos = buf_mgr.create_buffer(vbd);

		static const VertexPullUV uvs1[] =
		{
			{ { 0.f, 0.f } },
			{ { 1.0f, 1.f } },
			{ { 0.f, 1.f } },
			{ { 0.f, 0.f } },
			{ { 1.f, 0.f } },
			{ { 1.0f, 1.f } }
		};
		vbd.data = (void*)uvs1;
		vbd.data_size = sizeof(uvs1);
		vbd.element_count = _countof(uvs1);
		vbd.element_size = sizeof(VertexPullUV);
		auto vbo1_uv = buf_mgr.create_buffer(vbd);




		static const VertexPullPosition positions2[] =
		{
			{ { -0.25f, 0.75f, 0.f } },
			{ { 0.75f, -0.25f, 0.f } },
			{ { -0.25f, -0.25f, 0.f } },
			{ { -0.25f, 0.75f, 0.f } },
			{ { 0.75f, 0.75f, 0.f } },
			{ { 0.75f, -0.25f, 0.f } }
		};
		DXBufferDesc vbd2{};
		vbd2.data = (void*)positions2;
		vbd2.data_size = sizeof(positions2);
		vbd2.element_count = _countof(positions2);
		vbd2.element_size = sizeof(VertexPullPosition);
		vbd2.usage_cpu = UsageIntentCPU::eUpdateNever;
		vbd2.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
		vbd2.flag = BufferFlag::eNonConstant;
		auto vbo2_pos = buf_mgr.create_buffer(vbd2);

		static const VertexPullUV uvs2[] =
		{
			{ { 0.f, 0.f } },
			{ { 1.0f, 1.f } },
			{ { 0.f, 1.f } },
			{ { 0.f, 0.f } },
			{ { 1.f, 0.f } },
			{ { 1.0f, 1.f } }
		};
		vbd2.data = (void*)uvs2;
		vbd2.data_size = sizeof(uvs2);
		vbd2.element_count = _countof(uvs2);
		vbd2.element_size = sizeof(VertexPullUV);
		auto vbo2_uv = buf_mgr.create_buffer(vbd2);

		// notice how our geometry changes depending on the indices!
		// we are applying an index buffer on vertex pulling!
		
		// create index buffer for both quads (testing identical)
		static const uint32_t indices[] =
		{
			0, 1, 2, 0, 4, 1
		};
		DXBufferDesc ibd{};
		ibd.data = (void*)indices;
		ibd.data_size = sizeof(indices);
		ibd.element_count = _countof(indices);
		ibd.element_size = sizeof(uint32_t);
		ibd.usage_cpu = UsageIntentCPU::eUpdateNever;
		ibd.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
		ibd.flag = BufferFlag::eNonConstant;
		auto ibo = buf_mgr.create_buffer(ibd);

		// create index buffer view
		auto ibv = buf_mgr.get_ibv(ibo);

	
		
		struct Material
		{
			cptr<ID3D12PipelineState> pso;
			BindlessHandle resource;
		};



		struct MaterialHandle
		{
		public:
			MaterialHandle() = default;
		private:
			MaterialHandle(uint64_t handle_) : handle(handle_) {}
			uint64_t handle = 0;
		};

		struct Model
		{
			MeshHandle mesh;
			std::vector<MaterialHandle> materials;	// indirect dependency with mesh parts
		};
			
		MeshManager mesh_mgr(&buf_mgr);
		
		MeshHandle sponza_hdl;
		std::vector<Material> mats;
		{
			AssimpLoader loader("models/sponza/sponza.obj");

			// Load mesh
			{
				MeshDesc md{};
				md.pos = utils::MemBlob(
					(void*)loader.get_positions().data(),
					loader.get_positions().size(),
					sizeof(loader.get_positions()[0]));

				md.uv = utils::MemBlob(
					(void*)loader.get_uvs().data(),
					loader.get_uvs().size(),
					sizeof(loader.get_uvs()[0]));

				md.indices = utils::MemBlob(
					(void*)loader.get_indices().data(),
					loader.get_indices().size(),
					sizeof(loader.get_indices()[0]));

				for (const auto& loaded_part : loader.get_meshes())
				{
					MeshPart part{};
					part.index_count = loaded_part.index_count;
					part.index_start = loaded_part.index_start;
					part.vertex_start = loaded_part.vertex_start;
					md.subsets.push_back(part);
				}
				sponza_hdl = mesh_mgr.create_mesh(md);
			}

			// Load Bindless Element
			{
				const auto& loaded_mats = loader.get_materials();
				for (const auto& loaded_mat : loaded_mats)
				{
					// load textures
					const auto& paths = std::get<AssimpMaterialData::PhongPaths>(loaded_mat.file_paths);
					DXTextureDesc td{};
					td.filepath = paths.diffuse;
					td.flag = TextureFlag::eSRGB;
					td.usage_cpu = UsageIntentCPU::eUpdateNever;
					td.usage_gpu = UsageIntentGPU::eReadMultipleTimesPerFrame;
					auto tex = tex_mgr.create_texture(td);

					// assemble bindless element
					// we want to remove duplicates on bindless elements
					DXBindlessDesc bd{};
					bd.diffuse_tex = tex;

					Material mat{};
					mat.resource = bindless_mgr.create_bindless(bd);
					mat.pso = pipe;
					mats.push_back(mat);
				}
			}

			// Assemble material (Pipeline + Bindless Element)
 
			
		}

		


		/*
		
			Try another architecture for Repostories!:
				Keep repositories OUTSIDE and inject them!

			MeshRepository:
				ResourceHandle<Mesh> resources;

			MaterialRepostory
				ResourceHandle<Material> resources;

			In Renderer or somwhere low level..:
				ResourceHandleRepo<Mesh> mesh_repo;
				ResourceHandleRepo<Material> mat_repo;
				--> Re-expose the underlying ResourceHandle interface
				--> This is a good place to implement multithreading safety (e.g multiple reader, multiple writer locks)

				mesh_mgr = make_unique<...>(&mesh_repo);
				mat_mgr = make_unique<...>(&mat_repo);
				

				Render(...):
					
					// do impl stuff
					mesh_repo.get_mesh(...)	
					mat_repo.get_mat(...)


			MeshPart
			{
				index_start = 0;
				index_count = 0;
				vertex_start = 0;
			}

			// Uses non-interleaved format by default
			Mesh
			{
				vector<BufferHandles> vbs		-->		N buffer handles (Vertex, UV, Normal, etc.) 
				BufferHandle ib
				std::vector<MeshPart> parts		-->		Assumes that Mesh has at least one submesh
			}

			Material
			{
				PipelineHandle		--> 
				BindlessHandle		--> Resources (primarily textures but also cbuffers which contain extra material data)
			}
		
			// User constructs this manually
			Model
			{
				std::vector<std::pair<MeshHandle, MaterialHandle>> mesh_material_pairs;
			}

			// Or we can have a helper ModelLoader:
			model = ModelLoader.load_model(filepath);

			--> Ready to be submitted!


			MeshManager.create_mesh(MeshDesc)				--> Passes array of data+size pair + Index Data

			MaterialManager.create_mat(MaterialDesc)

			friends with Renderer
			in Renderer:
				mesh_mgr->get_relevant_res(..)





			submit(model, transform)
				
				for each mesh part in models
					front_renderer->submit(model, transform, render_flags);

		*/




		/*
			Per frame camera things
		*/
		InterOp_CameraData cam_data{};
		auto world = DirectX::XMMatrixScaling(3.f, 2.f, 2.f) * DirectX::XMMatrixTranslation(0.f, 0.f, 0.f);
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



		uint64_t frame_count = 0;
		MSG msg{};
		while (g_app_running)
		{
			++frame_count;
			win->pump_messages();
			if (!g_app_running)		// Early exit if WMs see exit request
				break;

			g_input->frame_begin();

			auto surface_idx = gfx_sc->get_curr_draw_surface();
			auto& frame_res = per_frame_res[surface_idx];
			auto curr_bb = gfx_sc->get_backbuffer(surface_idx);
			auto dq_ator = frame_res.dq_ator.Get();
			auto dq_cmdl = frame_res.dq_cmdl.Get();

			// CPU side updates
			DirectX::SimpleMath::Vector3 offset = { sinf(frame_count / 35.f) * 0.10f, 0.f, 0.f };
			CBData this_data{};
			this_data.offset = offset;

			cpu_pf.frame_begin();






			// GPU side updates
			frame_res.sync.wait();

			// Reset
			dq_ator->Reset();
			dq_cmdl->Reset(dq_ator, nullptr);

			gpu_pf.frame_begin(surface_idx);
			g_gui_ctx->frame_begin();
			gpu_dheap.frame_begin(surface_idx);
			buf_mgr.frame_begin(surface_idx);

			up_ctx.frame_begin(surface_idx);
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

			

			// upload data
			// force direct queue to wait on the GPU for async copy to be done before submitting this frame
			up_ctx.upload_data(&this_data, sizeof(CBData), buf_handle2);
			up_ctx.submit_work(gfx_ctx->get_next_fence_value());

			// GPU-GPU sync: blocks this frames submission until this frames copies are done
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


			// main draw
			gpu_pf.profile_begin(dq_cmdl, dq, "main draw setup");
			dq_cmdl->RSSetViewports(1, &main_vp);
			dq_cmdl->RSSetScissorRects(1, &main_scissor);
			dq_cmdl->SetGraphicsRootSignature(rsig.Get());
			dq_cmdl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		

			buf_mgr.bind_as_direct_arg(dq_cmdl, cam_buf, params["camera_data"], RootArgDest::eGraphics);
			buf_mgr.bind_as_direct_arg(dq_cmdl, buf_handle2, params["my_cbv"], RootArgDest::eGraphics);
			
			
			dq_cmdl->SetGraphicsRootDescriptorTable(params["my_samp"], samp_desc.gpu_handle());
			
			// per frame
			dq_cmdl->SetGraphicsRootDescriptorTable(params["bindless_views"], bindless_mgr.get_views_start());
			dq_cmdl->SetGraphicsRootDescriptorTable(params["bindless_access"], bindless_mgr.get_access_start());

			dq_cmdl->SetPipelineState(pipe.Get());
			gpu_pf.profile_end(dq_cmdl, "main draw setup");

			gpu_pf.profile_begin(dq_cmdl, dq, "main draw");

			// bind geom 1
			buf_mgr.bind_as_direct_arg(dq_cmdl, vbo1_pos, params["my_pos"], RootArgDest::eGraphics);
			buf_mgr.bind_as_direct_arg(dq_cmdl, vbo1_uv, params["my_uv"], RootArgDest::eGraphics);
			dq_cmdl->IASetIndexBuffer(&ibv);

			// bind mat 1
			dq_cmdl->SetGraphicsRoot32BitConstant(params["bindless_index"], bindless_mgr.access_index(bindless_hdl), 0);
			dq_cmdl->DrawIndexedInstanced(buf_mgr.get_element_count(vbo1_pos), 1, 0, 0, 0);

			// bind geom 2
			buf_mgr.bind_as_direct_arg(dq_cmdl, vbo2_pos, params["my_pos"], RootArgDest::eGraphics);
			buf_mgr.bind_as_direct_arg(dq_cmdl, vbo2_uv, params["my_uv"], RootArgDest::eGraphics);
			dq_cmdl->IASetIndexBuffer(&ibv);

			// bind mat 2
			dq_cmdl->SetGraphicsRoot32BitConstant(params["bindless_index"], bindless_mgr.access_index(bindless_hdl2), 0);
			dq_cmdl->DrawIndexedInstanced(buf_mgr.get_element_count(vbo2_pos), 1, 0, 0, 0);


			auto sponza_mesh = mesh_mgr.get_mesh(sponza_hdl);
			auto sponza_ibv = buf_mgr.get_ibv(sponza_mesh->ib);

			buf_mgr.bind_as_direct_arg(dq_cmdl, sponza_mesh->vbs[0], params["my_pos"], RootArgDest::eGraphics);
			buf_mgr.bind_as_direct_arg(dq_cmdl, sponza_mesh->vbs[1], params["my_uv"], RootArgDest::eGraphics);
			dq_cmdl->IASetIndexBuffer(&sponza_ibv);
			// bind and draw
			assert(sponza_mesh->parts.size() == mats.size());
			ID3D12PipelineState* prev_pipe = nullptr;
			for (int i = 0; i < sponza_mesh->parts.size(); ++i)
			{
				const auto& part = sponza_mesh->parts[i];

				if (mats[i].pso.Get() != prev_pipe)
					dq_cmdl->SetPipelineState(mats[i].pso.Get());
				dq_cmdl->SetGraphicsRoot32BitConstant(params["bindless_index"], bindless_mgr.access_index(mats[i].resource), 0);
				dq_cmdl->SetGraphicsRoot32BitConstant(params["vert_offset"], part.vertex_start, 0);
				dq_cmdl->DrawIndexedInstanced(part.index_count, 1, part.index_start, 0, 0);

				prev_pipe = mats[i].pso.Get();
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