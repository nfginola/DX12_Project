#include "pch.h"
#include "DXBufferMemPool.h"
#include "d3dx12.h"

DXBufferMemPool::DXBufferMemPool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type)
{
	assert(element_size % 256 == 0);
	const auto handle_size = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Create buffer
	D3D12_HEAP_PROPERTIES h_prop{};
	h_prop.Type = heap_type;

	D3D12_RESOURCE_DESC r_desc{};
	r_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	r_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	r_desc.Width = num_elements * element_size;
	r_desc.Height = r_desc.DepthOrArraySize = r_desc.MipLevels = 1;
	r_desc.Format = DXGI_FORMAT_UNKNOWN;
	r_desc.SampleDesc = { 1, 0 };
	r_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;		// Requirement for bufers
	r_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	auto hr = dev->CreateCommittedResource(
		&h_prop,
		D3D12_HEAP_FLAG_NONE,
		&r_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_constant_buffer.GetAddressOf()));

	if (FAILED(hr))
		assert(false);

	// Persistently map upload bufer
	D3D12_RANGE no_read{};
	m_constant_buffer->Map(0, &no_read, (void**)&m_base_cpu_adr);
	m_base_gpu_adr = m_constant_buffer->GetGPUVirtualAddress();

	// Initialize descriptors and allocations
	for (uint64_t alloc_id = 0; alloc_id < num_elements; ++alloc_id)
	{
		DXBufferMemPool::Allocation allocation{};
		allocation.allocation_id = alloc_id;
		allocation.mapped_memory = m_base_cpu_adr + alloc_id * element_size;
		allocation.gpu_address = m_base_gpu_adr + alloc_id * element_size;

		allocation.base_buffer = m_constant_buffer.Get();
		allocation.offset_from_base = alloc_id * element_size;
		allocation.size = element_size;

		m_free_allocations.push(allocation);
	}
}

std::optional<DXBufferMemPool::Allocation> DXBufferMemPool::allocate()
{
	if (m_free_allocations.empty())
		return {};

	auto new_allocation = m_free_allocations.front();
	m_allocations_in_use.insert(new_allocation.allocation_id);
	m_free_allocations.pop();

	return new_allocation;
}

void DXBufferMemPool::deallocate(Allocation& alloc)
{
	const auto& alloc_id = alloc.allocation_id;

	auto it = m_allocations_in_use.find(alloc_id);
	if (it == m_allocations_in_use.cend())
		assert(false);		// free-after-free detected

	m_allocations_in_use.erase(alloc_id);
	m_free_allocations.push(alloc);
}


