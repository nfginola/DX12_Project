#include "pch.h"
#include "DXDescriptorHeapGPU.h"

DXDescriptorHeapGPU::DXDescriptorHeapGPU(cptr<ID3D12Device> dev, D3D12_DESCRIPTOR_HEAP_TYPE type) :
	m_dev(dev),
	m_type(type)
{
	constexpr auto max_descriptors = 6000;

	m_base_main_pool = std::make_unique<DXDescriptorPool>(dev, type, max_descriptors, true);
	// static part first half
	auto static_alloc = m_base_main_pool->allocate(max_descriptors / 2);	
	// dynamic part second half
	auto dyn_alloc = m_base_main_pool->allocate(max_descriptors / 2);

	// steal allocations and sub-manage
	m_static_part = std::make_unique<DXDescriptorPool>(dev, std::move(static_alloc), type);
	m_dynamic_part = std::make_unique<DXDescriptorPool>(dev, std::move(dyn_alloc), type);
}

void DXDescriptorHeapGPU::begin_frame(uint32_t frame_idx)
{
	m_frame_idx = frame_idx;

	while (!m_active_dynamic_allocs2.empty())
	{
		auto& alloc_in_use = m_active_dynamic_allocs2.front();
		if (alloc_in_use.frame_idx == frame_idx)
		{
			// remove from internal tracker
			m_active_dynamic_allocs2.pop();

			m_dynamic_part->deallocate(std::move(alloc_in_use.alloc));
		}
		else
			break;
	}

}

DXDescriptorAllocation DXDescriptorHeapGPU::allocate_static(uint32_t num_descriptors)
{
	auto to_ret = m_static_part->allocate(num_descriptors);
	if (to_ret.num_descriptors() == 0)
		assert(false);	// please increase size manually

	// tag allocation
	m_active_static_allocs.insert(to_ret.gpu_handle().ptr);
	return to_ret;
}

DXDescriptorAllocation DXDescriptorHeapGPU::allocate_dynamic(uint32_t num_descriptors)
{
	auto to_ret = m_dynamic_part->allocate(num_descriptors);
	if (to_ret.num_descriptors() == 0)
		assert(false);	// please increase size manually

	// tag allocation
	m_active_dynamic_allocs.insert(to_ret.gpu_handle().ptr);

	DynamicElement el{};
	el.alloc = to_ret;
	el.frame_idx = m_frame_idx;
	m_active_dynamic_allocs2.push(std::move(el));

	return to_ret;
}

void DXDescriptorHeapGPU::deallocate_static(DXDescriptorAllocation&& alloc)
{
	auto it = m_active_static_allocs.find(alloc.gpu_handle().ptr);
	if (it == m_active_static_allocs.cend())
		assert(false);		// couldnt find, programmer error

	m_static_part->deallocate(std::move(alloc));
}

ID3D12DescriptorHeap* DXDescriptorHeapGPU::get_desc_heap() const
{
	return m_base_main_pool->get_desc_heap();
}
