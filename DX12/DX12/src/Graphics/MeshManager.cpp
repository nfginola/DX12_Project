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

void MeshManager::create_RT_accel_structure(const std::vector<RTMeshDesc>& descs)
{
	// we only support single model for now
	assert(descs.size() == 1);

	// Assemble Geometry descs for BLAS
	for (auto& desc : descs)
	{
		const auto& mesh_data = m_handles.get_resource(desc.mesh.handle);
		const auto& vb = m_buf_mgr->get_buffer_alloc(mesh_data->vbs[0]);		// assuming pos is always 0:th
		const auto& ib = m_buf_mgr->get_buffer_alloc(mesh_data->ib);

		for (const auto& part : mesh_data->parts)
		{
			D3D12_RAYTRACING_GEOMETRY_DESC geom_desc{};
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

			m_geom_descs.push_back(geom_desc);
		}
	}

	// schedule for deletion
	if (m_rt_bufs.tlas.valid())
	{
		m_old_rt_bufs = m_rt_bufs;
		m_frames_until_del = m_max_FIF;
	}

	// Fill BLAS and TLAS input declarations
	// Query the size needed for the declared BLAS and TLAS respectively
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tl_preb_info = {};
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bl_preb_info = {};
	{
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bl_in = m_blas_d.Inputs;
		bl_in.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bl_in.Flags = flags;
		bl_in.NumDescs = (UINT)m_geom_descs.size();
		bl_in.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bl_in.pGeometryDescs = m_geom_descs.data();

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& tl_in = m_tlas_d.Inputs;
		tl_in.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		tl_in.Flags = flags;
		tl_in.NumDescs = 1;					
		tl_in.pGeometryDescs = nullptr;
		tl_in.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		// Query build info
		m_dxr_dev->GetRaytracingAccelerationStructurePrebuildInfo(&bl_in, &bl_preb_info);
		m_dxr_dev->GetRaytracingAccelerationStructurePrebuildInfo(&tl_in, &tl_preb_info);
		assert(bl_preb_info.ResultDataMaxSizeInBytes > 0);
		assert(tl_preb_info.ResultDataMaxSizeInBytes > 0);
	}
	
	// grab buffers
	{
		// grab a scratch buffer
		DXBufferDesc scratch_d{};
		scratch_d.element_count = 1;
		scratch_d.element_size = (UINT)(std::max)(tl_preb_info.ScratchDataSizeInBytes, bl_preb_info.ScratchDataSizeInBytes);		// assuming they arent both used concurrently?
		scratch_d.flag = BufferFlag::eNonConstant;
		scratch_d.usage_cpu = UsageIntentCPU::eUpdateNever;
		scratch_d.usage_gpu = UsageIntentGPU::eWrite;
		m_rt_bufs.scratch = m_buf_mgr->create_buffer(scratch_d);		// normal UAV

		// grab blas buffer
		DXBufferDesc blas_buf_d{};
		blas_buf_d.element_count = 1;
		blas_buf_d.element_size = (UINT)bl_preb_info.ResultDataMaxSizeInBytes;
		blas_buf_d.flag = BufferFlag::eNonConstant;
		blas_buf_d.usage_cpu = UsageIntentCPU::eUpdateNever;
		blas_buf_d.usage_gpu = UsageIntentGPU::eWrite;
		blas_buf_d.is_rt_structure = true;								// raytracing UAV
		m_rt_bufs.blas = m_buf_mgr->create_buffer(blas_buf_d);

		// grab tlas buffer
		DXBufferDesc tlas_buf_d{};
		tlas_buf_d.element_count = 1;
		tlas_buf_d.element_size = (UINT)tl_preb_info.ResultDataMaxSizeInBytes;
		tlas_buf_d.flag = BufferFlag::eNonConstant;
		tlas_buf_d.usage_cpu = UsageIntentCPU::eUpdateNever;
		tlas_buf_d.usage_gpu = UsageIntentGPU::eWrite;
		tlas_buf_d.is_rt_structure = true;								// raytracing UAV
		m_rt_bufs.tlas = m_buf_mgr->create_buffer(tlas_buf_d);
	}

	// finish accel structures
	{
		auto scratch = m_buf_mgr->get_buffer_alloc(m_rt_bufs.scratch);
		auto blas = m_buf_mgr->get_buffer_alloc(m_rt_bufs.blas);
		auto tlas = m_buf_mgr->get_buffer_alloc(m_rt_bufs.tlas);

		// finish BLAS desc
		m_blas_d.ScratchAccelerationStructureData = scratch->gpu_adr();
		m_blas_d.DestAccelerationStructureData = blas->gpu_adr();

		// make instance buffer
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
		{
			DXBufferDesc instance_buf_d{};
			instance_buf_d.data = &instance_d;
			instance_buf_d.data_size = sizeof(instance_d);
			instance_buf_d.element_count = 1;
			instance_buf_d.element_size = sizeof(instance_d);
			instance_buf_d.flag = BufferFlag::eNonConstant;
			instance_buf_d.usage_cpu = UsageIntentCPU::eUpdateNever;
			instance_buf_d.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
			m_rt_bufs.instances = m_buf_mgr->create_buffer(instance_buf_d);
		}
		auto instances = m_buf_mgr->get_buffer_alloc(m_rt_bufs.instances);

		// finish TLAS desc
		m_tlas_d.DestAccelerationStructureData = tlas->gpu_adr();
		m_tlas_d.ScratchAccelerationStructureData = scratch->gpu_adr();
		m_tlas_d.Inputs.InstanceDescs = instances->gpu_adr();
	}


}

void MeshManager::build_RT_accel_structure(ID3D12GraphicsCommandList5* cmdl)
{
	// instance and scratch buffer can be discarded after building has finished!
	auto scratch = m_buf_mgr->get_buffer_alloc(m_rt_bufs.scratch);
	auto blas = m_buf_mgr->get_buffer_alloc(m_rt_bufs.blas);
	auto tlas = m_buf_mgr->get_buffer_alloc(m_rt_bufs.tlas);

	// build
	//auto extra_barr = CD3DX12_RESOURCE_BARRIER::Transition(scratch->base_buffer(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	//cmdl->ResourceBarrier(1, &extra_barr);

	cmdl->BuildRaytracingAccelerationStructure(&m_blas_d, 0, nullptr);
	auto barr = CD3DX12_RESOURCE_BARRIER::UAV(blas->base_buffer());
	cmdl->ResourceBarrier(1, &barr);
	cmdl->BuildRaytracingAccelerationStructure(&m_tlas_d, 0, nullptr);
	barr = CD3DX12_RESOURCE_BARRIER::UAV(tlas->base_buffer());
	cmdl->ResourceBarrier(1, &barr);

}

const RTAccelStructure* MeshManager::get_RT_accel_structure()
{
	return &m_rt_bufs;
}

void MeshManager::frame_begin(uint32_t frame_idx)
{
	if (m_frames_until_del > 0)
	{
		--m_frames_until_del;
		if (m_frames_until_del == 0)
		{
			m_buf_mgr->destroy_buffer(m_old_rt_bufs.scratch);
			m_buf_mgr->destroy_buffer(m_old_rt_bufs.tlas);
			m_buf_mgr->destroy_buffer(m_old_rt_bufs.blas);
			m_buf_mgr->destroy_buffer(m_old_rt_bufs.instances);
		}
	}
}
