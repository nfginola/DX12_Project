#pragma once
#include <d3d12.h>
/*
	begin frame
		- discard all allocations for this frame idx

	module using DXConstantRingBuffer (e.g BufferManager) does:
		
	UpdateBuffer:

		if (buffer.dynamic && (buffer.update_once_per_frame || buffer.update_multiple_per_frame))
			buffer.res = RingBuffer.get_next_free(alloc_size);

		if (buffer.dynamic && buffer.update_sometimes)
		{
			// assumed that buffer.res is an allocation which is persistent
			auto staging = RingBuffer.get_next_free(alloc_size);
			
			// cpu --> staging then staging --> device-local (on async copy?)
			map_copy(staging, data, size);
			gpu_copy(buffer.res, staging);
		}

	RingBuffer expected to keep pools of 256, 512 and 1024

*/

class DXBufferMemPool;

class DXConstantRingBuffer
{
public:
	DXConstantRingBuffer() = delete;
	DXConstantRingBuffer(cptr<ID3D12Device> dev);

	/*
		// begin frame() 
			// gives ID to all allocations in this frame
	
		get_next_free_memory(size, frame_idx);		
		// size restricted to 1024 max
		// choose pool which is >= requested size
		// ** should we pass a frame_idx here? (it makes interface cluttered, maybe track it internally?
			// or maybe have an internal clock instead?
			// exact frame_idx doesnt matter I think, if we allocated on a 0, we need to wait until 0 comes back again,
				// but 0 has to be connected with the frame in flight..
				// if we assume that it moves up chronologically (e.g 0-1-2-0-1-2) then it makes the job easier as we can internally track!..

		// cleans up resources used up on specific frame
		// end_frame(frame_idx);
			// free_memory(uint64_t frame_idx);
	
	*/

private:
	/*
		We can possibly change the pool scheme in the future if we want (e.g pool of 256 pools to tackle out of memory, etc.)
		We will keep it simple for now
	*/
	cptr<ID3D12Device> m_dev;
	uptr<DXBufferMemPool> m_pool_256, m_pool_512, m_pool_1024;
};

