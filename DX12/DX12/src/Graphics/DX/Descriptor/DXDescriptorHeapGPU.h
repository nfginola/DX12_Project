#pragma once
#include "DXDescriptorPool.h"
#include <set>
#include <queue>

/*
	Handles GPU-visible descriptor heaps
*/
class DXDescriptorHeapGPU
{

public:
	DXDescriptorHeapGPU(cptr<ID3D12Device> dev, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t max_descriptors = 2000);

	void frame_begin(uint32_t frame_idx);

	DXDescriptorAllocation allocate_static(uint32_t num_descriptors);
	void deallocate_static(DXDescriptorAllocation&& alloc);
	
	DXDescriptorAllocation allocate_dynamic(uint32_t num_descriptors);
	
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
	std::set<uint64_t> m_active_static_allocs;



	/*
		=====================================================================
		Below are ring buffer vars for the dynamic part (should refactor this into another class)

		Lets analyze how to properly divide the Dynamic Part for management SOME OTHER TIME!
		For now, lets assume single-thread usage and tackle only FIF.

		=====================================================================
	*/
	struct DynamicElement
	{
		uint32_t frame_idx;
		DXDescriptorAllocation alloc;
	};

	// due to the nature of transient descriptors, we may want to change the allocation strategy for the dynamic part in the future
	// if we can assume that everything is discarded every frame, we can employ better algorithms (e.g ring)
	uptr<DXDescriptorPool> m_dynamic_part;						
	std::set<uint64_t> m_active_dynamic_allocs;

	uint32_t m_frame_idx = 0;
	std::queue<DynamicElement> m_active_dynamic_allocs2;


};

