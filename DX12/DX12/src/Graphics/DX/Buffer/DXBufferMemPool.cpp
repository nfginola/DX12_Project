#include "pch.h"
#include "DXBufferMemPool.h"
#include "d3dx12.h"

DXBufferMemPool::DXBufferMemPool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_HEAP_TYPE heap_type) :
	m_element_size(element_size)
{
	const auto handle_size = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	if (heap_type == D3D12_HEAP_TYPE_UPLOAD)
		m_curr_state = D3D12_RESOURCE_STATE_GENERIC_READ;		// required start state for upload buffer

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
		m_curr_state,
		nullptr,
		IID_PPV_ARGS(m_buffer.GetAddressOf()));
	if (FAILED(hr))
		assert(false);
	m_base_gpu_adr = m_buffer->GetGPUVirtualAddress();
	m_end_gpu_adr = m_buffer->GetGPUVirtualAddress() + num_elements * element_size;

	// Persistently map if upload/readback buffer
	if (heap_type == D3D12_HEAP_TYPE_UPLOAD || heap_type == D3D12_HEAP_TYPE_READBACK)
	{
		D3D12_RANGE no_read{};
		m_buffer->Map(0, &no_read, (void**)&m_base_cpu_adr);
	}


	//m_free_allocations.reserve(num_elements);	// important to ensure that memory is in place (no reallocations and invalidating of pointer!)
	// Initialize sub-allocations
	for (uint32_t alloc_id = 0; alloc_id < num_elements; ++alloc_id)
	{
		auto allocation = DXBufferAllocation(
			m_buffer.Get(),
			alloc_id * element_size,
			element_size,
			m_base_gpu_adr + (uint64_t)alloc_id * element_size,
			true,
			m_base_cpu_adr ? m_base_cpu_adr + (uint64_t)alloc_id * element_size : nullptr
		);

		m_free_allocations.push(std::move(allocation));
	}
}

DXBufferAllocation DXBufferMemPool::allocate()
{
	if (m_free_allocations.empty())
		return {};

	auto new_allocation = std::move(m_free_allocations.front());
	m_free_allocations.pop();

	return new_allocation;
}

void DXBufferMemPool::deallocate(DXBufferAllocation&& alloc)
{

	// given allocation must be part of this pool
	assert(m_base_gpu_adr <= alloc.gpu_adr() && alloc.gpu_adr() < m_end_gpu_adr);

	m_free_allocations.push(alloc);
}

void DXBufferMemPool::set_state(D3D12_RESOURCE_STATES new_state, ID3D12GraphicsCommandList* cmdl)
{
	auto t_barr = CD3DX12_RESOURCE_BARRIER::Transition(m_buffer.Get(), m_curr_state, new_state, 0, D3D12_RESOURCE_BARRIER_FLAG_NONE);
	cmdl->ResourceBarrier(1, &t_barr);
}

uint16_t DXBufferMemPool::get_allocation_size() const
{
	return m_element_size;
}


