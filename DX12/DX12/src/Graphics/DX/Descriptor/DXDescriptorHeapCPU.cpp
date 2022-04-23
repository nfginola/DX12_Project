#include "pch.h"
#include "DXDescriptorHeapCPU.h"

DXDescriptorHeapCPU::DXDescriptorHeapCPU(cptr<ID3D12Device> dev, D3D12_DESCRIPTOR_HEAP_TYPE type) :
	m_dev(dev),
	m_type(type)
{
	constexpr auto init_num_pools = 5;
	constexpr auto init_desc_per_pool = 500;

	for (int i = 0; i < init_num_pools; ++i)
		m_pools.push_back(DXDescriptorPool(dev, type, init_desc_per_pool, false));
}

DXDescriptorAllocation DXDescriptorHeapCPU::allocate(uint32_t num_descriptors)
{
	for (auto& pool : m_pools)
	{
		auto alloc = pool.allocate(num_descriptors);
		if (alloc.num_descriptors() == 0)
			continue;
		
		// tag this allocation
		m_allocation_to_pool.insert({ alloc.cpu_handle().ptr, &pool });

		return alloc;
	}

	// couldn't find any suitable space in all pools, we wont handle this for now
	assert(false);
	return {};
}

void DXDescriptorHeapCPU::deallocate(DXDescriptorAllocation&& alloc)
{
	auto it = m_allocation_to_pool.find(alloc.cpu_handle().ptr);
	if (it == m_allocation_to_pool.cend())
		assert(false);		// this allocation does not belong to this manager

	// return allocation to pool
	auto& [_, pool] = *it;
	pool->deallocate(std::move(alloc));
}
