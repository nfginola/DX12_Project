#pragma once
#include "DXDescriptorAllocation.h"

/*
	Designed to handle the dynamic part of a GPU descriptor heap.
	
	User must promise to only use the allocation during the frame they are allocated in.
	Allocations done on a will automatically be recycled after a loop through FIFs, meaning app allocations are invalidated.
	Hence, holding a dynamic Descriptor Allocation for an arbitrary amount of time (e.g saving it somewhere) is unsafe!

	This is a Ring-buffer with an internal stack per ring buffer 'element'.
	A ring buffer element is in this case a 'frame'.
	For each frame, a stack allocator exists.

*/

class DXDescriptorRingBuffer
{
public:
	// steals existing allocation to manage it in a ring-buffer fashion
	DXDescriptorRingBuffer(DXDescriptorAllocation&& alloc);
	
	void frame_begin(uint32_t frame_idx);
	DXDescriptorAllocation allocate(uint32_t num_descriptors);

private:


};

