#include "pch.h"
#include "DXUploadContext.h"
#include "WinPixEventRuntime/pix3.h"

static int thing = 0;

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
	m_sync_prims.resize(max_fif);
	for (uint32_t i = 0; i < max_fif; ++i)
	{
		hr = dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(m_ators[i].GetAddressOf()));
		if (FAILED(hr))
			assert(false);
		hr = dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_ators[i].Get(), nullptr, IID_PPV_ARGS(m_cmdls[i].GetAddressOf()));
		if (FAILED(hr))
			assert(false);

		// cmdls are open on creation..
		m_cmdls[i]->Close();

		m_sync_prims[i] = DXFence(dev.Get());
	}
}

void DXUploadContext::frame_begin(uint32_t frame_idx)
{
	// Assuming that resources that are used have been transitioned to Common state (for use on Copy queue)
	m_curr_frame_idx = frame_idx;

	m_ators[frame_idx]->Reset();
	m_cmdls[frame_idx]->Reset(m_ators[frame_idx].Get(), nullptr);

	// the deault buffer promoted to VERTEX AND CONST BUF decay by itself upon completion on the main queue, 
	// meaning that for copies --> it is ready to be used on the Copy queue

	//m_buf_mgr->m_constant_persistent_bufs[m_curr_frame_idx]->set_state(D3D12_RESOURCE_STATE_COMMON, before_copy);
	//m_buf_mgr->m_constant_persistent_bufs[m_curr_frame_idx]->set_state(D3D12_RESOURCE_STATE_COPY_DEST, m_cmdls[frame_idx].Get());

	std::string event_name = "copy thing #" + std::to_string(thing++);
	PIXBeginEvent(m_cmdls[frame_idx].Get(), PIX_COLOR(200, 0, 200), event_name.c_str());
}

void DXUploadContext::upload_data(void* data, size_t size, BufferHandle hdl)
{
	auto res = m_buf_mgr->get_internal_buf(hdl);

	if (res->is_constant)
	{
		update_constant(data, size, res);
	}
}

void DXUploadContext::submit_work(uint32_t sig_val)
{
	auto cmdl = m_cmdls[m_curr_frame_idx].Get();
	cmdl->Close();
	ID3D12CommandList* cmdls[] = { cmdl };
	m_copy_queue->ExecuteCommandLists(1, cmdls);
	// Resources decays to common state

	PIXEndEvent(m_copy_queue.Get());

	auto& sync = m_sync_prims[m_curr_frame_idx];
	sync.signal(m_copy_queue.Get(), sig_val);
}

void DXUploadContext::wait_for_async_copy(ID3D12CommandQueue* queue)
{
	auto& sync = m_sync_prims[m_curr_frame_idx];
	sync.wait(queue);
}







void DXUploadContext::update_constant(void* data, size_t size, DXBufferManager::InternalBufferResource* res)
{
	if (res->usage_cpu == UsageIntentCPU::eUpdateNever)
	{
		assert(false);
	}
	else if (res->usage_cpu == UsageIntentCPU::eUpdateOnce && res->usage_gpu == UsageIntentGPU::eReadOncePerFrame)
	{
		// grab new memory from ring buffer
		res->alloc = m_buf_mgr->m_constant_ring_buf->allocate(res->total_requested_size);
		res->frame_idx_allocation = m_curr_frame_idx;

		// upload
		assert(size <= res->total_requested_size);

		// check that the memory IS mappable!
		assert(res->alloc.mappable());

		// copy to upload heap
		auto dst = res->alloc.mapped_memory();
		auto src = data;
		std::memcpy(dst, src, size);
	}
	else if (res->usage_cpu == UsageIntentCPU::eUpdateSometimes || 
			(res->usage_cpu == UsageIntentCPU::eUpdateOnce && res->usage_gpu == UsageIntentGPU::eReadMultipleTimesPerFrame)
		)
	{
		// push deletion request
		auto del_func = [this, frame_idx = res->frame_idx_allocation, alloc = std::move(res->alloc)]() mutable
		{
			m_buf_mgr->m_constant_persistent_bufs[frame_idx]->deallocate(std::move(alloc));
		};
		m_buf_mgr->m_deletion_queue.push({ m_curr_frame_idx, del_func });

		// grab new version
		auto new_alloc = m_buf_mgr->m_constant_persistent_bufs[m_curr_frame_idx]->allocate(res->total_requested_size);
		res->frame_idx_allocation = m_curr_frame_idx;

		// grab temp staging memory
		auto staging = m_buf_mgr->m_constant_ring_buf->allocate(res->total_requested_size);

		// copy data to staging
		auto dst = staging.mapped_memory();
		auto src = data;
		std::memcpy(dst, src, size);

		// schedule GPU-GPU copy to new version
		m_cmdls[m_curr_frame_idx]->CopyBufferRegion(
			new_alloc.base_buffer(), new_alloc.offset_from_base(),
			staging.base_buffer(), staging.offset_from_base(), res->total_requested_size);

		// assign new version to resource
		res->alloc = std::move(new_alloc);

		//assert(false);
	}
}

