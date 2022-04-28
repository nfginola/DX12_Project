#include "pch.h"
#include "DXBufferGenericAllocator.h"

DXBufferGenericAllocator::DXBufferGenericAllocator(cptr<ID3D12Device> dev, D3D12_HEAP_TYPE heap_type) :
	m_dev(dev),
	m_heap_type(heap_type)
{
}

DXBufferAllocation DXBufferGenericAllocator::allocate(uint32_t element_count, uint32_t element_size)
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
	d.Flags = D3D12_RESOURCE_FLAG_NONE;

	cptr<ID3D12Resource> buf;
	m_dev->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&d,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(buf.GetAddressOf()));

	return DXBufferAllocation
	(
		buf.Get(),
		0,
		total_size,
		element_size,
		buf->GetGPUVirtualAddress(),
		false,
		nullptr
	);;
}