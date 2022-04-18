#pragma once
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

class DXConstantRingBuffer
{
public:



private:
};

