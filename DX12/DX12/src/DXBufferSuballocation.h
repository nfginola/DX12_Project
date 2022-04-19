#pragma once
#include <d3d12.h>
#include <stdint.h>
#include <assert.h>

// Not guaranteed to be mappable, calling app must check before use
class DXBufferSuballocation
{
public:
	ID3D12Resource* get_base_buffer() const { return m_base_buffer; }
	uint32_t get_offset_from_base() const { return m_offset_from_base; }
	uint32_t get_size() const { return m_size; }
	D3D12_GPU_VIRTUAL_ADDRESS get_gpu_adr() const { return m_gpu_address; }

	bool mappable() const { return m_mapped_memory != nullptr; }
	uint8_t* get_mapped_memory() const { assert(mappable()); return m_mapped_memory; }

private:
	friend class DXBufferMemPool;
	DXBufferSuballocation() = default;								// Protect calling app from constructing a bogus allocation

	// For internal verification
	uint64_t m_allocation_id = 0;

	// For copying
	ID3D12Resource* m_base_buffer{};					// Non-owning ptr to base buffer. ComPtr takes care of cleaning up, this doesnt have to worry (verify with DXGI Debug Print on application exit)
	uint32_t m_offset_from_base = 0;
	uint32_t m_size = 0;

	uint8_t* m_mapped_memory = nullptr;					// CPU updateable memory (optional)
	D3D12_GPU_VIRTUAL_ADDRESS m_gpu_address{};			// GPU address to bind as immediate root argument
};