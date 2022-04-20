#include "pch.h"
#include "DXConstantStaticBuffer.h"

DXConstantStaticBuffer::DXConstantStaticBuffer(cptr<ID3D12Device> dev)
{
	auto pool_infos =
	{
		DXConstantSuballocator::PoolInfo(1, 256, 100),
		DXConstantSuballocator::PoolInfo(1, 512, 50),
		DXConstantSuballocator::PoolInfo(1, 1024, 25),
	};
	m_suballoc_utils = std::make_unique<DXConstantSuballocator>(dev.Get(), pool_infos, D3D12_HEAP_TYPE_DEFAULT);
}

DXConstantSuballocation* DXConstantStaticBuffer::allocate(uint64_t requested_size)
{
    return m_suballoc_utils->allocate(requested_size);
}

void DXConstantStaticBuffer::deallocate(DXConstantSuballocation* alloc)
{
    m_suballoc_utils->deallocate(alloc);
}
