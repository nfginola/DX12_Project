#pragma once
#include "DXBufferAllocation.h"

/*

	Represents generic committed resource allocator.
	We only support committed allocations for now

*/

class DXBufferGenericAllocator
{
public:
	DXBufferGenericAllocator(cptr<ID3D12Device> dev, D3D12_HEAP_TYPE type);		// utilize committed

	DXBufferAllocation allocate(uint32_t element_count, uint32_t element_size);
	void deallocate(DXBufferAllocation&& alloc);

private:
	cptr<ID3D12Device> m_dev;
	D3D12_HEAP_TYPE m_heap_type;

	// keps track internally to make d3d12 resource destruction explicit
	std::unordered_map<uint64_t, DXBufferAllocation> m_allocs;


	
	

};

