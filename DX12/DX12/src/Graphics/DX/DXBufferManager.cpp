#include "pch.h"
#include "DXBufferManager.h"
#include "Graphics/DX/DXCommon.h"

DXBufferManager::~DXBufferManager()
{

}

DXBufferManager::DXBufferManager(Microsoft::WRL::ComPtr<ID3D12Device> dev, uint32_t max_fif) :
	m_dev(dev)
{
	// setup allocator for persistent memory
	{
		m_constant_persistent_bufs.resize(max_fif);
		auto pool_infos =
		{
			DXBufferPoolAllocator::PoolInfo(1, 256, 100),
			DXBufferPoolAllocator::PoolInfo(1, 512, 100),
			DXBufferPoolAllocator::PoolInfo(1, 1024, 4000),
		};
		//m_constant_persistent_buf = std::make_unique<DXBufferPoolAllocator>(dev, pool_infos, D3D12_HEAP_TYPE_DEFAULT);
		for (uint32_t i = 0; i < max_fif; ++i)
			m_constant_persistent_bufs[i] = std::make_unique<DXBufferPoolAllocator>(dev, pool_infos, D3D12_HEAP_TYPE_DEFAULT);
	}

	// setup ring buffer for transient upload buffer
	{
		auto pool_infos =
		{
			DXBufferPoolAllocator::PoolInfo(1, 256, 100),
			DXBufferPoolAllocator::PoolInfo(1, 512, 50),
			DXBufferPoolAllocator::PoolInfo(1, 1024, 6000),
		};
		auto pool_for_ring = std::make_unique<DXBufferPoolAllocator>(dev, pool_infos, D3D12_HEAP_TYPE_UPLOAD);
		m_constant_ring_buf = std::make_unique<DXBufferRingPoolAllocator>(std::move(pool_for_ring));
	}
}

BufferHandle DXBufferManager::create_buffer(const DXBufferDesc& desc)
{
	// enforce some rules to the calling app
	assert(desc.usage_cpu != UsageIntentCPU::eInvalid);
	assert(desc.usage_gpu != UsageIntentGPU::eInvalid);
	assert(desc.flag != BufferFlag::eInvalid);
	assert(desc.element_count >= 0);
	assert(desc.element_size > 0);
	assert(desc.element_count * desc.element_size >= desc.data_size);


	InternalBufferResource* res = nullptr;
	if (desc.flag == BufferFlag::eConstant)
	{
		res = create_constant(desc);
	}
	else
	{
		assert(false);
	}

	// fill out common metadata
	if (res)
	{
		res->usage_cpu = desc.usage_cpu;
		res->usage_gpu = desc.usage_gpu;
		res->total_requested_size = desc.element_count * desc.element_size;
		res->frame_idx_allocation = m_curr_frame_idx;
		return BufferHandle(res->handle);
	}

	return BufferHandle(0);
}

void DXBufferManager::destroy_buffer(BufferHandle hdl)
{
	auto res = m_handles.get_resource(hdl.handle);

	if (res->is_constant)
	{
		destroy_constant(res);
	}
	else
	{

	}
}

void DXBufferManager::bind_as_direct_arg(ID3D12GraphicsCommandList* cmdl, BufferHandle buf, UINT param_idx, RootArgDest dest)
{
	const auto& res = m_handles.get_resource(buf.handle);

	if (res->is_constant)
	{
		if (dest == RootArgDest::eGraphics)
			cmdl->SetGraphicsRootConstantBufferView(param_idx, res->alloc.gpu_adr());
	}

	//switch (res->usage_gpu)
	//{
	//case UsageIntentGPU::eConstantRead:
	//{
	//	if (dest == RootArgDest::eGraphics)
	//		cmdl->SetGraphicsRootConstantBufferView(param_idx, res->alloc.gpu_adr());	

	//	break;
	//}
	//case UsageIntentGPU::eShaderRead:
	//{
	//	assert(false);
	//	break;
	//}
	//case UsageIntentGPU::eReadWrite:
	//{
	//	assert(false);		// ..
	//	break;
	//}
	//default:
	//	assert(false);		// programmer error: forgot to fill UsageIntentGPU on the underlying resource somewhere!
	//	break;
	//}

}

void DXBufferManager::create_view_for(BufferHandle handle, D3D12_CPU_DESCRIPTOR_HANDLE descriptor)
{
	auto res = m_handles.get_resource(handle.handle);

	D3D12_CONSTANT_BUFFER_VIEW_DESC d{};
	d.BufferLocation = res->alloc.gpu_adr();
	d.SizeInBytes = res->alloc.size();
	m_dev->CreateConstantBufferView(&d, descriptor);
}

void DXBufferManager::frame_begin(uint32_t frame_idx)
{
	m_curr_frame_idx = frame_idx;

	// resources are freed back to the ring buffer on a per-frame basis
	m_constant_ring_buf->frame_begin(frame_idx);


	// deallocate 
	while (!m_deletion_queue.empty())
	{
		auto el = m_deletion_queue.front();
		if (el.first == frame_idx)
		{
			auto& del_func = el.second;
			del_func();
			m_deletion_queue.pop();
		}
		else
			break;
	}

}






DXBufferManager::InternalBufferResource* DXBufferManager::get_internal_buf(BufferHandle handle)
{
	return m_handles.get_resource(handle.handle);
}

DXBufferManager::InternalBufferResource* DXBufferManager::create_constant(const DXBufferDesc& desc)
{
	assert(desc.element_count == 1);

	auto [handle, resource] = m_handles.get_next_free_handle();
	resource->is_constant = true;

	const auto& usage_cpu = desc.usage_cpu;
	const auto& usage_gpu = desc.usage_gpu;
	uint32_t requested_size = desc.element_count * desc.element_size;

	if (desc.usage_gpu == UsageIntentGPU::eReadMultipleTimesPerFrame)
		assert(false);		// currently not supported

	// Immutable. Irrelevant what GPU read access is given
	if (usage_cpu == UsageIntentCPU::eUpdateNever)
	{
		// allocate from persistent allocator
		//auto alloc = m_constant_persistent_buf->allocate(requested_size);
		auto alloc = m_constant_persistent_bufs[m_curr_frame_idx]->allocate(requested_size);
		resource->alloc = alloc;
		resource->is_transient = false;

	}
	// Only updates sometimes every X frames
	else if (usage_cpu == UsageIntentCPU::eUpdateSometimes)
	{
		//auto alloc = m_constant_persistent_buf->allocate(requested_size);
		auto alloc = m_constant_persistent_bufs[m_curr_frame_idx]->allocate(requested_size);
		resource->alloc = alloc;
		resource->is_transient = false;
	}
	// Write-once-read-once --> MSFT recommends Upload Heap
	// https://docs.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources
	else if (usage_cpu == UsageIntentCPU::eUpdateOnce && usage_gpu == UsageIntentGPU::eReadOncePerFrame)
	{
		// allocate from ring buffer
		auto alloc = m_constant_ring_buf->allocate(requested_size);
		resource->alloc = alloc;
		resource->is_transient = true;
	}
	else if (usage_cpu == UsageIntentCPU::eUpdateOnce && usage_gpu == UsageIntentGPU::eReadMultipleTimesPerFrame)
	{
		// not supported for now
		// allocate from persistent allocator (max FIF versions, to handle worst case)
		// each update request --> cycle resource --> copy up to new resource
		// on the update request --> we discard the existing allocation by pushing it into a deletion queue 
		// deletion queue includes a lambda
		// allocate from persistent allocator
		//auto alloc = m_constant_persistent_buf->allocate(requested_size);
		auto alloc = m_constant_persistent_bufs[m_curr_frame_idx]->allocate(requested_size);
		resource->alloc = alloc;
		resource->is_transient = true;

		assert(false);
	}
	else if (usage_cpu == UsageIntentCPU::eUpdateMultipleTimesPerFrame && usage_gpu == UsageIntentGPU::eReadMultipleTimesPerFrame)
	{
		// not supported for now: this could either be on Upload Heap OR device-local
		// we could just subscribe this to the ring buffer, but not doing it now unless we need this explicitly.
		assert(false);
	}
	else if (usage_cpu == UsageIntentCPU::eUpdateMultipleTimesPerFrame && usage_gpu == UsageIntentGPU::eReadOncePerFrame)
	{
		// This makes no sense as we would want to read multiple times if we update multiple times!
		assert(false);
	}
	else
		assert(false);

	return resource;
}

void DXBufferManager::destroy_constant(InternalBufferResource* res)
{
	if (res->usage_cpu == UsageIntentCPU::eUpdateNever)
	{
		//m_constant_persistent_buf->deallocate(std::move(res->alloc));
		m_constant_persistent_bufs[res->frame_idx_allocation]->deallocate(std::move(res->alloc));
		m_handles.free_handle(res->handle);
	}
	else if (res->usage_cpu == UsageIntentCPU::eUpdateOnce && res->usage_gpu == UsageIntentGPU::eReadOncePerFrame)
	{
		// automatically cleaned up on frame begin
		return;
	}
	else if (res->usage_cpu == UsageIntentCPU::eUpdateOnce && res->usage_gpu == UsageIntentGPU::eReadMultipleTimesPerFrame)
	{
		//m_constant_persistent_buf->deallocate(std::move(res->alloc));
		m_constant_persistent_bufs[res->frame_idx_allocation]->deallocate(std::move(res->alloc));
		m_handles.free_handle(res->handle);
	}
	else
		assert(false);
}



