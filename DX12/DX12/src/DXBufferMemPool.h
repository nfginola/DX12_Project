#pragma once
#include <d3d12.h>
#include <queue>
#include <set>
#include <optional>

/*
	A memory pool using suballocation with a fixed-sized element allocation strategy (element size supplied on construction time)

	Creating views are delegated to higher level modules.
	This memory pool only has the responsibility of distributing fixed-sized suballocations.
*/

class DXBufferMemPool
{
public:
	using allocation_id_t = uint32_t;

	// Calling app should NOT modify these members, only use them
	// Maybe its wise to put getters and make this struct a friend of DXBufferMemPool (?)
	struct Allocation
	{
		allocation_id_t allocation_id = 0;

		/*
			Metadata
		*/ 

		// For copying
		ID3D12Resource* base_buffer;						// Non-owning ptr to base buffer. ComPtr takes care of cleaning up, this doesnt have to worry (verify with DXGI Debug Print on application exit)
		uint32_t offset_from_base = 0;
		uint32_t size = 0;

		uint8_t* mapped_memory = nullptr;					// CPU update
		D3D12_GPU_VIRTUAL_ADDRESS gpu_address{};			// GPU address to bind as immediate root argument
	};

public:
	DXBufferMemPool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type);

	std::optional<Allocation> allocate();
	void deallocate(Allocation& alloc);

private:
	cptr<ID3D12DescriptorHeap> m_descriptors;		// Non-shader visible descriptor heap!

	cptr<ID3D12Resource> m_constant_buffer;
	uint8_t* m_base_cpu_adr;
	D3D12_GPU_VIRTUAL_ADDRESS m_base_gpu_adr;

	std::set<allocation_id_t> m_allocations_in_use;			//	to verify that we are deallocating something that we should (e.g covering for free-after-free)
	std::queue<Allocation> m_free_allocations;


};