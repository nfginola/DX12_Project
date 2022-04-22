#include "pch.h"
#include "DXBufferRingPoolAllocator.h"

#include "DXBufferPoolAllocator.h"

DXBufferRingPoolAllocator::DXBufferRingPoolAllocator(std::unique_ptr<DXBufferPoolAllocator> pool_allocator) :
	m_allocator(std::move(pool_allocator))
{
	assert(m_allocator != nullptr);
}

void DXBufferRingPoolAllocator::frame_begin(uint32_t frame_idx)
{
	m_frame_idx = frame_idx;
	
	while (!m_allocations_in_use.empty())
	{
		auto& alloc_in_use = m_allocations_in_use.front();
		if (alloc_in_use.frame_idx == frame_idx)
		{
			// remove from internal tracker
			m_allocations_in_use.pop();

			alloc_in_use.is_valid = false;		// invalidate app handle

			m_allocator->deallocate(alloc_in_use.alloc);
		}
		else
			break;
	}
}

DXBufferSuballocation* DXBufferRingPoolAllocator::allocate(uint64_t requested_size)
{
	// grab allocation
	auto alloc = m_allocator->allocate(requested_size);
	assert(alloc != nullptr);

	QueueElement el{};
	el.alloc = alloc;
	el.frame_idx = m_frame_idx;
	el.is_valid = true;

	m_allocations_in_use.push(el);

	return alloc;
}
