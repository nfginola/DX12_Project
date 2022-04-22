#pragma once
#include "DXBufferMemPool.h"
#include <unordered_map>

class DXBufferPoolAllocator
{
public:
	struct PoolInfo
	{
		uint32_t num_pools = 0;
		uint32_t element_size = 0;
		uint32_t num_elements = 0;

		PoolInfo(uint32_t num_pools_, uint32_t element_size_, uint32_t num_elements_) : num_pools(num_pools_), element_size(element_size_), num_elements(num_elements_) {}
	};


public:
	DXBufferPoolAllocator(Microsoft::WRL::ComPtr<ID3D12Device> dev, std::initializer_list<DXBufferPoolAllocator::PoolInfo> pool_infos_list, D3D12_HEAP_TYPE heap_type);
	~DXBufferPoolAllocator() = default;

	DXBufferSuballocation* allocate(uint64_t requested_size);
	void deallocate(DXBufferSuballocation* alloc);

	// Sets state of the underlying buffer
	void set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl);

private:
	void init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type);

	
private:
	cptr<ID3D12Device> m_dev;
	std::vector<std::unique_ptr<DXBufferMemPool>> m_pools;

	std::unordered_map<DXBufferSuballocation*, DXBufferMemPool*> m_active_alloc_to_pool;



};

