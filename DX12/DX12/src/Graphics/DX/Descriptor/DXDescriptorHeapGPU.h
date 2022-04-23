#pragma once
#include "DXDescriptorPool.h"
#include <set>

/*
	Handles GPU-visible descriptor heaps
*/
class DXDescriptorHeapGPU
{
public:
	DXDescriptorHeapGPU(cptr<ID3D12Device> dev, D3D12_DESCRIPTOR_HEAP_TYPE type);

	DXDescriptorAllocation allocate_static(uint32_t num_descriptors);
	DXDescriptorAllocation allocate_dynamic(uint32_t num_descriptors);
	
	void deallocate_static(DXDescriptorAllocation&& alloc);
	void deallocate_dynamic(DXDescriptorAllocation&& alloc);

	ID3D12DescriptorHeap* get_desc_heap() const;

private:
	cptr<ID3D12Device> m_dev;
	D3D12_DESCRIPTOR_HEAP_TYPE m_type;

	uptr<DXDescriptorPool> m_base_main_pool;

	/*
		[ ------ STATIC / INFREQUENTLY CHANGING SECTION ------ | ------ DYNAMIC SECTION (e.g per frame transient descriptors) ------ ]
	*/

	// These alloators steal ownership of allocations from the base main pool respectively and manage them 
	// Meaning the base main pool isn't used explicitly
	uptr<DXDescriptorPool> m_static_part;
	uptr<DXDescriptorPool> m_dynamic_part;

	// used to verify dealloations (we tag allocations)
	std::set<uint64_t> m_active_static_allocs;
	std::set<uint64_t> m_active_dynamic_allocs;


};

