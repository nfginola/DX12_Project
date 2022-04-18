#include "pch.h"
#include "DXConstantStaticBuffer.h"

DXConstantStaticBuffer::DXConstantStaticBuffer(cptr<ID3D12Device> dev)
{


}

DXConstantSuballocation* DXConstantStaticBuffer::allocate()
{
	return nullptr;
}

void DXConstantStaticBuffer::deallocate(DXConstantSuballocation* alloc)
{
}
