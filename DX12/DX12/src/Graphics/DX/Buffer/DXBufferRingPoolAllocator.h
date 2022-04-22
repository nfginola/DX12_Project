#pragma once
#include "DXBufferSuballocation.h"
#include <queue>

class DXBufferPoolAllocator;

/*
	A ring-buffered pool allocator
*/
class DXBufferRingPoolAllocator
{
public:
	DXBufferRingPoolAllocator(std::unique_ptr<DXBufferPoolAllocator> pool_allocator);
	~DXBufferRingPoolAllocator() = default;

	void frame_begin(uint32_t frame_idx);

	DXBufferSuballocation* allocate(uint64_t requested_size);

private:
	struct QueueElement
	{
		DXBufferSuballocation* alloc = nullptr;

		// metadata
		uint32_t frame_idx = 0;
		bool is_valid = false;
	};

private:
	std::queue<QueueElement> m_allocations_in_use;
	std::unique_ptr<DXBufferPoolAllocator> m_allocator;
	uint32_t m_frame_idx = 0;

};

