#include "pch.h"
#include "DXBufferManager.h"
#include "Graphics/DX/DXCommon.h"

DXBufferManager::~DXBufferManager()
{

}

DXBufferManager::DXBufferManager(Microsoft::WRL::ComPtr<ID3D12Device> dev) :
	m_dev(dev)
{
	// setup allocator for persistent memory
	{
		auto pool_infos =
		{
			DXBufferPoolAllocator::PoolInfo(1, 256, 100),
			DXBufferPoolAllocator::PoolInfo(1, 512, 50),
			DXBufferPoolAllocator::PoolInfo(1, 1024, 25),
		};
		m_constant_persistent_buf = std::make_unique<DXBufferPoolAllocator>(dev, pool_infos, D3D12_HEAP_TYPE_DEFAULT);
	}

	// setup ring buffer for transient upload buffer
	{
		auto pool_infos =
		{
			DXBufferPoolAllocator::PoolInfo(1, 256, 100),
			DXBufferPoolAllocator::PoolInfo(1, 512, 50),
			DXBufferPoolAllocator::PoolInfo(1, 1024, 25),
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
	assert(desc.element_count >= 0);
	assert(desc.element_size >= 0);
	assert(desc.element_count * desc.element_size >= desc.data_size);
	if (desc.usage_gpu == UsageIntentGPU::eConstantRead)
		assert(desc.element_count == 1);

	InternalBufferResource* res = nullptr;
	switch (desc.usage_gpu)
	{
	case UsageIntentGPU::eConstantRead:
	{
		res = grab_constant_memory(desc);	
		const auto& md = res->get_metadata<ConstantAccessBufferMD>();

		// upload init. data
		//auto dst = md.alloc->get_memory()->get_mapped_memory();
		auto dst = md.alloc->get_mapped_memory();
		auto src = desc.data;
		std::memcpy(dst, src, desc.data_size);
		
		break;
	}

	case UsageIntentGPU::eShaderRead:
	{
		assert(false); // to impl.
		break;
	}
	case UsageIntentGPU::eReadWrite:
	{
		assert(false);	// to impl.
		break;
	}
	default:
		assert(false);
		break;
	}
	
	assert(res != nullptr);
	res->total_requested_size = desc.element_count * desc.element_size;

	return BufferHandle(res->handle);
}

void DXBufferManager::destroy_buffer(BufferHandle hdl)
{
	auto res = m_handles.get_resource(hdl.handle);
	switch (res->usage_gpu)
	{
	case UsageIntentGPU::eConstantRead:
	{
		auto& md = res->get_metadata<ConstantAccessBufferMD>();
		if (!md.transient)
		{
			//m_constant_persistent_buf->deallocate(md.alloc);
			m_constant_persistent_buf->deallocate(md.alloc);
		}
		break;
	}
	case UsageIntentGPU::eShaderRead:
	{
		break;
	}
	case UsageIntentGPU::eReadWrite:
	{
		break;
	}
	}
}

void DXBufferManager::upload_data(void* data, size_t size, BufferHandle hdl)
{
	auto res = m_handles.get_resource(hdl.handle);
	switch (res->usage_gpu)
	{
	case UsageIntentGPU::eConstantRead:
	{
		auto& md = res->get_metadata<ConstantAccessBufferMD>();
		if (md.transient)
		{
			// grab new memory from ring buffer
			//md.alloc = m_constant_ring_buf->allocate(res->total_requested_size);
			md.alloc = m_constant_ring_buf->allocate(res->total_requested_size);

			// upload
			assert(size <= res->total_requested_size);	

			// check that the memory IS mappable!
			//assert(md.alloc->get_memory()->mappable());
			assert(md.alloc->mappable());

			//auto dst = md.alloc->get_memory()->get_mapped_memory();
			auto dst = md.alloc->get_mapped_memory();
			auto src = data;
			std::memcpy(dst, src, size);
		}
		else
		{
			// do cpu-gpu copy to staging (allocate from a fixed size upload buffer)
			// do gpu-gpu copy from staging to device-local memory
		}
		break;
	}
	case UsageIntentGPU::eShaderRead:
	{
		break;
	}
	case UsageIntentGPU::eReadWrite:
	{
		assert(false);		// ..
		break;
	}
	default:
		assert(false);		// programmer error: forgot to fill UsageIntentGPU on the underlying resource somewhere!
		break;
	}
}

void DXBufferManager::bind_as_direct_arg(ID3D12GraphicsCommandList* cmdl, BufferHandle buf, UINT param_idx, RootArgDest dest)
{
	const auto& res = m_handles.get_resource(buf.handle);

	switch (res->usage_gpu)
	{
	case UsageIntentGPU::eConstantRead:
	{
		const auto& md = res->get_metadata<ConstantAccessBufferMD>();
		if (!md.alloc->is_valid())
			assert(false);				// Transient suballocation will be stomped! You forgot to use update on this buffer which was declared Transient (e.g at least one update per frame)

		if (dest == RootArgDest::eGraphics)
			cmdl->SetGraphicsRootConstantBufferView(param_idx, md.alloc->get_gpu_adr());	

		break;
	}
	case UsageIntentGPU::eShaderRead:
	{
		assert(false);
		break;
	}
	case UsageIntentGPU::eReadWrite:
	{
		assert(false);		// ..
		break;
	}
	default:
		assert(false);		// programmer error: forgot to fill UsageIntentGPU on the underlying resource somewhere!
		break;
	}

}

void DXBufferManager::frame_begin(uint32_t frame_idx)
{
	// resources are freed back to the ring buffer on a per-frame basis
	m_constant_ring_buf->frame_begin(frame_idx);
}

DXBufferManager::InternalBufferResource* DXBufferManager::grab_constant_memory(const DXBufferDesc& desc)
{
	HandlePool<InternalBufferResource>::Allocation handle_alloc;

	switch (desc.usage_cpu)
	{
	case UsageIntentCPU::eUpdateNever:					// immutable (requires persistent memory)
	case UsageIntentCPU::eUpdateSometimes:				// arbitrary lifetime (requires persistent memory)
	{
		handle_alloc = m_handles.get_next_free_handle();
		auto& md = handle_alloc.resource->get_metadata<ConstantAccessBufferMD>();
		//md.alloc = m_constant_persistent_buf->allocate(desc.element_count * desc.element_size);
		md.alloc = m_constant_persistent_buf->allocate(desc.element_count * desc.element_size);
		md.transient = false;
		break;
	}
	case UsageIntentCPU::eUpdateOnceOrMorePerFrame:		// transient (subscribed to ring buffer)
	{
		handle_alloc = m_handles.get_next_free_handle();
		auto& md = handle_alloc.resource->get_metadata<ConstantAccessBufferMD>();
		//md.alloc = m_constant_ring_buf->allocate(desc.element_count * desc.element_size);
		md.alloc = m_constant_ring_buf->allocate(desc.element_count * desc.element_size);
		md.transient = true;
		break;
	}
	default:
		assert(false);
		break;

	}

	handle_alloc.resource->usage_gpu = desc.usage_gpu;

	return handle_alloc.resource;
}
