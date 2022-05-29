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
			DXBufferPoolAllocator::PoolInfo(1, 256, 5000),
			DXBufferPoolAllocator::PoolInfo(1, 512, 5000),
			DXBufferPoolAllocator::PoolInfo(1, 1024, 30000),
		};
		//m_constant_persistent_buf = std::make_unique<DXBufferPoolAllocator>(dev, pool_infos, D3D12_HEAP_TYPE_DEFAULT);
		for (uint32_t i = 0; i < max_fif; ++i)
			m_constant_persistent_bufs[i] = std::make_unique<DXBufferPoolAllocator>(dev, pool_infos, D3D12_HEAP_TYPE_DEFAULT);
	}

	// setup ring buffer for transient upload buffer
	{
		auto pool_infos =
		{
			DXBufferPoolAllocator::PoolInfo(1, 256, 5000),
			DXBufferPoolAllocator::PoolInfo(1, 512, 5000),
			DXBufferPoolAllocator::PoolInfo(1, 1024, 30000),
		};
		auto pool_for_ring = std::make_unique<DXBufferPoolAllocator>(dev, pool_infos, D3D12_HEAP_TYPE_UPLOAD);
		m_constant_ring_buf = std::make_unique<DXBufferRingPoolAllocator>(std::move(pool_for_ring));
	}

	m_committed_def_ator = std::make_unique<DXBufferGenericAllocator>(m_dev, D3D12_HEAP_TYPE_DEFAULT);
	m_committed_upload_ator = std::make_unique<DXBufferGenericAllocator>(m_dev, D3D12_HEAP_TYPE_UPLOAD);
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
		res = create_non_constant(desc);
		res->is_constant = false;

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
		if (res->usage_gpu == UsageIntentGPU::eWrite)
		{
			m_committed_def_ator->deallocate(std::move(res->alloc));
			m_handles.free_handle(res->handle);
		}
	}
}

void DXBufferManager::bind_as_direct_arg(ID3D12GraphicsCommandList* cmdl, BufferHandle buf, UINT param_idx, RootArgDest dest, bool write)
{
	const auto& res = m_handles.get_resource(buf.handle);


	if (res->is_constant)
	{
		if (dest == RootArgDest::eGraphics)
			cmdl->SetGraphicsRootConstantBufferView(param_idx, res->alloc.gpu_adr());
	}
	else
	{
		if (dest == RootArgDest::eGraphics)
		{
			if (write)
				cmdl->SetGraphicsRootUnorderedAccessView(param_idx, res->alloc.gpu_adr());
			else
				cmdl->SetGraphicsRootShaderResourceView(param_idx, res->alloc.gpu_adr());
		}
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

void DXBufferManager::create_cbv(BufferHandle handle, D3D12_CPU_DESCRIPTOR_HANDLE descriptor)
{
	auto res = m_handles.get_resource(handle.handle);

	D3D12_CONSTANT_BUFFER_VIEW_DESC d{};
	d.BufferLocation = res->alloc.gpu_adr();
	d.SizeInBytes = res->alloc.size();
	m_dev->CreateConstantBufferView(&d, descriptor);
}

void DXBufferManager::create_srv(BufferHandle handle, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t start_el, uint32_t num_el, bool raw)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC d{};
	const auto& alloc = m_handles.get_resource(handle.handle)->alloc;
	auto res_d = alloc.base_buffer()->GetDesc();

	d.Format = res_d.Format;
	d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	d.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	d.Buffer.FirstElement = start_el;
	d.Buffer.NumElements = num_el;
	d.Buffer.StructureByteStride = alloc.element_size();
	d.Buffer.Flags = raw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;
	m_dev->CreateShaderResourceView(alloc.base_buffer(), &d, descriptor);
}

void DXBufferManager::create_rt_accel_view(BufferHandle handle, D3D12_CPU_DESCRIPTOR_HANDLE descriptor)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC d{};
	const auto& alloc = m_handles.get_resource(handle.handle)->alloc;
	auto res_d = alloc.base_buffer()->GetDesc();

	d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	d.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	d.RaytracingAccelerationStructure.Location = alloc.gpu_adr();
	m_dev->CreateShaderResourceView(alloc.base_buffer(), &d, descriptor);
}

D3D12_INDEX_BUFFER_VIEW DXBufferManager::get_ibv(BufferHandle handle, DXGI_FORMAT format)
{
	const auto& alloc = m_handles.get_resource(handle.handle)->alloc;
	auto res_d = alloc.base_buffer()->GetDesc();

	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = alloc.gpu_adr();
	ibv.Format = format;
	ibv.SizeInBytes = alloc.size();
	return ibv;
}

uint32_t DXBufferManager::get_element_count(BufferHandle handle)
{
	return m_handles.get_resource(handle.handle)->alloc.element_count();
}

const DXBufferAllocation* DXBufferManager::get_buffer_alloc(BufferHandle handle)
{
	return &m_handles.get_resource(handle.handle)->alloc;
}

void DXBufferManager::frame_begin(uint32_t frame_idx)
{
	m_curr_frame_idx = frame_idx;

	// resources are freed back to the ring buffer on a per-frame basis
	m_constant_ring_buf->frame_begin(frame_idx);


	// deallocate
	if (!m_first_frame)
	{
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
	else
		m_first_frame = false;
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

		// grab temp staging memory
		auto staging = m_constant_ring_buf->allocate(requested_size);

		// copy data to staging
		auto dst = staging.mapped_memory();
		auto src = desc.data;
		std::memcpy(dst, src, desc.data_size);

		// schedule GPU-GPU copy to new version
		auto upload_func = [staging, alloc, requested_size](ID3D12GraphicsCommandList* cmdl)
		{
			cmdl->CopyBufferRegion(
				alloc.base_buffer(), alloc.offset_from_base(),
				staging.base_buffer(), staging.offset_from_base(), requested_size);
		};
		m_deferred_init_copies.push(upload_func);
	}
	// Only updates sometimes every X frames
	else if (usage_cpu == UsageIntentCPU::eUpdateSometimes)
	{
		auto alloc = m_constant_persistent_bufs[m_curr_frame_idx]->allocate(requested_size);
		resource->alloc = alloc;
		resource->is_transient = false;

		// grab temp staging memory
		auto staging = m_constant_ring_buf->allocate(requested_size);

		// copy data to staging
		auto dst = staging.mapped_memory();
		auto src = desc.data;
		std::memcpy(dst, src, desc.data_size);

		// schedule GPU-GPU copy to new version
		auto upload_func = [staging, alloc, requested_size](ID3D12GraphicsCommandList* cmdl)
		{
			cmdl->CopyBufferRegion(
				alloc.base_buffer(), alloc.offset_from_base(),
				staging.base_buffer(), staging.offset_from_base(), requested_size);
		};
		m_deferred_init_copies.push(upload_func);


	}
	// Write-once-read-once --> MSFT recommends Upload Heap
	// https://docs.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources
	else if (usage_cpu == UsageIntentCPU::eUpdateOnce && usage_gpu == UsageIntentGPU::eReadOncePerFrame)
	{
		// allocate from ring buffer
		auto alloc = m_constant_ring_buf->allocate(requested_size);
		resource->alloc = alloc;
		resource->is_transient = true;

		assert(alloc.mappable());
		std::memcpy(alloc.mapped_memory(), desc.data, desc.data_size);
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

DXBufferManager::InternalBufferResource* DXBufferManager::create_non_constant(const DXBufferDesc& desc)
{
	auto [handle, resource] = m_handles.get_next_free_handle();
	resource->is_constant = true;

	const auto& usage_cpu = desc.usage_cpu;
	const auto& usage_gpu = desc.usage_gpu;
	uint32_t requested_size = desc.element_count * desc.element_size;

	resource->is_rt_structure = desc.is_rt_structure;

	// GPU write (UAV)
	if (desc.usage_gpu == UsageIntentGPU::eWrite)
	{
		if (desc.is_rt_structure)
			resource->alloc = std::move(m_committed_def_ator->allocate(desc.element_count, desc.element_size, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
		else
			resource->alloc = std::move(m_committed_def_ator->allocate(desc.element_count, desc.element_size, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));

		resource->is_transient = false;

		if (desc.data && desc.data_size > 0)
		{
			// grab temp staging memory
			auto staging = m_committed_upload_ator->allocate(desc.element_count, desc.element_size);

			// copy data to staging
			auto dst = staging.mapped_memory();
			auto src = desc.data;
			std::memcpy(dst, src, desc.data_size);

			// schedule GPU-GPU copy to new version
			auto upload_func = [staging, alloc = resource->alloc, requested_size](ID3D12GraphicsCommandList* cmdl)
			{
				cmdl->CopyBufferRegion(
					alloc.base_buffer(), alloc.offset_from_base(),
					staging.base_buffer(), staging.offset_from_base(), requested_size);
			};
			m_deferred_init_copies.push(upload_func);

			// remove staging later (we need to guarantee GPU-GPU copy)
			auto del_func = [this, staging]() mutable
			{
				m_committed_upload_ator->deallocate(std::move(staging));
			};
			m_deletion_queue.push({ m_curr_frame_idx, del_func });		// defer destruction of staging until next frame
		}
	}
	// Immutable
	else if (desc.usage_cpu == UsageIntentCPU::eUpdateNever && desc.usage_gpu != UsageIntentGPU::eInvalid)
	{
		resource->alloc = std::move(m_committed_def_ator->allocate(desc.element_count, desc.element_size));
		resource->is_transient = false;

		if (desc.data && desc.data_size > 0)
		{
			// grab temp staging memory
			auto staging = m_committed_upload_ator->allocate(desc.element_count, desc.element_size);

			// copy data to staging
			auto dst = staging.mapped_memory();
			auto src = desc.data;
			std::memcpy(dst, src, desc.data_size);

			// schedule GPU-GPU copy to new version
			auto upload_func = [staging, alloc = resource->alloc, requested_size](ID3D12GraphicsCommandList* cmdl)
			{
				cmdl->CopyBufferRegion(
					alloc.base_buffer(), alloc.offset_from_base(),
					staging.base_buffer(), staging.offset_from_base(), requested_size);
			};
			m_deferred_init_copies.push(upload_func);

			// remove staging later (we need to guarantee GPU-GPU copy)
			auto del_func = [this, staging]() mutable
			{
				m_committed_upload_ator->deallocate(std::move(staging));
			};
			m_deletion_queue.push({ m_curr_frame_idx, del_func });		// defer destruction of staging until next frame
		}
	}

	else
		assert(false);		// other types not supported for now


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
	else if (res->usage_cpu == UsageIntentCPU::eUpdateSometimes)
	{
		//m_constant_persistent_buf->deallocate(std::move(res->alloc));
		m_constant_persistent_bufs[res->frame_idx_allocation]->deallocate(std::move(res->alloc));
		m_handles.free_handle(res->handle);
	}
	else
		assert(false);
}



