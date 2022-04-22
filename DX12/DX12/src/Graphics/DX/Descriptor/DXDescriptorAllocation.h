#pragma once
#include <d3d12.h>
#include <stdint.h>

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
		uint32_t descriptor_count);

	uint32_t num_descriptors() const { return m_descriptor_count; }
	uint32_t descriptor_size() const { return m_descriptor_size; }

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle() const { return m_cpu_handle; }
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle() const { return m_gpu_handle; }

private:
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpu_handle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_gpu_handle{};

	uint32_t m_descriptor_size = 0;		// 4 types available
	uint32_t m_descriptor_count = 0;	// allocation can represent a group of descriptors (e.g use for table)
};
