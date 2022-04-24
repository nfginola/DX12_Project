#pragma once
#include "DXCommon.h"

class DXBufferManager;

// Async copy queue
class DXUploadContext
{
public:
	DXUploadContext(cptr<ID3D12Device> dev, DXBufferManager* buf_mgr, uint32_t max_fif);

	/*
		CopyBufferRegion
		CopyResource
		CopyTextureRegion
	
	*/


private:
	cptr<ID3D12Device> m_dev;
	cptr<ID3D12CommandQueue> m_copy_queue;

	// paired list + allocator for each FIF
	std::vector<cptr<ID3D12GraphicsCommandList>> m_cmdls;
	std::vector<cptr<ID3D12CommandAllocator>> m_ators;



	DXBufferManager* m_buf_mgr;
	// DXTextureManager* m_tex_mgr;

	uint32_t m_max_fif = 0;

};

