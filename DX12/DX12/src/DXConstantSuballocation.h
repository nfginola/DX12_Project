#pragma once
#include "DXBufferSuballocation.h"

class DXConstantSuballocation
{
public:
	bool valid() const { return m_valid; }
	const DXBufferSuballocation* const get_memory() const { assert(valid()); return m_memory; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& get_cpu_descriptor() const { return m_cpu_descriptor; }

private:
	friend class DXConstantRingBuffer;
	friend class DXConstantStaticBuffer;
	friend struct thingy;

private:
	bool m_valid = false;
	uint32_t m_frame_idx = -1;
	DXBufferSuballocation* m_memory;				// DXConstantRingBuffer has a non-owning pointer to a memory allocation (the pool owns it)
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpu_descriptor;	// Descriptor in a non-shader visible descriptor heap 
	uint8_t m_pool_idx = -1;
};
