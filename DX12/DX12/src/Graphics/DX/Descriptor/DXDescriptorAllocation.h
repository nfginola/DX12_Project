#pragma once
#include <d3d12.h>
#include <stdint.h>
#include <assert.h>

/*
	Represents a descriptor allocation (similarly to how DXBufferSuballocation represents a suballocation in a buffer)
*/
class DXDescriptorAllocation
{
public:
	DXDescriptorAllocation() = default;
	DXDescriptorAllocation(
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle,
		uint32_t descriptor_size,
		uint32_t descriptor_count,
		uint32_t offset_from_base)  :
		m_cpu_handle(cpu_handle),
		m_gpu_handle(gpu_handle),
		m_descriptor_size(descriptor_size),
		m_descriptor_count(descriptor_count),
		m_offset_from_base(offset_from_base)
	{
	}

	uint32_t num_descriptors() const { return m_descriptor_count; }
	uint32_t descriptor_size() const { return m_descriptor_size; }
	uint32_t offset_from_base() const { return m_offset_from_base; }

	bool gpu_visible() const { return m_gpu_handle.ptr != 0; }

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle(uint32_t index = 0) const 
	{ 
		assert(index < m_descriptor_count);
		D3D12_CPU_DESCRIPTOR_HANDLE hdl = m_cpu_handle;
		hdl.ptr += m_descriptor_size * index;
		return hdl; 
	}
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle(uint32_t index = 0) const 
	{ 
		assert(gpu_visible()); 
		assert(index < m_descriptor_count);
		D3D12_GPU_DESCRIPTOR_HANDLE hdl = m_gpu_handle;
		hdl.ptr += m_descriptor_size * index;
		return hdl;
	}

private:
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpu_handle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_gpu_handle{};

	uint32_t m_descriptor_size = 0;		// 4 types available
	uint32_t m_descriptor_count = 0;	// allocation can represent a group of descriptors (e.g use for table)

	// Offset from the beginning of the heap this is allocated on
	uint32_t m_offset_from_base = 0;
};

/*
	When we want to do Bindless, we can have a BindlessManager which takes in a DXDescriptorAllocation.
	Specifically, it takes in a Descriptor Allocation which is GPU visible and is from the primary GPU descriptor heap.

	Meaning that we have a Bindless manager for that specific part off the Descriptor Heap!

	DXDescriptorAllocatorCPU		--> Uses DXDescriptorPool (non shader-visible)
		make 4: CBV/SRV/UAV
				SAMPLER
				RTV	
				DSV

	DXDescriptorAllocatorGPU		--> Uses DXDescriptorPool (shader visible)
		make 2: One CBV/SRV/UAV and One Sampler

	These two are thin wrappers, just like DXBufferPoolAllocator and RingPoolAllocator

*/
