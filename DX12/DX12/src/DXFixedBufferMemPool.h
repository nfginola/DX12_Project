#pragma once
#include <d3d12.h>
#include <queue>
#include <set>
#include <optional>

/*
	A memory pool using suballocation with a fixed-sized element pool allocation strategy (element size supplied on construction time)

	Creating views are delegated to higher level modules.
	This memory pool only has the responsibility of distributing fixed-sized suballocations.

	========================== Note that constant buffers require element sizes which are a MULTIPLE of 256! ==========================================

	Example uses..:
		- For dynamic and static constant buffer management
			- Pools of 256, 512, 1024
			- Ring-buffer, persistent on default heap, immutable

		- Array of structured buffers
			- Light data
			- Material data
				- If bindless, we have indices to different material textures, or indices to buffers (if bindless vertex buffer too)
					- These would be stored on default heap (they persist, until the materials are unloaded)
			- others...

*/

class DXFixedBufferMemPool
{
public:
	using allocation_id_t = uint32_t;

	class Allocation
	{
	public:
		ID3D12Resource* get_base_buffer() const { return m_base_buffer; }
		uint32_t get_offset_from_base() const { return m_offset_from_base; }
		uint32_t get_size() const { return m_size; }
		D3D12_GPU_VIRTUAL_ADDRESS get_gpu_adr() const { return m_gpu_address; }

		bool mappable() { return m_mapped_memory != nullptr; }
		uint8_t* get_mapped_memory() const { return m_mapped_memory; }

	private:
		friend class DXFixedBufferMemPool;

		// For internal verification
		allocation_id_t m_allocation_id = 0;

		// For copying
		ID3D12Resource* m_base_buffer;						// Non-owning ptr to base buffer. ComPtr takes care of cleaning up, this doesnt have to worry (verify with DXGI Debug Print on application exit)
		uint32_t m_offset_from_base = 0;
		uint32_t m_size = 0;

		uint8_t* m_mapped_memory = nullptr;					// CPU update
		D3D12_GPU_VIRTUAL_ADDRESS m_gpu_address{};			// GPU address to bind as immediate root argument
	};

public:
	DXFixedBufferMemPool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_STATES initial_state);

	std::optional<Allocation> allocate();
	void deallocate(Allocation& alloc);

private:
	cptr<ID3D12DescriptorHeap> m_descriptors;		// Non-shader visible descriptor heap!

	cptr<ID3D12Resource> m_constant_buffer;
	uint8_t* m_base_cpu_adr = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_base_gpu_adr{};

	std::set<allocation_id_t> m_allocations_in_use;			//	to verify that we are deallocating something that we should (e.g covering for free-after-free)
	std::queue<Allocation> m_free_allocations;


};