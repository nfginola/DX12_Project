#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>
#include <memory>
#include <tuple>
#include <queue>
#include <initializer_list>
#include "DXConstantSuballocation.h"
#include "Graphics/DX/Buffer/DXBufferMemPool.h"

/*
	Helper utility structure which acts as a distributor:
		Selects an appropriate pool to allocate from given the requested size

	In addition, this class acts as an intermediary for storing non-shader visible descriptors for every suballocation avalable from all pools.
	Hence, this is a "D3D12-ified pool manager" specifically for constant buffer management

	No interface, since the allocation details are specific and needs to be accessed!
	DXConstantSuballocator returns DXConstantSuballocation with specific details for Constant data management
*/
class DXConstantSuballocator
{
public:
	using pools_and_allocations_t = std::vector<std::pair<DXBufferMemPool*, std::queue<DXConstantSuballocation*>>>;

	struct PoolInfo
	{
		uint32_t num_pools = 0;
		uint32_t element_size = 0;
		uint32_t num_elements = 0;

		PoolInfo(uint32_t num_pools_, uint32_t element_size_, uint32_t num_elements_) : num_pools(num_pools_), element_size(element_size_), num_elements(num_elements_) {}
	};

public:
	DXConstantSuballocator(Microsoft::WRL::ComPtr<ID3D12Device> dev, std::initializer_list<DXConstantSuballocator::PoolInfo> pool_infos_list, D3D12_HEAP_TYPE heap_type);
	~DXConstantSuballocator() = default;

	DXConstantSuballocation* allocate(uint64_t requested_size);
	void deallocate(DXConstantSuballocation* alloc);

	// Set underlying memory pools resource state for optimal usage (up to the application code)
	void set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl);

private:
	void init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_CPU_DESCRIPTOR_HANDLE& base_handle, D3D12_HEAP_TYPE heap_type);

private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_dev;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_desc_heap;		// Non shader visible descriptor heaps with CBVs
	std::vector<std::unique_ptr<DXBufferMemPool>> m_pools;

	std::vector<DXConstantSuballocation> m_all_allocations;			// Storage for all allocations
	pools_and_allocations_t m_pools_and_available_allocations;
};


