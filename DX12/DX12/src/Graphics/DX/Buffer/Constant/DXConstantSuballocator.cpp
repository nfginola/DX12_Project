#include "pch.h"
#include "DXConstantSuballocator.h"

DXConstantSuballocator::DXConstantSuballocator(Microsoft::WRL::ComPtr<ID3D12Device> dev, std::initializer_list<DXConstantSuballocator::PoolInfo> pool_infos_list, D3D12_HEAP_TYPE heap_type) :
	m_dev(dev)
{
	std::vector<PoolInfo> pool_infos{ pool_infos_list.begin(), pool_infos_list.end() };
	uint64_t descriptors_needed = 0;
	for (auto& pool_info : pool_infos)
		descriptors_needed += pool_info.num_elements * pool_info.num_pools;

	D3D12_DESCRIPTOR_HEAP_DESC dhd{};
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	dhd.NumDescriptors = (UINT)descriptors_needed;
	auto hr = dev->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(m_desc_heap.GetAddressOf()));
	if (FAILED(hr))
		assert(false);

	// reserve upfront to hinder reallocation and moving of data (invalidates our pointers)
	m_all_allocations.reserve(descriptors_needed);

	// initialize pools
	auto handle = m_desc_heap->GetCPUDescriptorHandleForHeapStart();
	for (auto& pool_info : pool_infos)
		for (uint32_t i = 0; i < pool_info.num_pools; ++i)
			init_pool(dev.Get(), pool_info.element_size, pool_info.num_elements, handle, heap_type);
}

DXConstantSuballocation* DXConstantSuballocator::allocate(uint64_t requested_size)
{
	for (auto i = 0; i < m_pools_and_available_allocations.size(); ++i)
	{
		auto& [pool, available_allocs] = m_pools_and_available_allocations[i];
		if (pool->get_allocation_size() < requested_size)
			continue;	// move on to find another pool which fits size

		auto alloc = available_allocs.front();
		available_allocs.pop();
		return alloc;
	}
	assert(false);		// we'll handle this later (e.g adding a new pool on the fly)
	return nullptr;
}

void DXConstantSuballocator::deallocate(DXConstantSuballocation* alloc)
{
	assert(alloc != nullptr);

	// return memory
	auto& allocations = m_pools_and_available_allocations[alloc->m_pool_idx].second;
	allocations.push(alloc);
}

void DXConstantSuballocator::set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl)
{
	for (auto& pool : m_pools)
		pool->set_state(new_state, cmdl);
}


void DXConstantSuballocator::init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_CPU_DESCRIPTOR_HANDLE& base_handle, D3D12_HEAP_TYPE heap_type)
{
	auto pool = std::make_unique<DXBufferMemPool>(dev, element_size, num_elements, D3D12_HEAP_TYPE_UPLOAD);
	auto handle_size = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// initialize new pool + container for allocations entry
	m_pools_and_available_allocations.push_back({ pool.get(), std::queue<DXConstantSuballocation*>() });
	auto& [_, available_allocs] = m_pools_and_available_allocations.back();

	// create descriptor for each allocation and fill free allocations
	for (uint32_t i = 0; i < num_elements; ++i)
	{
		auto buf_suballoc = pool->allocate();
		if (!buf_suballoc)
			assert(false);

		// grab desc handle
		auto hdl = base_handle;
		hdl.ptr += (uint64_t)i * handle_size;

		// create view
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbd{};
		cbd.BufferLocation = buf_suballoc->get_gpu_adr();
		cbd.SizeInBytes = buf_suballoc->get_size();
		dev->CreateConstantBufferView(&cbd, hdl);

		// init allocation
		DXConstantSuballocation constant_suballoc{};
		constant_suballoc.m_frame_idx = -1;
		constant_suballoc.m_memory = buf_suballoc;
		constant_suballoc.m_valid = false;
		constant_suballoc.m_cpu_descriptor = hdl;
		constant_suballoc.m_pool_idx = (uint8_t)m_pools_and_available_allocations.size() - 1;

		// store allocation internally
		m_all_allocations.push_back(constant_suballoc);

		// work with pointers when distributing
		available_allocs.push(&m_all_allocations.back());
	}

	m_pools.push_back(std::move(pool));

	base_handle.ptr += (uint64_t)num_elements * handle_size;
}