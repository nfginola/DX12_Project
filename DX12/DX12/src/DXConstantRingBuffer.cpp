#include "pch.h"
#include "DXConstantRingBuffer.h"
#include "DXBufferMemPool.h"

DXConstantRingBuffer::DXConstantRingBuffer(cptr<ID3D12Device> dev) 
{
	//auto pool_infos = 
	//{
	//		DXConstantSuballocator::PoolInfo(1, 256, 100),
	//		DXConstantSuballocator::PoolInfo(1, 512, 50),
	//		DXConstantSuballocator::PoolInfo(1, 1024, 25),
	//};
	//m_suballoc_utils = std::make_unique<DXConstantSuballocator>(dev, pool_infos, D3D12_HEAP_TYPE_UPLOAD);

	auto pool_infos =
	{
			PoolInfo(1, 256, 100),
			PoolInfo(1, 512, 50),
			PoolInfo(1, 1024, 25),
	};
	m_suballoc_utils = std::make_unique<DXBufferSuballocator<DXConstantSuballocation>>(dev, pool_infos, D3D12_HEAP_TYPE_UPLOAD);
}

void DXConstantRingBuffer::frame_begin(uint32_t frame_idx)
{
	m_curr_frame_idx = frame_idx;
	/*
		Since we are using this a ring buffer, the expected layout of the allocations are (in frame indices):
		
		assuming 3 FIFs:
		4 buffers allocated on frame 0
		3 buffers allocated on frame 1
		3 buffers allocated on frame 2
		1 buffer allocated on frame 3 (idx 0)
		
		[0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, ...]

		No disjoint allocations! This should be ensured by this API given that a queue is used and the nature of asking for a memory chunk from this class
	*/

	// invalidate memory previously used for the frame index and return memory to free list
	while (!m_allocations_in_use.empty())
	{
		auto alloc_in_use = m_allocations_in_use.front();
		if (alloc_in_use->m_frame_idx == frame_idx)
		{
			// remove from internal tracker
			m_allocations_in_use.pop();

			// invalidate handle to block user from accidental re-use
			alloc_in_use->m_valid = false;

			m_suballoc_utils->deallocate(alloc_in_use);
		}
		else
			break;
	}
}

DXConstantSuballocation* DXConstantRingBuffer::allocate(uint32_t requested_size)
{	
	// grab allocation
	auto alloc = m_suballoc_utils->allocate(requested_size);
	assert(alloc != nullptr);	

	// set which frame this alloc is used on
	alloc->m_frame_idx = m_curr_frame_idx;

	// internally track allocation
	m_allocations_in_use.push(alloc);

	return alloc;
}

