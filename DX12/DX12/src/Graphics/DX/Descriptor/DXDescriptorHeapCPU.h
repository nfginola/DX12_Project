#pragma once
#include "DXDescriptorPool.h"

/*
	Allocates descriptor heap chunks with a pool of non-shader visible descriptor heaps
*/
class DXDescriptorHeapCPU
{
public:
	DXDescriptorHeapCPU(cptr<ID3D12Device> dev, D3D12_DESCRIPTOR_HEAP_TYPE type);
	~DXDescriptorHeapCPU() = default;

	DXDescriptorAllocation allocate(uint32_t num_descriptors);
	void deallocate(DXDescriptorAllocation&& alloc);

private:
	cptr<ID3D12Device> m_dev;
	D3D12_DESCRIPTOR_HEAP_TYPE m_type;
	std::vector<DXDescriptorPool> m_pools;

	std::unordered_map<uint64_t, DXDescriptorPool*> m_allocation_to_pool;

	// keep track of allocations by tagging CPU_DESCRIPTOR_PTR to pool index
};

