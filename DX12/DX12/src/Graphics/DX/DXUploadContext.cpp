#include "pch.h"
#include "DXUploadContext.h"

#include "DXBufferManager.h"

DXUploadContext::DXUploadContext(cptr<ID3D12Device> dev, DXBufferManager* buf_mgr, uint32_t max_fif) :
	m_dev(dev),
	m_buf_mgr(buf_mgr),
	m_max_fif(max_fif)
{
	D3D12_COMMAND_QUEUE_DESC desc{};
	desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	auto hr = dev->CreateCommandQueue(&desc, IID_PPV_ARGS(m_copy_queue.GetAddressOf()));
	if (FAILED(hr))
		assert(false);

	// create ators n cmdls
	m_cmdls.resize(max_fif);
	m_ators.resize(max_fif);
	for (uint32_t i = 0; i < max_fif; ++i)
	{
		hr = dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(m_ators[i].GetAddressOf()));
		if (FAILED(hr))
			assert(false);
		hr = dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_ators[i].Get(), nullptr, IID_PPV_ARGS(m_cmdls[i].GetAddressOf()));
		if (FAILED(hr))
			assert(false);
	}
}

