#include "pch.h"
#include "DXConstantRingBuffer.h"
#include "DXBufferMemPool.h"

DXConstantRingBuffer::DXConstantRingBuffer(cptr<ID3D12Device> dev) :
	m_dev(dev.Get())
{
	//constexpr auto element_count_256 = 100;
	//constexpr auto element_count_512 = element_count_256 / 2;
	//constexpr auto element_count_1024 = element_count_512 / 2;
	//m_all_allocations.reserve(element_count_256 + element_count_512 + element_count_1024);

	//D3D12_DESCRIPTOR_HEAP_DESC dhd{};
	//dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	//dhd.NumDescriptors = element_count_256 + element_count_512 + element_count_1024;
	//auto hr = dev->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(m_desc_heap.GetAddressOf()));
	//if (FAILED(hr))
	//	assert(false);

	//D3D12_CPU_DESCRIPTOR_HANDLE descriptor_start = m_desc_heap->GetCPUDescriptorHandleForHeapStart();
	//m_pools.push_back(init_pool(dev.Get(), 256, element_count_256, descriptor_start));
	//m_pools.push_back(init_pool(dev.Get(), 512, element_count_512, descriptor_start));
	//m_pools.push_back(init_pool(dev.Get(), 1024, element_count_1024, descriptor_start));


	auto pool_infos = 
	{
			thingy::PoolInfo(1, 256, 100),
			thingy::PoolInfo(1, 512, 50),
			thingy::PoolInfo(1, 1024, 25),
	};
	m_thingy = std::make_unique<thingy>(dev.Get(), pool_infos);
	std::cout << "yay!\n";
}

void DXConstantRingBuffer::frame_begin(uint32_t frame_idx)
{
	m_curr_frame_idx = frame_idx;
	/*
		Since we are using this a ring buffer, the expected layout of the allocations are (in frame indices):
		
		assuming 3 FIFs:
		4 buffers allocated on frame 0
		3 buffers allocated on frame 1
		3 buffers allocated on frame 2
		1 buffer allocated on frame 3 (idx 0)
		
		[0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, ...]

		No disjoint allocations! This should be ensured by this API given that a queue is used and the nature of asking for a memory chunk from this class
	*/

	//// invalidate memory previoiusly used for the frame index and return memory to free list
	//while (!m_allocations_in_use.empty())
	//{
	//	auto alloc_in_use = m_allocations_in_use.front();
	//	if (alloc_in_use->m_frame_idx == frame_idx)
	//	{
	//		// invalidate handle to block user from accidental re-use
	//		alloc_in_use->m_valid = false;

	//		// return this allocation to the corresponding pool
	//		auto& allocations = m_pools_and_available_allocations[alloc_in_use->m_pool_idx].second;
	//		allocations.push(alloc_in_use);
	//		m_allocations_in_use.pop();
	//	}
	//	else
	//		break;
	//}


	// invalidate memory previoiusly used for the frame index and return memory to free list
	while (!m_thingy->allocations_in_use.empty())
	{
		auto alloc_in_use = m_thingy->allocations_in_use.front();
		if (alloc_in_use->m_frame_idx == frame_idx)
		{
			// invalidate handle to block user from accidental re-use
			alloc_in_use->m_valid = false;

			// return this allocation to the corresponding pool
			auto& allocations = m_thingy->pools_and_available_allocations[alloc_in_use->m_pool_idx].second;
			allocations.push(alloc_in_use);
			m_thingy->allocations_in_use.pop();
		}
		else
			break;
	}
}

DXConstantSuballocation* DXConstantRingBuffer::allocate(uint32_t requested_size)
{
	for (auto i = 0; i < m_thingy->pools_and_available_allocations.size(); ++i)
	{
		auto& [pool, available_allocs] = m_thingy->pools_and_available_allocations[i];
		if (pool->get_allocation_size() < requested_size)
			continue;	// move on to find another pool which fits size
		
		auto alloc = available_allocs.front();
		alloc->m_frame_idx = m_curr_frame_idx;		// set which frame this allocation was used on
		available_allocs.pop();
	
		// track allocation internally
		m_thingy->allocations_in_use.push(alloc);

		return alloc;
	}

	assert(false);		// we'll handle this later (e.g adding a new pool)
	return nullptr;
}

uptr<DXBufferMemPool>  DXConstantRingBuffer::init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_CPU_DESCRIPTOR_HANDLE& base_handle)
{
	auto pool = std::make_unique<DXBufferMemPool>(dev, element_size, num_elements, D3D12_HEAP_TYPE_UPLOAD);
	auto handle_size = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// initialize new pool + container for allocations entry
	// (SPECIFIC TO RING BUFFER)
	m_thingy->pools_and_available_allocations.push_back({ pool.get(), std::queue<DXConstantSuballocation*>() });
	auto& [_, available_allocs] = m_thingy->pools_and_available_allocations.back();

	// create descriptor for each allocation and fill free allocations
	for (uint32_t i = 0; i < num_elements; ++i)
	{
		auto buf_suballoc = pool->allocate();
		if (!buf_suballoc)
			assert(false);

		// grab desc handle
		auto hdl = base_handle;
		hdl.ptr += i * handle_size;

		// create view
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbd{};
		cbd.BufferLocation = buf_suballoc->get_gpu_adr();
		cbd.SizeInBytes = buf_suballoc->get_size();
		dev->CreateConstantBufferView(&cbd, hdl);

		// init allocation
		DXConstantSuballocation constant_suballoc{};
		constant_suballoc.m_frame_idx = -1;
		constant_suballoc.m_memory = buf_suballoc;
		constant_suballoc.m_valid = false;
		constant_suballoc.m_cpu_descriptor = hdl;
		constant_suballoc.m_pool_idx = (uint8_t)m_thingy->pools_and_available_allocations.size() - 1;

		// store allocation internally
		m_thingy->all_allocations.push_back(constant_suballoc);

		// work with pointers when distributing
		// (SPECIFFIC TO RING BUFFER)
		available_allocs.push(&m_thingy->all_allocations.back());
	}

	base_handle.ptr += num_elements * handle_size;
	return pool;
}
