#include "pch.h"
#include "DXBufferMemPool.h"
#include "d3dx12.h"
#include <random>

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

	// Add unique ID to differentiate different pool allocations (we can at least minimize POTENTIAL collisions)
	// Otherwise, the calling app should only allocate/deallocate allocations which come from a specific pool!
	uint32_t unique_key = 0;
	{
		std::random_device random_dev; 
		std::mt19937 gen(random_dev());
		std::uniform_int_distribution<uint32_t> distr(0, UINT32_MAX); 
		unique_key = distr(gen);
	}

	m_allocations.reserve(num_elements);	// important to ensure that memory is in place (no reallocations and invalidating of pointer!)
	// Initialize sub-allocations
	for (uint32_t alloc_id = 0; alloc_id < num_elements; ++alloc_id)
	{
		DXBufferSuballocation allocation{};
		allocation.m_allocation_id = unique_key + (uint64_t)alloc_id;
		allocation.m_gpu_address = m_base_gpu_adr + (uint64_t)alloc_id * element_size;
		allocation.m_base_buffer = m_buffer.Get();
		allocation.m_offset_from_base = alloc_id * element_size;
		allocation.m_size = element_size;

		if (m_base_cpu_adr)
			allocation.m_mapped_memory = m_base_cpu_adr + (uint64_t)alloc_id * element_size;

		m_allocations.push_back(allocation);
		m_free_allocations.push(&m_allocations.back());

	}
	std::cout << "wa\n";
}

DXBufferSuballocation* DXBufferMemPool::allocate()
{
	if (m_free_allocations.empty())
		return nullptr;

	DXBufferSuballocation* new_allocation = m_free_allocations.front();
	m_allocations_in_use.insert(new_allocation->m_allocation_id);
	m_free_allocations.pop();

	new_allocation->m_is_valid = true;

	return new_allocation;
}

void DXBufferMemPool::deallocate(DXBufferSuballocation* alloc)
{
	assert(alloc != nullptr);
	const auto& alloc_id = alloc->m_allocation_id;
	const auto gpu_adr = alloc->m_gpu_address;

	// given allocation must be part of this pool
	assert(m_base_gpu_adr <= gpu_adr && gpu_adr < m_end_gpu_adr);
	// crash if free-after-free detected
	assert(m_allocations_in_use.find(alloc_id) != m_allocations_in_use.cend());

	alloc->m_is_valid = false;

	m_allocations_in_use.erase(alloc_id);
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


