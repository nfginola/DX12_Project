#include "pch.h"
#include "DXBindlessManager.h"

DXBindlessManager::DXBindlessManager(
	Microsoft::WRL::ComPtr<ID3D12Device> dev, 
	DXDescriptorAllocation&& bindless_part, 
	DXBufferManager* buf_mgr, 
	DXTextureManager* tex_mgr) :
	m_dev(dev),
	m_buf_mgr(buf_mgr),
	m_tex_mgr(tex_mgr)
{
	// Submanage the bindless part into a range specifically for CBVs (mat access) and the storage for the actual views
	{
		// use the stolen memory for easier internal allocation
		// we allocate a separate chunk for CBVs and a separate chunk for the views
		auto num_descs = bindless_part.num_descriptors();
		auto temp = DXDescriptorPool(dev, std::move(bindless_part), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		assert(num_descs % 2 == 0);

		m_max_elements = num_descs / 2;
		auto views = temp.allocate(m_max_elements);				// Places views first
		auto access_cbvs = temp.allocate(m_max_elements);		// Access CBVs after

		m_views_start = views.gpu_handle();
		m_access_start = access_cbvs.gpu_handle();
		
		// for this implementation, we will be placing the access part right after the views
		m_offset_to_access_part = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;	

		m_view_desc_ator = std::make_unique<DXDescriptorPool>(dev, std::move(access_cbvs), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		m_access_desc_ator = std::make_unique<DXDescriptorPool>(dev, std::move(views), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}

void DXBindlessManager::begin_frame(uint32_t frame_idx)
{
	m_curr_frame_idx = frame_idx;
	while (!m_deletion_queue.empty())
	{
		auto el = m_deletion_queue.front();
		if (el.first == frame_idx)
		{
			auto& del_func = el.second;
			del_func();
			m_deletion_queue.pop();
		}
		else
			break;
	}
}

BindlessHandle DXBindlessManager::create_bindless(const DXBindlessDesc& desc)
{
	/*
	 
		1. Allocate from view heap
		2. Allocate from buf heap (to store mat data)
		3. Create shader view on the allocated view heap
		4. Create constant view on the allocatoed buf heap

		5. Grab handle and fill the internal resource
		6. Return the handle
	*/

	assert(m_curr_max_indices < m_max_elements);

	// Grab index to use in the descriptor heap in both parts respectively
	uint64_t idx_to_use = 0;
	if (!m_used_indices.empty())
	{
		idx_to_use = m_used_indices.front();
		m_used_indices.pop();
	}
	else
	{
		idx_to_use = m_curr_max_indices++;
	}
	
	// Grab handle
	auto [handle, res] = m_handles.get_next_free_handle();

	// Fill bindless metadata
	res->diffuse_index = idx_to_use;
	res->frame_idx_allocation = m_curr_frame_idx; 

	// Fill buffer init data
	BindlessElement bel{};
	bel.diffuse_idx = idx_to_use;

	// Create buffer
	DXBufferDesc bdesc{};
	bdesc.data = &bel;
	bdesc.data_size = sizeof(bel);
	bdesc.element_count = 1;
	bdesc.element_size = sizeof(bel);
	bdesc.flag = BufferFlag::eConstant;
	bdesc.usage_cpu = UsageIntentCPU::eUpdateSometimes;
	bdesc.usage_gpu = UsageIntentGPU::eReadOncePerFrame;
	auto access_hdl = m_buf_mgr->create_buffer(bdesc);

	// Allocate descriptors for the view group (bindless element) and texture
	auto view_desc = m_view_desc_ator->allocate(1);		// just diffuse
	auto access_desc = m_access_desc_ator->allocate(1);	// one persistent per tex group

	// Create views for the resources
	m_buf_mgr->create_view_for(access_hdl, access_desc.cpu_handle());
	m_tex_mgr->create_view_for(desc.diffuse_tex, view_desc.cpu_handle());

	res->access_alloc = std::move(access_desc);
	res->view_alloc = std::move(view_desc);

	return BindlessHandle(handle);
}

void DXBindlessManager::destroy_bindless(BindlessHandle handle)
{
	auto res = m_handles.get_resource(handle.handle);
	auto del_func = [this, res, curr_frame = m_curr_frame_idx]()
	{
		// deallocate descriptors to allow re-use (stomping)
		m_view_desc_ator->deallocate(std::move(res->access_alloc));
		m_access_desc_ator->deallocate(std::move(res->view_alloc));

		// free access buffer
		m_buf_mgr->destroy_buffer(res->access_buf);
		// free bindless group to allow re-use
		m_handles.free_handle(res->handle);
		// re-use indices
		m_used_indices.push(res->diffuse_index);

	};
	m_deletion_queue.push({ m_curr_frame_idx, del_func });
}

D3D12_GPU_DESCRIPTOR_HANDLE DXBindlessManager::get_views_start() const
{
	return m_views_start;
}

D3D12_GPU_DESCRIPTOR_HANDLE DXBindlessManager::get_access_start() const
{
	return m_access_start;
}


uint64_t DXBindlessManager::offset_to_access_part() const
{
	return m_offset_to_access_part;
}

uint64_t DXBindlessManager::index_in_descs(BindlessHandle handle)
{
	return m_handles.get_resource(handle.handle)->diffuse_index;
}
