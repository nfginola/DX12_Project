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
class DXBufferAllocation
{
public:
	DXBufferAllocation() = default;	
	DXBufferAllocation(
		ID3D12Resource* base_buffer,
		uint32_t offset_from_base,
		uint32_t size,
		D3D12_GPU_VIRTUAL_ADDRESS gpu_adr,
		bool is_submanaged,
		uint8_t* mapped_memory) :
		m_base_buffer(base_buffer),
		m_offset_from_base(offset_from_base),
		m_size(size),
		m_gpu_address(gpu_adr),
		m_is_submanaged(is_submanaged),
		m_mapped_memory(mapped_memory)
	{}

	ID3D12Resource* base_buffer() const { return m_base_buffer; }
	uint32_t offset_from_base() const { return m_offset_from_base; }
	uint32_t size() const { return m_size; }
	D3D12_GPU_VIRTUAL_ADDRESS gpu_adr() const { return m_gpu_address; }

	bool mappable() const { return m_mapped_memory != nullptr; }
	uint8_t* mapped_memory() const { assert(mappable()); return m_mapped_memory; }

	// Identiies whether this allocation belongs to an internal manager handling the resource or not.
	// This identiies whether we are allowed to transition the state of the underlying resource or not, which is the responsibility of 
	// the caller to check prior to using the base buffer to transition
	bool transition_allowed() const { return m_is_submanaged; }

private:
	// For copying
	ID3D12Resource* m_base_buffer = nullptr;					// Non-owning ptr to base buffer. ComPtr takes care of cleaning up, this doesnt have to worry (verify with DXGI Debug Print on application exit)
	uint32_t m_offset_from_base = 0;
	uint32_t m_size = 0;

	uint8_t* m_mapped_memory = nullptr;					// CPU updateable memory (optional)
	D3D12_GPU_VIRTUAL_ADDRESS m_gpu_address{};			// GPU address to bind as immediate root argument

	bool m_is_submanaged = false;
};
