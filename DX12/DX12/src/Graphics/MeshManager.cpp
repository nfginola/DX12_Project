#include "pch.h"
#include "MeshManager.h"

MeshManager::MeshManager(cptr<ID3D12Device> dev, DXBufferManager* buf_mgr, uint32_t max_FIF) :
	m_buf_mgr(buf_mgr),
	m_max_FIF(max_FIF)
{

	auto hr = dev.As(&m_dxr_dev);
	if (FAILED(hr))
		assert(false);
}

MeshHandle MeshManager::create_mesh(const MeshDesc& desc)
{
	auto [handle, res] = m_handles.get_next_free_handle();
	
	assert(!desc.pos.empty());
	assert(!desc.uv.empty());
	assert(!desc.indices.empty());

	// position
	DXBufferDesc bdesc{};
	bdesc.data = desc.pos.data;
	bdesc.data_size = desc.pos.total_size;
	bdesc.element_count = desc.pos.count;
	bdesc.element_size = desc.pos.stride;
	bdesc.flag = BufferFlag::eNonConstant;
	bdesc.usage_cpu = UsageIntentCPU::eUpdateNever;
	bdesc.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
	res->vbs.push_back(m_buf_mgr->create_buffer(bdesc));

	// uv
	bdesc.data = desc.uv.data;
	bdesc.data_size = desc.uv.total_size;
	bdesc.element_count = desc.uv.count;
	bdesc.element_size = desc.uv.stride;
	res->vbs.push_back(m_buf_mgr->create_buffer(bdesc));

	// indices
	bdesc.data = desc.indices.data;
	bdesc.data_size = desc.indices.total_size;
	bdesc.element_count = desc.indices.count;
	bdesc.element_size = desc.indices.stride;
	res->ib = m_buf_mgr->create_buffer(bdesc);

	// normals
	bdesc.data = desc.normals.data;
	bdesc.data_size = desc.normals.total_size;
	bdesc.element_count = desc.normals.count;
	bdesc.element_size = desc.tangents.stride;
	res->vbs.push_back(m_buf_mgr->create_buffer(bdesc));

	// tangent
	bdesc.data = desc.tangents.data;
	bdesc.data_size = desc.tangents.total_size;
	bdesc.element_count = desc.tangents.count;
	bdesc.element_size = desc.tangents.stride;
	res->vbs.push_back(m_buf_mgr->create_buffer(bdesc));

	// bitangent
	bdesc.data = desc.bitangents.data;
	bdesc.data_size = desc.bitangents.total_size;
	bdesc.element_count = desc.bitangents.count;
	bdesc.element_size = desc.bitangents.stride;
	res->vbs.push_back(m_buf_mgr->create_buffer(bdesc));

	res->parts = desc.subsets;

	return MeshHandle(handle);
}

void MeshManager::destroy_mesh(MeshHandle handle)
{
	// we are not destroying anything during the frame, we'll just rely on destructor cleanup
	assert(false);
}

const Mesh* MeshManager::get_mesh(MeshHandle handle)
{
	return m_handles.get_resource(handle.handle);
}


void MeshManager::create_RT_accel_structure_v3(const std::vector<RTMeshDesc>& descs, RTBuildSetting setting, UINT submesh_per_BLAS)
{
	if (m_frames_until_del > 0)		// forbid recreation for a bit to simplify resource handling
		return;

	if (m_tlas_element)
	{
		m_tlas_to_delete = std::move(m_tlas_element);
		m_frames_until_del = m_max_FIF;			// wait until full loop to guarantee all res off flight before clearing the GPU mem
	}

	m_tlas_element = std::make_unique<TLASElement>();

	// Assemble Geometry descs for BLAS
	for (auto& desc : descs)
	{
		if (setting == RTBuildSetting::eBLASPerModel)
		{
			const auto& mesh_data = m_handles.get_resource(desc.mesh.handle);
			const auto& vb = m_buf_mgr->get_buffer_alloc(mesh_data->vbs[0]);		// assuming pos is always 0:th
			const auto& ib = m_buf_mgr->get_buffer_alloc(mesh_data->ib);

			auto element = std::make_unique<BLASElement>();		// BLAS per model
			for (const auto& part : mesh_data->parts)
			{
				element->geoms.push_back({});

				D3D12_RAYTRACING_GEOMETRY_DESC& geom_desc = element->geoms.back();
				geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;						// assuming always opaque

				geom_desc.Triangles.IndexBuffer = ib->gpu_adr() + part.index_start * ib->element_size();	// index offset
				geom_desc.Triangles.IndexCount = part.index_count;
				geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;						// assuming R32

				geom_desc.Triangles.Transform3x4 = 0;

				geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				geom_desc.Triangles.VertexCount = vb->element_count() - part.vertex_start;
				geom_desc.Triangles.VertexBuffer.StartAddress = vb->gpu_adr() + part.vertex_start * vb->element_size();	// vertex offset
				geom_desc.Triangles.VertexBuffer.StrideInBytes = vb->element_size();
			}
			element->wm = desc.world_mat;
			m_tlas_element->blas_elements.push_back(std::move(element));
		}
		else if (setting == RTBuildSetting::eBLASPerSubmesh)
		{
			const auto& mesh_data = m_handles.get_resource(desc.mesh.handle);
			const auto& vb = m_buf_mgr->get_buffer_alloc(mesh_data->vbs[0]);		// assuming pos is always 0:th
			const auto& ib = m_buf_mgr->get_buffer_alloc(mesh_data->ib);

			for (const auto& part : mesh_data->parts)
			{
				auto element = std::make_unique<BLASElement>();		// BLAS per submesh

				element->geoms.push_back({});

				D3D12_RAYTRACING_GEOMETRY_DESC& geom_desc = element->geoms.back();
				geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;						// assuming always opaque

				geom_desc.Triangles.IndexBuffer = ib->gpu_adr() + part.index_start * ib->element_size();	// index offset
				geom_desc.Triangles.IndexCount = part.index_count;
				geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;						// assuming R32

				geom_desc.Triangles.Transform3x4 = 0;

				geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				geom_desc.Triangles.VertexCount = vb->element_count() - part.vertex_start;
				geom_desc.Triangles.VertexBuffer.StartAddress = vb->gpu_adr() + part.vertex_start * vb->element_size();	// vertex offset
				geom_desc.Triangles.VertexBuffer.StrideInBytes = vb->element_size();

				element->wm = desc.world_mat;
				m_tlas_element->blas_elements.push_back(std::move(element));
			}
		}
		else if (setting == RTBuildSetting::eBLASVariableSubmesh)
		{
			const auto& mesh_data = m_handles.get_resource(desc.mesh.handle);
			const auto& vb = m_buf_mgr->get_buffer_alloc(mesh_data->vbs[0]);		// assuming pos is always 0:th
			const auto& ib = m_buf_mgr->get_buffer_alloc(mesh_data->ib);

			UINT& steps = submesh_per_BLAS;
			UINT count = 0;


			std::unique_ptr<BLASElement> element;
			for (const auto& part : mesh_data->parts)
			{
				if (count % steps == 0)
				{
					if (element)
					{
						m_tlas_element->blas_elements.push_back(std::move(element));
						element = nullptr;
					}
					element = std::make_unique<BLASElement>();		// BLAS per submesh
				}

				element->geoms.push_back({});

				D3D12_RAYTRACING_GEOMETRY_DESC& geom_desc = element->geoms.back();
				geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;						// assuming always opaque

				geom_desc.Triangles.IndexBuffer = ib->gpu_adr() + part.index_start * ib->element_size();	// index offset
				geom_desc.Triangles.IndexCount = part.index_count;
				geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;						// assuming R32

				geom_desc.Triangles.Transform3x4 = 0;

				geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				geom_desc.Triangles.VertexCount = vb->element_count() - part.vertex_start;
				geom_desc.Triangles.VertexBuffer.StartAddress = vb->gpu_adr() + part.vertex_start * vb->element_size();	// vertex offset
				geom_desc.Triangles.VertexBuffer.StrideInBytes = vb->element_size();

				element->wm = desc.world_mat;

				++count;
			}
			// push last one
			if (element)
				m_tlas_element->blas_elements.push_back(std::move(element));
		}

	}

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	// Fill BLAS 
	for (auto& blas_el : m_tlas_element->blas_elements)
	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bl_in = blas_el->blas_desc.Inputs;
		bl_in.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bl_in.Flags = flags;
		bl_in.NumDescs = (UINT)blas_el->geoms.size();
		bl_in.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bl_in.pGeometryDescs = blas_el->geoms.data();

		m_dxr_dev->GetRaytracingAccelerationStructurePrebuildInfo(&bl_in, &blas_el->preb_info);
		assert(blas_el->preb_info.ResultDataMaxSizeInBytes > 0);

		// grab scratch per BLAS
		DXBufferDesc scratch_d{};
		scratch_d.element_count = 1;
		scratch_d.element_size = (uint32_t)(blas_el->preb_info.ScratchDataSizeInBytes);
		scratch_d.flag = BufferFlag::eNonConstant;
		scratch_d.usage_cpu = UsageIntentCPU::eUpdateNever;
		scratch_d.usage_gpu = UsageIntentGPU::eWrite;
		blas_el->scratch_buffer = m_buf_mgr->create_buffer(scratch_d);		// normal UAV

		// grab BLAS buffer
		DXBufferDesc blas_buf_d{};
		blas_buf_d.element_count = 1;
		blas_buf_d.element_size = (uint32_t)blas_el->preb_info.ResultDataMaxSizeInBytes;
		blas_buf_d.flag = BufferFlag::eNonConstant;
		blas_buf_d.usage_cpu = UsageIntentCPU::eUpdateNever;
		blas_buf_d.usage_gpu = UsageIntentGPU::eWrite;
		blas_buf_d.is_rt_structure = true;								// raytracing UAV
		blas_el->blas_buffer = m_buf_mgr->create_buffer(blas_buf_d);

		// finish BLAS desc
		auto scratch = m_buf_mgr->get_buffer_alloc(blas_el->scratch_buffer);
		auto blas = m_buf_mgr->get_buffer_alloc(blas_el->blas_buffer);
		blas_el->blas_desc.ScratchAccelerationStructureData = scratch->gpu_adr();
		blas_el->blas_desc.DestAccelerationStructureData = blas->gpu_adr();
	}

	// create instance data and connect per BLAS
	for (auto& blas_el : m_tlas_element->blas_elements)
	{
		auto blas = m_buf_mgr->get_buffer_alloc(blas_el->blas_buffer);

		D3D12_RAYTRACING_INSTANCE_DESC instance_d{};
		// scaling
		instance_d.Transform[0][0] = descs[0].world_mat(0, 0);
		instance_d.Transform[1][1] = descs[0].world_mat(1, 1);
		instance_d.Transform[2][2] = descs[0].world_mat(2, 2);

		// translation
		instance_d.Transform[0][3] = descs[0].world_mat(3, 0);
		instance_d.Transform[1][3] = descs[0].world_mat(3, 1);
		instance_d.Transform[2][3] = descs[0].world_mat(3, 2);

		instance_d.InstanceMask = 0xFF;
		instance_d.AccelerationStructure = blas->gpu_adr();
		m_tlas_element->instance_d.push_back(instance_d);

	}

	// setup TLAS
	{
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tl_preb_info = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& tl_in = m_tlas_element->tlas_desc.Inputs;
		tl_in.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		tl_in.Flags = flags;
		tl_in.NumDescs = m_tlas_element->instance_d.size();		// num instances for this TLAS
		tl_in.pGeometryDescs = nullptr;
		tl_in.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		// Query build info
		m_dxr_dev->GetRaytracingAccelerationStructurePrebuildInfo(&tl_in, &tl_preb_info);
		assert(tl_preb_info.ResultDataMaxSizeInBytes > 0);

		/// temp
		UINT64 max = 0;
		for (const auto& blas_el : m_tlas_element->blas_elements)
			max = (std::max)(max, blas_el->preb_info.ScratchDataSizeInBytes);

		// grab a scratch buffer
		DXBufferDesc scratch_d{};
		scratch_d.element_count = 1;
		scratch_d.element_size = (UINT)(std::max)(tl_preb_info.ScratchDataSizeInBytes, max);		// assuming they arent both used concurrently?
		scratch_d.flag = BufferFlag::eNonConstant;
		scratch_d.usage_cpu = UsageIntentCPU::eUpdateNever;
		scratch_d.usage_gpu = UsageIntentGPU::eWrite;
		m_tlas_element->single_scratch = m_buf_mgr->create_buffer(scratch_d);		// normal UAV

		// grab tlas buffer
		DXBufferDesc tlas_buf_d{};
		tlas_buf_d.element_count = 1;
		tlas_buf_d.element_size = (UINT)tl_preb_info.ResultDataMaxSizeInBytes;
		tlas_buf_d.flag = BufferFlag::eNonConstant;
		tlas_buf_d.usage_cpu = UsageIntentCPU::eUpdateNever;
		tlas_buf_d.usage_gpu = UsageIntentGPU::eWrite;
		tlas_buf_d.is_rt_structure = true;								// raytracing UAV
		m_tlas_element->single_tlas = m_buf_mgr->create_buffer(tlas_buf_d);
	}


	// create instance buffer
	{
		DXBufferDesc instance_buf_d{};
		instance_buf_d.data = m_tlas_element->instance_d.data();
		instance_buf_d.data_size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * m_tlas_element->instance_d.size();
		instance_buf_d.element_count = m_tlas_element->instance_d.size();
		instance_buf_d.element_size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
		instance_buf_d.flag = BufferFlag::eNonConstant;
		instance_buf_d.usage_cpu = UsageIntentCPU::eUpdateNever;
		instance_buf_d.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
		m_tlas_element->single_instance = m_buf_mgr->create_buffer(instance_buf_d);
	}

	// finish TLAS desc
	auto scratch = m_buf_mgr->get_buffer_alloc(m_tlas_element->single_scratch);
	auto tlas = m_buf_mgr->get_buffer_alloc(m_tlas_element->single_tlas);
	auto instances = m_buf_mgr->get_buffer_alloc(m_tlas_element->single_instance);
	m_tlas_element->tlas_desc.DestAccelerationStructureData = tlas->gpu_adr();
	m_tlas_element->tlas_desc.ScratchAccelerationStructureData = scratch->gpu_adr();
	m_tlas_element->tlas_desc.Inputs.InstanceDescs = instances->gpu_adr();

	m_rt_bufs.tlas = m_tlas_element->single_tlas;

	m_rt_scene.clear();
	// log RT scene data
	for (size_t i = 0; i < m_tlas_element->blas_elements.size(); ++i)
	{
		const auto& blas = m_tlas_element->blas_elements[i];
		m_rt_scene.geometries_per_blas.push_back(std::to_string(blas->geoms.size()));

		// also log vert count
		for (const auto& geom : blas->geoms)
			m_rt_scene.total_verts += geom.Triangles.VertexCount;
	}

	m_rt_scene.tlas_count = 1;
}

void MeshManager::build_RT_accel_structure(ID3D12GraphicsCommandList5* cmdl)
{
	auto tlas = m_buf_mgr->get_buffer_alloc(m_tlas_element->single_tlas);
	
	std::vector<D3D12_RESOURCE_BARRIER> barrs;
	for (const auto& blas_el : m_tlas_element->blas_elements)
	{
		cmdl->BuildRaytracingAccelerationStructure(&blas_el->blas_desc, 0, nullptr);
		auto blas = m_buf_mgr->get_buffer_alloc(blas_el->blas_buffer);
		barrs.push_back(CD3DX12_RESOURCE_BARRIER::UAV(blas->base_buffer()));
	}
	cmdl->ResourceBarrier(barrs.size(), barrs.data());

	cmdl->BuildRaytracingAccelerationStructure(&m_tlas_element->tlas_desc, 0, nullptr);
	auto new_barr = CD3DX12_RESOURCE_BARRIER::UAV(tlas->base_buffer());
	cmdl->ResourceBarrier(1, &new_barr);
}

const RTAccelStructure* MeshManager::get_RT_accel_structure()
{
	return &m_rt_bufs;
}

const MeshManager::RTSceneData* MeshManager::get_RT_scene_data()
{
	return &m_rt_scene;
}

void MeshManager::frame_begin(uint32_t frame_idx)
{
	if (m_frames_until_del > 0)
	{
		--m_frames_until_del;
		if (m_frames_until_del == 0)
		{
			m_tlas_to_delete->clear_resources(m_buf_mgr);
			m_tlas_to_delete.reset();
		}
	}
}
