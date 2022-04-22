#pragma once
#include <d3d12.h>
#include <stdint.h>
#include <assert.h>

/*
	Represents a generic buffer suballocation:
		This can be re-used for any type of allocators which has the purpose of distributing suballocators.
		For example, the DXBufferMemPool distributes a DXBufferSuballocation in a pool-fashion
*/

// Not guaranteed to be mappable, calling app must check before use
class DXBufferSuballocation
{
public:
	ID3D12Resource* get_base_buffer() const { return m_base_buffer; }
	uint32_t get_offset_from_base() const { return m_offset_from_base; }
	uint32_t get_size() const { return m_size; }
	D3D12_GPU_VIRTUAL_ADDRESS get_gpu_adr() const { return m_gpu_address; }

	bool is_valid() const { return m_is_valid; }

	bool mappable() const { return m_mapped_memory != nullptr; }
	uint8_t* get_mapped_memory() const { assert(mappable()); return m_mapped_memory; }

	uint64_t get_id() { return m_allocation_id; }

private:
	friend class DXBufferMemPool;
	DXBufferSuballocation() = default;								// Protect calling app from constructing a bogus allocation

	// For internal verification
	uint64_t m_allocation_id = 0;

	// For copying
	ID3D12Resource* m_base_buffer = nullptr;					// Non-owning ptr to base buffer. ComPtr takes care of cleaning up, this doesnt have to worry (verify with DXGI Debug Print on application exit)
	uint32_t m_offset_from_base = 0;
	uint32_t m_size = 0;

	uint8_t* m_mapped_memory = nullptr;					// CPU updateable memory (optional)
	D3D12_GPU_VIRTUAL_ADDRESS m_gpu_address{};			// GPU address to bind as immediate root argument

	bool m_is_valid = false;			// Is app-handle valid?
};
