#pragma once
#include <d3d12.h>
#include <queue>

#include "DXConstantSuballocation.h"
#include "helperthing.h"

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
class DXBufferSuballocation;

struct thingy;
/*
	We want to use the pools to allocate all possible allocations and store them locally in a FIFO fashion just like MemPool
	Difference is that we supply CBVs on a non-shader visible descriptor heap here for each allocation
*/

class DXConstantRingBuffer
{
public:
	DXConstantRingBuffer() = delete;
	DXConstantRingBuffer(cptr<ID3D12Device> dev);
	~DXConstantRingBuffer() = default;

	// Public API
	DXConstantSuballocation* allocate(uint32_t requested_size);	
	
	// specifically called at the beginning of a frame (after frame fence)
	void frame_begin(uint32_t frame_idx);

private:
	uptr<DXBufferMemPool> init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_CPU_DESCRIPTOR_HANDLE& base_handle);

private:
	std::unique_ptr<thingy> m_thingy;

	//using pools_and_allocations_t = std::vector<std::pair<DXBufferMemPool*, std::queue<DXConstantSuballocation*>>>;

	cptr<ID3D12Device> m_dev;


	//cptr<ID3D12DescriptorHeap> m_desc_heap;		// Non shader visible descriptor heaps with CBVs

	//std::vector<DXConstantSuballocation> m_all_allocations;			// Storage for all allocations

	//// Pools used 
	///*
	//	We can possibly change the pool scheme in the future if we want (e.g pool of 256 pools to tackle out of memory, etc.)
	//	We will keep it simple for now and use 3 pools
	//*/
	//std::vector<uptr<DXBufferMemPool>> m_pools;	

	//// Track allocations internally to allow for internal invalidation of allocations held by application
	//std::queue<DXConstantSuballocation*> m_allocations_in_use;
	//
	//// Hold the free allocations for distribution
	//pools_and_allocations_t m_pools_and_available_allocations;

	// Track frame index to identify allocations
	uint32_t m_curr_frame_idx = 0;
};





/*
	Thoughts and brain dump below (ignore)
*/	


/*

	vec <  pair<MemPool, std::queue<Allocation>  >	pools;

	when allocating:
		for (auto i = 0; i < pools.size(); ++i)
		{
			auto& [pool, allocations] = pools[i];
			if (pool < requested_size)
				continue;

			Allocation* alloc = allocations.front();		// allocations contain pointers to allocations stored inside
			allocations.pop();

			m_allocations_in_use.push(alloc);

			return alloc;
		}

	when freeing (e.g on begin frame):
		for (auto& allocation_in_use : m_allocations_in_use)
			allocation_in_use.m_valid = false;				// Invalidate memory from being used




	allocations[pool_size_idx][pool_idx] = allocation from pool(pool_idx, e.g 1) from the pool_size_idx(e.g 512)


	256					512

	pool1				pool1
		alloc1				alloc1
		alloc2			pool2
		alloc3			pool3
	pool2				pool4
	pool3
	pool4

*/


// Allocation* allocate(alloc_size);
/*
	Calling code can only look into stored Allocations:
		Internal allocations are invalidated (e.g ID invalidated) once they have been cleared,
		so if calling-app stores some Allocation which have been invalidated, accessing the Allocations memory pool will assert

		// App can either use get_memory() to bind directly as root arg or use descriptor to bind a proper CBV
		struct Allocation
		{
		public:
			const DXBufferMemPool& get_memory()
			{
				assert(m_valid);
				...
			}

			CPU_DESCRIPTOR_HANDLE get_descriptor();


		private:
			bool m_valid = false;
			uint32_t m_frame_idx = -1;
			DXBufferMemPool::Allocation m_memory;

			// now we know its or a Constant Buffer so we have CBVs on a non-shader visible descriptor heap
			// we supply this so that the calling app can copy this descriptor to shader-visible heap if they wish
			CPU_DESCRIPTOR_HANDLE

			uint32_t mem_pool_id = -1;		// idx into vector of memory pools stored in this class, so we can identify who to return this memory to
		}

		Allocation* alloc = ringBuffer.allocate(size);
		alloc->get_memory()	--> returns the internal DXMemPool Allocation
			inside this function, we check for its validity
x

	*/

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
