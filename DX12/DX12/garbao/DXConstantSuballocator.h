#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>
#include <memory>
#include <tuple>
#include <queue>
#include <initializer_list>
#include "DXConstantSuballocation.h"
#include "DXBufferMemPool.h"

/*
	Helper utility structure to avoid code duplication
	This utility is inteded to setup the data structures used for distributing suballocations.

	Maybe make this a template?
		Make DXConstantSuballocation a Template type

		Use template overloading to make initializers for them (otherwise problematic with init_pool)



*/
struct DXConstantSuballocator
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

	DXConstantSuballocation* allocate(uint32_t requested_size);
	void deallocate(DXConstantSuballocation* alloc);

	// Set underlying memory pools resource state for optimal usage (up to the application code)
	void set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl);

private:
	void init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_CPU_DESCRIPTOR_HANDLE& base_handle, D3D12_HEAP_TYPE heap_type);


	// Use template initalization to generalize allocation metadat filling
	struct CommonDetails
	{
		bool valid = false;
		uint32_t frame_idx = -1;
		DXBufferSuballocation* memory;
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor;
		uint8_t pool_idx = -1;
	};

	template <typename T>
	void fill_allocation(T& to_fill, const CommonDetails& details)
	{
		assert(false);
	}

	template<>
	void fill_allocation<DXConstantSuballocation>(DXConstantSuballocation& to_fill, const CommonDetails& details)
	{
		to_fill.m_frame_idx = details.frame_idx;
		to_fill.m_memory = details.memory;
		to_fill.m_valid = details.valid;
		to_fill.m_cpu_descriptor = details.cpu_descriptor;
		to_fill.m_pool_idx = details.pool_idx;
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_dev;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_desc_heap;		// Non shader visible descriptor heaps with CBVs
	std::vector<std::unique_ptr<DXBufferMemPool>> m_pools;

	std::vector<DXConstantSuballocation> m_all_allocations;			// Storage for all allocations
	pools_and_allocations_t m_pools_and_available_allocations;
};


