#pragma once
#include <d3d12.h>
#include <queue>
#include <set>
#include <optional>

#include "DXBufferSuballocation.h"

/*
	A memory pool using suballocation with a fixed-sized element pool allocation strategy (element size supplied on construction time)
	https://en.wikipedia.org/wiki/Memory_pool

	Creating views are delegated to higher level modules.
	This memory pool only has the responsibility of distributing fixed-sized suballocations.

	========================== Note that constant buffers require element sizes which are a MULTIPLE of 256! ==========================================

	Example uses..:
		- For dynamic and static constant buffer management
			- Pools of 256, 512, 1024
			- Ring-buffer for discard-after-frame, persistent on default heap, samed-sized immutable (lookups)

		- Array of structured buffers
			- Light data
			- Material data
				- If bindless, we have indices to different material textures, or indices to buffers (if bindless vertex buffer too)
					- These would be stored on default heap (they persist, until the materials are unloaded)
			- others...

*/

class DXBufferMemPool
{
public:
	DXBufferMemPool() = delete;
	DXBufferMemPool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type);
	~DXBufferMemPool() = default;

	// API for retrieving and returning suballocations
	DXBufferSuballocation* allocate();
	void deallocate(DXBufferSuballocation* alloc);

	// Set underlying memory resource state for optimal usage (up to the application code)
	void set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl);

	uint16_t get_allocation_size() const;

private:
	cptr<ID3D12Resource> m_buffer;
	D3D12_RESOURCE_STATES m_curr_state = D3D12_RESOURCE_STATE_COMMON;

	uint8_t* m_base_cpu_adr = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_base_gpu_adr{};
	D3D12_GPU_VIRTUAL_ADDRESS m_end_gpu_adr{};				// Used to verify deallocation

	std::vector<DXBufferSuballocation> m_allocations;
	std::queue<DXBufferSuballocation*> m_free_allocations;
	std::set<uint64_t> m_allocations_in_use;				// for internal verification (e.g handle free-after-free)

	uint16_t m_element_size = 0;


};