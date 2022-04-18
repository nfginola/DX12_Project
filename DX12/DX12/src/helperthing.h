#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>
#include <memory>
#include "DXConstantSuballocation.h"
#include "DXBufferMemPool.h"
#include <tuple>
#include <queue>
#include <initializer_list>

struct thingy
{
	struct PoolInfo
	{
		uint32_t num_pools = 0;
		uint32_t element_size = 0;
		uint32_t num_elements = 0;

		PoolInfo(uint32_t num_pools_, uint32_t element_size_, uint32_t num_elements_) : num_pools(num_pools_), element_size(element_size_), num_elements(num_elements_) {}
	};

	using pools_and_allocations_t = std::vector<std::pair<DXBufferMemPool*, std::queue<DXConstantSuballocation*>>>;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> desc_heap;		// Non shader visible descriptor heaps with CBVs
	std::vector<DXConstantSuballocation> all_allocations;		// Storage for all allocations
	std::vector<std::unique_ptr<DXBufferMemPool>> pools;
	pools_and_allocations_t pools_and_available_allocations;
	std::queue<DXConstantSuballocation*> allocations_in_use;



	thingy(ID3D12Device* dev, std::initializer_list<thingy::PoolInfo> pool_infos_list)
	{
		std::vector<PoolInfo> pool_infos{ pool_infos_list.begin(), pool_infos_list.end() };
		uint64_t descriptors_needed = 0;
		for (auto& pool_info : pool_infos)
			descriptors_needed += pool_info.num_elements * pool_info.num_pools;

		D3D12_DESCRIPTOR_HEAP_DESC dhd{};
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		dhd.NumDescriptors = (UINT)descriptors_needed;
		auto hr = dev->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(desc_heap.GetAddressOf()));
		if (FAILED(hr))
			assert(false);

		// reserve upfront to hinder reallocation and moving of data (invalidates our pointers)
		all_allocations.reserve(descriptors_needed);

		// initialize pools
		auto handle = desc_heap->GetCPUDescriptorHandleForHeapStart();
		for (auto& pool_info : pool_infos)
			for (uint32_t i = 0; i < pool_info.num_pools; ++i)
				init_pool(dev, pool_info.element_size, pool_info.num_elements, handle);

	}

	~thingy() {}

	void init_pool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, D3D12_CPU_DESCRIPTOR_HANDLE& base_handle)
	{
		auto pool = std::make_unique<DXBufferMemPool>(dev, element_size, num_elements, D3D12_HEAP_TYPE_UPLOAD);
		auto handle_size = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// initialize new pool + container for allocations entry
		// (SPECIFIC TO RING BUFFER)
		pools_and_available_allocations.push_back({ pool.get(), std::queue<DXConstantSuballocation*>() });
		auto& [_, available_allocs] = pools_and_available_allocations.back();

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
			constant_suballoc.m_pool_idx = (uint8_t)pools_and_available_allocations.size() - 1;

			// store allocation internally
			all_allocations.push_back(constant_suballoc);

			// work with pointers when distributing
			available_allocs.push(&all_allocations.back());
		}

		pools.push_back(std::move(pool));

		base_handle.ptr += num_elements * handle_size;
	}
};
