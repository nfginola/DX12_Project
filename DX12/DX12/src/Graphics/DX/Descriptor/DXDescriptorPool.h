#pragma once
#include "Graphics/DX/DXCommon.h"

#include "DXDescriptorAllocation.h"

#include <list>

class DXDescriptorPool
{
public:
	DXDescriptorPool(cptr<ID3D12Device> dev, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t max_descriptors, bool gpu_visible);
	~DXDescriptorPool() = default;

	DXDescriptorAllocation allocate(uint32_t num_requested_descriptors);
	void deallocate(DXDescriptorAllocation&& alloc);


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

	D3D12_CPU_DESCRIPTOR_HANDLE m_curr_cpu_unallocated_start;
	D3D12_GPU_DESCRIPTOR_HANDLE m_curr_gpu_unallocated_start;

	//std::list<DescriptorChunk> m_free_chunks;

	size_t m_used_indices = 0;
	std::vector<DescriptorChunk> m_free_chunks2;
};

