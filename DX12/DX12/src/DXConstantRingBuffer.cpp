#include "pch.h"
#include "DXConstantRingBuffer.h"
#include "DXBufferMemPool.h"

DXConstantRingBuffer::DXConstantRingBuffer(cptr<ID3D12Device> dev) :
	m_dev(dev.Get())
{
	m_pool_256 = std::make_unique<DXBufferMemPool>(dev.Get(), 256, 100, D3D12_HEAP_TYPE_UPLOAD);
	m_pool_512 = std::make_unique<DXBufferMemPool>(dev.Get(), 512, 50, D3D12_HEAP_TYPE_UPLOAD);
	m_pool_1024 = std::make_unique<DXBufferMemPool>(dev.Get(), 1024, 25, D3D12_HEAP_TYPE_UPLOAD);
}
