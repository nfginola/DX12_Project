#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>
#include <memory>
#include <tuple>
#include <queue>
#include <initializer_list>
#include "DXBufferMemPool.h"
#include "DXConstantSuballocation.h"

/*
	Helper utility structure to avoid code duplication
	This utility is inteded to setup the data structures used for distributing suballocations.


	We should make this a Base class instead! 
	And then inherit..

	DXBufferSuballocator has the primary functionalities

		DXConstBufSuballocator for constant data						: public DXBufferSuballocator
		DXShaderBufSuballocator for shader resource						: public DXBufferSuballocator
		DXUnorderedBufSuballocator for unordered access access			: public DXBufferSuballocator

*/

struct PoolInfo
{
	uint32_t num_pools = 0;
	uint32_t element_size = 0;
	uint32_t num_elements = 0;

	PoolInfo(uint32_t num_pools_, uint32_t element_size_, uint32_t num_elements_) : num_pools(num_pools_), element_size(element_size_), num_elements(num_elements_) {}
};

template <typename T>
class DXBufferSuballocator
{
public:
	using pools_and_allocations_t = std::vector<std::pair<DXBufferMemPool*, std::queue<T*>>>;

public:
	DXBufferSuballocator(Microsoft::WRL::ComPtr<ID3D12Device> dev, std::initializer_list<PoolInfo> pool_infos_list, D3D12_HEAP_TYPE heap_type);
	~DXBufferSuballocator() = default;

	T* allocate(uint32_t requested_size);
	void deallocate(T* alloc);

	// Set underlying memory pools resource state for optimal usage (up to the application code)
	void set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl);

private:
	void init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_CPU_DESCRIPTOR_HANDLE& base_handle, D3D12_HEAP_TYPE heap_type);

	// Template specialization to tackle some known use cases for the Buffer suballocator
	struct CommonDetails
	{
		bool valid = false;
		uint32_t frame_idx = UINT32_MAX;
		DXBufferSuballocation* memory;
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor;
		uint8_t pool_idx = UINT8_MAX;
	};

	template <typename T>
	void fill_allocation(T& to_fill, const CommonDetails& details) { assert(false); }

	template <>
	void fill_allocation<DXConstantSuballocation>(DXConstantSuballocation& to_fill, const CommonDetails& details)
	{
		to_fill.m_frame_idx = details.frame_idx;
		to_fill.m_memory = details.memory;
		to_fill.m_valid = details.valid;
		to_fill.m_cpu_descriptor = details.cpu_descriptor;
		to_fill.m_pool_idx = details.pool_idx;
	}

	template <typename T>
	void deallocation_postprocess(T* to_process) { assert(false); }

	template <>
	void deallocation_postprocess<DXConstantSuballocation>(DXConstantSuballocation* to_fill)
	{
		// handle invalidated
		to_fill->m_valid = false;
	}

	template <typename T>
	void create_descriptor(ID3D12Device* dev, DXBufferSuballocation* alloc, D3D12_CPU_DESCRIPTOR_HANDLE handle) { assert(false); }

	template <>
	void create_descriptor<DXConstantSuballocation>(ID3D12Device* dev, DXBufferSuballocation* alloc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
	{	
		// create view
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbd{};
		cbd.BufferLocation = alloc->get_gpu_adr();
		cbd.SizeInBytes = alloc->get_size();
		dev->CreateConstantBufferView(&cbd, handle);
	}


private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_dev;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_desc_heap;		// Non shader visible descriptor heaps with CBVs
	std::vector<std::unique_ptr<DXBufferMemPool>> m_pools;

	std::vector<T> m_all_allocations;								// Storage for all allocations
	pools_and_allocations_t m_pools_and_available_allocations;
};

template<typename T>
inline DXBufferSuballocator<T>::DXBufferSuballocator(Microsoft::WRL::ComPtr<ID3D12Device> dev, std::initializer_list<PoolInfo> pool_infos_list, D3D12_HEAP_TYPE heap_type)
{
	std::vector<PoolInfo> pool_infos{ pool_infos_list.begin(), pool_infos_list.end() };
	uint64_t descriptors_needed = 0;
	for (auto& pool_info : pool_infos)
		descriptors_needed += (uint64_t)pool_info.num_elements * pool_info.num_pools;

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

template<typename T>
inline T* DXBufferSuballocator<T>::allocate(uint32_t requested_size)
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

template<typename T>
inline void DXBufferSuballocator<T>::deallocate(T* alloc)
{
	assert(alloc != nullptr);

	// return memory
	auto& allocations = m_pools_and_available_allocations[alloc->m_pool_idx].second;
	deallocation_postprocess(alloc);
	allocations.push(alloc);
}

template<typename T>
inline void DXBufferSuballocator<T>::set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl)
{
	for (auto& pool : m_pools)
		pool->set_state(new_state, cmdl);
}

template<typename T>
inline void DXBufferSuballocator<T>::init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_CPU_DESCRIPTOR_HANDLE& base_handle, D3D12_HEAP_TYPE heap_type)
{
	auto pool = std::make_unique<DXBufferMemPool>(dev, element_size, num_elements, D3D12_HEAP_TYPE_UPLOAD);
	auto handle_size = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// initialize new pool + container for allocations entry
	m_pools_and_available_allocations.push_back({ pool.get(), std::queue<T*>() });
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
		create_descriptor<T>(dev, buf_suballoc, hdl);
		//D3D12_CONSTANT_BUFFER_VIEW_DESC cbd{};
		//cbd.BufferLocation = buf_suballoc->get_gpu_adr();
		//cbd.SizeInBytes = buf_suballoc->get_size();
		//dev->CreateConstantBufferView(&cbd, hdl);

		// init allocation
		T suballoc{};
		CommonDetails cd{};
		cd.frame_idx = -1;
		cd.memory = buf_suballoc;
		cd.valid = false;
		cd.cpu_descriptor = hdl;
		cd.pool_idx = (uint8_t)m_pools_and_available_allocations.size() - 1;
		fill_allocation(suballoc, cd);

		// store allocation internally
		m_all_allocations.push_back(suballoc);

		// work with pointers when distributing
		available_allocs.push(&m_all_allocations.back());
	}

	m_pools.push_back(std::move(pool));

	base_handle.ptr += (uint64_t)num_elements * handle_size;
}
