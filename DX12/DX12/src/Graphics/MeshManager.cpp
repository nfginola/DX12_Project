#include "pch.h"
#include "MeshManager.h"

MeshManager::MeshManager(DXBufferManager* buf_mgr) :
	m_buf_mgr(buf_mgr)
{
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

	res->parts = desc.subsets;

	return MeshHandle(handle);
}

void MeshManager::destroy_mesh(MeshHandle handle)
{

}

const Mesh* MeshManager::get_mesh(MeshHandle handle)
{
	return m_handles.get_resource(handle.handle);
}
