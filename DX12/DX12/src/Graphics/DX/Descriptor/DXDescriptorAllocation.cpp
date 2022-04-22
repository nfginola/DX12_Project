#include "pch.h"
#include "DXDescriptorAllocation.h"

DXDescriptorAllocation::DXDescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle, uint32_t descriptor_size, uint32_t descriptor_count) :
	m_cpu_handle(cpu_handle),
	m_gpu_handle(gpu_handle),
	m_descriptor_size(descriptor_size),
	m_descriptor_count(descriptor_count)
{
}
