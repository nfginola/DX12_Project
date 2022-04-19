#pragma once
#include "DXBufferSuballocator.h"

/*
	Nothing more than a thin wrapper for initializing set pool sizes for a static buffer (resides in device-local memory)
*/
class DXConstantStaticBuffer
{
public:
	DXConstantStaticBuffer() = delete;
	DXConstantStaticBuffer(cptr<ID3D12Device> dev);
	~DXConstantStaticBuffer() = default;

	DXConstantSuballocation* allocate(uint32_t requested_size);
	void deallocate(DXConstantSuballocation* alloc);

private:
	std::unique_ptr<DXBufferSuballocator<DXConstantSuballocation>> m_suballoc_utils;

	// ring buffer of allocations being used for deallocations
	std::queue<DXConstantSuballocation*> m_allocations_in_use;
};

