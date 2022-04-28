#pragma once
#include <d3d12.h>
#include <stdint.h>
#include <assert.h>

/*
	Represents a generic buffer allocation:
		This can be re-used for any type of allocators which has the purpose of distributing buffer memory

*/
class DXBufferAllocation
{
public:
	DXBufferAllocation() = default;	
	DXBufferAllocation(
		ID3D12Resource* base_buffer,
		uint32_t offset_from_base,
		uint32_t total_size,
		uint32_t element_size,
		D3D12_GPU_VIRTUAL_ADDRESS gpu_adr,
		bool is_submanaged,
		uint8_t* mapped_memory) :
		m_base_buffer(base_buffer),
		m_offset_from_base(offset_from_base),
		m_total_size(total_size),
		m_element_size(element_size),
		m_gpu_address(gpu_adr),
		m_is_submanaged(is_submanaged),
		m_mapped_memory(mapped_memory)
	{}

	ID3D12Resource* base_buffer() const { return m_base_buffer.Get(); }
	uint32_t offset_from_base() const { return m_offset_from_base; }
	uint32_t size() const { return m_total_size; }
	uint32_t element_size() const { return m_element_size; };
	D3D12_GPU_VIRTUAL_ADDRESS gpu_adr() const { return m_gpu_address; }

	bool mappable() const { return m_mapped_memory != nullptr; }
	uint8_t* mapped_memory() const { assert(mappable()); return m_mapped_memory; }

	// Identiies whether this allocation belongs to an internal manager handling the resource or not.
	// This identiies whether we are allowed to transition the state of the underlying resource or not, which is the responsibility of 
	// the caller to check prior to using the base buffer to transition
	bool transition_allowed() const { return m_is_submanaged; }

private:
	// For copying
	cptr<ID3D12Resource> m_base_buffer;	
	uint32_t m_offset_from_base = 0;
	uint32_t m_total_size = 0;			
	uint32_t m_element_size = 0;

	uint8_t* m_mapped_memory = nullptr;					// CPU updateable memory (optional)
	D3D12_GPU_VIRTUAL_ADDRESS m_gpu_address{};			// GPU address to bind as immediate root argument

	bool m_is_submanaged = false;
};
