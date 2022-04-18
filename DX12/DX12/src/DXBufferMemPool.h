#pragma once
#include <d3d12.h>
#include <queue>
#include <set>
#include <optional>

/*
	A memory pool using suballocation with a fixed-sized element pool allocation strategy (element size supplied on construction time)
	https://en.wikipedia.org/wiki/Memory_pool

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

class DXBufferMemPool
{
public:
	using allocation_id_t = uint64_t;

	// Not guaranteed to be mappable, calling app must check before use
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
		friend class DXBufferMemPool;
		Allocation() = default;								// protect calling app from constructing a bogus allocation

		// For internal verification
		allocation_id_t m_allocation_id = 0;

		// For copying
		ID3D12Resource* m_base_buffer{};						// Non-owning ptr to base buffer. ComPtr takes care of cleaning up, this doesnt have to worry (verify with DXGI Debug Print on application exit)
		uint32_t m_offset_from_base = 0;
		uint32_t m_size = 0;

		uint8_t* m_mapped_memory = nullptr;					// CPU updateable memory (optional)
		D3D12_GPU_VIRTUAL_ADDRESS m_gpu_address{};			// GPU address to bind as immediate root argument
	};

public:
	DXBufferMemPool() = delete;
	DXBufferMemPool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type);

	// API for suballocations
	std::optional<Allocation> allocate();
	void deallocate(Allocation& alloc);

	// Set underlying memory resource state for optimal usage (up to the calling code)
	void set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl);

private:
	cptr<ID3D12Resource> m_buffer;
	D3D12_RESOURCE_STATES m_curr_state = D3D12_RESOURCE_STATE_COMMON;

	uint8_t* m_base_cpu_adr = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_base_gpu_adr{};
	D3D12_GPU_VIRTUAL_ADDRESS m_end_gpu_adr{};				// Used to verify deallocation

	std::set<allocation_id_t> m_allocations_in_use;			// for internal verification (e.g handle free-after-free)
	std::queue<Allocation> m_free_allocations;


};