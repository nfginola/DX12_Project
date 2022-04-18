#pragma once
#include "DXBufferMemPool.h"
#include "DXConstantSuballocation.h"

/*
	Provides a persistent suballocation for the application.
	For any buffers which have arbitrary lifetimes. This is stored on the DEFAULT heap!

*/

class DXConstantStaticBuffer
{
public:
	DXConstantStaticBuffer() = delete;
	DXConstantStaticBuffer(cptr<ID3D12Device> dev);

	// Public API
	DXConstantSuballocation* allocate();
	void deallocate(DXConstantSuballocation* alloc);

private:


};

