#pragma once
#include "Graphics/DX/DXCommon.h"

#include "DXDescriptorAllocation.h"

#include <list>


/*
	Allocates in a free-list fashion and also does memory coalescing.
	Using small pools is recommended so that we dont have too large chunks to iterate over when deallocating!
	The list is sorted by GPU address.

	Using in place vector yields about 3x speedup in comparison to using std::list (checked in Release mode)
*/
class DXDescriptorPool
{
public:
	DXDescriptorPool(cptr<ID3D12Device> dev, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t max_descriptors, bool gpu_visible);
	
	// Can also act as a suballocator to a specific allocation!
	// Here, we 'steal' ownership of the allocation from another pool
	DXDescriptorPool(cptr<ID3D12Device> dev, DXDescriptorAllocation&& alloc, D3D12_DESCRIPTOR_HEAP_TYPE type);

	~DXDescriptorPool() = default;

	DXDescriptorAllocation allocate(uint32_t num_requested_descriptors);
	void deallocate(DXDescriptorAllocation&& alloc);		// experimentation: force use of move semantics so calling app is fully aware that the allocation they are holding is invalid afterwards!

	ID3D12DescriptorHeap* get_desc_heap() const;

private:
	struct DescriptorChunk
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_start;
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_start;
		uint32_t num_descriptors = 0;
	};

private:
	cptr<ID3D12Device> m_dev;
	cptr<ID3D12DescriptorHeap> m_desc_heap;
	D3D12_DESCRIPTOR_HEAP_TYPE m_heap_type;
	bool m_gpu_visible = false;
	uint32_t m_max_descriptors = 0;
	uint32_t m_handle_size = 0;

	//D3D12_CPU_DESCRIPTOR_HANDLE m_curr_cpu_unallocated_start;
	//D3D12_GPU_DESCRIPTOR_HANDLE m_curr_gpu_unallocated_start;

	//std::list<DescriptorChunk> m_free_chunks;

	//size_t m_used_indices = 0;
	std::vector<DescriptorChunk> m_free_chunks2;
};

