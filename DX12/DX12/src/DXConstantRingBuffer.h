#pragma once
#include <d3d12.h>
#include <queue>

#include "DXBufferSuballocator.h"


/*
	Ring buffer wrapper around a pool suballocator
	Akin to how std::queue uses std::deque under the hood and simply offers a constrained API
*/

class DXBufferMemPool;
class DXBufferSuballocation;

class DXConstantRingBuffer
{
public:
	DXConstantRingBuffer() = delete;
	DXConstantRingBuffer(cptr<ID3D12Device> dev);
	~DXConstantRingBuffer() = default;

	DXConstantSuballocation* allocate(uint32_t requested_size);	
	
	// specifically called after the frame fence
	void frame_begin(uint32_t frame_idx);

private:
	std::unique_ptr<DXBufferSuballocator<DXConstantSuballocation>> m_suballoc_utils;

	// ring buffer of allocations being used for deallocations
	std::queue<DXConstantSuballocation*> m_allocations_in_use;

	uint32_t m_curr_frame_idx = 0;
};