#include "pch.h"
#include "DXBufferGenericAllocator.h"

DXBufferGenericAllocator::DXBufferGenericAllocator(cptr<ID3D12Device> dev, D3D12_HEAP_TYPE heap_type) :
	m_dev(dev),
	m_heap_type(heap_type)
{
}

DXBufferAllocation DXBufferGenericAllocator::allocate(uint32_t element_count, uint32_t element_size, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags)
{
	const auto total_size = element_count * element_size;

	D3D12_HEAP_PROPERTIES hp{};
	hp.Type = m_heap_type; 

	D3D12_RESOURCE_DESC d{};
	d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	d.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	d.Width = total_size;
	d.Height = d.DepthOrArraySize = d.MipLevels = 1;
	d.Format = DXGI_FORMAT_UNKNOWN;
	d.SampleDesc = { 1, 0 };
	d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d.Flags = flags;

	cptr<ID3D12Resource> buf;
	m_dev->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&d,
		state,
		nullptr,
		IID_PPV_ARGS(buf.GetAddressOf()));

	uint8_t* mapped_start = nullptr;
	// Persistently map if upload/readback buffer
	if (m_heap_type == D3D12_HEAP_TYPE_UPLOAD || m_heap_type == D3D12_HEAP_TYPE_READBACK)
	{
		D3D12_RANGE no_read{};
		buf->Map(0, &no_read, (void**)&mapped_start);
	}

	auto alloc = DXBufferAllocation
	(
		buf,
		0,
		total_size,
		element_size,
		buf->GetGPUVirtualAddress(),
		false,
		mapped_start
	);

	// track
	//m_allocs.insert({ alloc.gpu_adr(), alloc });
	m_existing_allocs.insert(alloc.gpu_adr());

	return alloc;
}

void DXBufferGenericAllocator::deallocate(DXBufferAllocation&& alloc)
{
//	auto it = m_allocs.find(alloc.gpu_adr());
//	if (it == m_allocs.cend())
//		assert(false);

	auto it = m_existing_allocs.find(alloc.gpu_adr());
	if (it == m_existing_allocs.cend())
		assert(false);

	//auto refs_left = alloc.reset();
	//for (int i = 0; i < refs_left; ++i)
	//	alloc.reset();

	// erase from internal tracker
	m_allocs.erase(alloc.gpu_adr());
}
