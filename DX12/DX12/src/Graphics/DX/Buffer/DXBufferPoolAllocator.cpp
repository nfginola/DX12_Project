#include "pch.h"
#include "DXBufferPoolAllocator.h"

DXBufferPoolAllocator::DXBufferPoolAllocator(Microsoft::WRL::ComPtr<ID3D12Device> dev, std::initializer_list<DXBufferPoolAllocator::PoolInfo> pool_infos_list, D3D12_HEAP_TYPE heap_type) :
	m_dev(dev)
{
	std::vector<PoolInfo> pool_infos{ pool_infos_list.begin(), pool_infos_list.end() };

	for (auto& pool_info : pool_infos)
		for (uint32_t i = 0; i < pool_info.num_pools; ++i)
			init_pool(dev.Get(), pool_info.element_size, pool_info.num_elements, heap_type);
}

DXBufferAllocation DXBufferPoolAllocator::allocate(uint64_t requested_size)
{
	DXBufferAllocation alloc{};
	DXBufferMemPool* pool_used = nullptr;
	for (auto& pool : m_pools)
	{
		if (pool->get_allocation_size() < requested_size)
			continue;

		alloc = std::move(pool->allocate());
		if (alloc.size() != 0)
		{
			pool_used = pool.get();
			break;
		}
	}

	// couldn't find any suitable memory after going through all pools.. crash
	if (alloc.size() == 0)
		assert(false);
	
	m_active_alloc_to_pool.insert({ alloc.gpu_adr() , pool_used });
	return alloc;
}

void DXBufferPoolAllocator::deallocate(DXBufferAllocation&& alloc)
{
	auto it = m_active_alloc_to_pool.find(alloc.gpu_adr());
	if (it == m_active_alloc_to_pool.cend())
		assert(false);

	auto& [_, pool] = *it;
	pool->deallocate(std::move(alloc));

	m_active_alloc_to_pool.erase(it);
}

void DXBufferPoolAllocator::set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl)
{
	for (auto& pool : m_pools)
		pool->set_state(new_state, cmdl);
}

void DXBufferPoolAllocator::init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type)
{
	auto pool = std::make_unique<DXBufferMemPool>(dev, element_size, num_elements, heap_type);
	m_pools.push_back(std::move(pool));
}
