#pragma once
#include <d3d12.h>
#include <queue>

#include "Graphics/DX/Buffer/Constant/DXConstantSuballocator.h"


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

	DXConstantSuballocation* allocate(uint64_t requested_size);	
	
	// specifically called after the frame fence
	void frame_begin(uint32_t frame_idx);

private:
	std::unique_ptr<DXConstantSuballocator> m_suballoc_utils;

	// ring buffer of allocations being used for deallocations
	std::queue<DXConstantSuballocation*> m_allocations_in_use;

	/*
		If we instead just have a queue with a struct:
		struct ...
		{
			FrameIdx
			DXBufferSuballocations
		}

		stored internally, that is enough to internally keep track!
		We can still simply return a DXBufferSuballocation.
	
	*/

	uint32_t m_curr_frame_idx = 0;
};