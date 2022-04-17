#include "pch.h"
#include "DXFixedBufferMemPool.h"
#include "d3dx12.h"

DXFixedBufferMemPool::DXFixedBufferMemPool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_STATES initial_state)
{
	const auto handle_size = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	if (heap_type == D3D12_HEAP_TYPE_UPLOAD)
		assert(initial_state == D3D12_RESOURCE_STATE_GENERIC_READ);		// required start state for upload buffer

	// Create buffer
	D3D12_HEAP_PROPERTIES hp{};
	hp.Type = heap_type;

	D3D12_RESOURCE_DESC rd{};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	rd.Width = (uint64_t)num_elements * element_size;
	rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN;
	rd.SampleDesc = { 1, 0 };
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;		// Requirement for bufers
	rd.Flags = D3D12_RESOURCE_FLAG_NONE;

	auto hr = dev->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&rd,
		initial_state,
		nullptr,
		IID_PPV_ARGS(m_constant_buffer.GetAddressOf()));
	if (FAILED(hr))
		assert(false);
	m_base_gpu_adr = m_constant_buffer->GetGPUVirtualAddress();

	// Persistently map if upload/readback buffer
	if (heap_type == D3D12_HEAP_TYPE_UPLOAD || heap_type == D3D12_HEAP_TYPE_READBACK)
	{
		D3D12_RANGE no_read{};
		m_constant_buffer->Map(0, &no_read, (void**)&m_base_cpu_adr);
	}

	// Initialize sub-allocations
	for (uint32_t alloc_id = 0; alloc_id < num_elements; ++alloc_id)
	{
		DXFixedBufferMemPool::Allocation allocation{};
		allocation.m_allocation_id = alloc_id;
		allocation.m_gpu_address = m_base_gpu_adr + (uint64_t)alloc_id * element_size;
		allocation.m_base_buffer = m_constant_buffer.Get();
		allocation.m_offset_from_base = alloc_id * element_size;
		allocation.m_size = element_size;

		if (m_base_cpu_adr)
			allocation.m_mapped_memory = m_base_cpu_adr + (uint64_t)alloc_id * element_size;

		m_free_allocations.push(allocation);
	}
}

std::optional<DXFixedBufferMemPool::Allocation> DXFixedBufferMemPool::allocate()
{
	if (m_free_allocations.empty())
		return {};

	auto new_allocation = m_free_allocations.front();
	m_allocations_in_use.insert(new_allocation.m_allocation_id);
	m_free_allocations.pop();

	return new_allocation;
}

void DXFixedBufferMemPool::deallocate(Allocation& alloc)
{
	const auto& alloc_id = alloc.m_allocation_id;

	auto it = m_allocations_in_use.find(alloc_id);
	if (it == m_allocations_in_use.cend())
		assert(false);		// free-after-free detected

	m_allocations_in_use.erase(alloc_id);
	m_free_allocations.push(alloc);
}


