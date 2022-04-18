#pragma once
#include <queue>
#include <set>

#include "DXIBufferMemPool.h"

/*
	A memory pool using suballocation with a fixed-sized element pool allocation strategy (element size supplied on construction time)

	Creating views are delegated to higher level modules.
	This memory pool only has the responsibility of distributing fixed-sized suballocations.

	========================== Note that constant buffers require element sizes which are a MULTIPLE of 256! ==========================================

	Example uses..:
		- For dynamic and static constant buffer management
			- Pools of 256, 512, 1024
			- Ring-buffer for discard-after-frame, persistent on default heap, immutable

		- Array of structured buffers
			- Light data
			- Material data
				- If bindless, we have indices to different material textures, or indices to buffers (if bindless vertex buffer too)
					- These would be stored on default heap (they persist, until the materials are unloaded)
			- others...

*/

class DXFixedBufferMemPool : public DXIBufferMemPool
{
public:
	DXFixedBufferMemPool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_STATES initial_state);

	std::optional<Allocation> allocate();
	void deallocate(Allocation& alloc);

private:
	cptr<ID3D12Resource> m_constant_buffer;
	uint8_t* m_base_cpu_adr = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_base_gpu_adr{};

	std::set<allocation_id_t> m_allocations_in_use;			// for internal verification (e.g handle free-after-free)
	std::queue<Allocation> m_free_allocations;


};