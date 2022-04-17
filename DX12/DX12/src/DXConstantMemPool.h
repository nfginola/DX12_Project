#pragma once
#include <d3d12.h>
#include <queue>
#include <set>
#include <optional>

class DXConstantMemPool
{
public:
	using allocation_id_t = uint32_t;

	// Calling app should NOT modify these members, only use them
	// Maybe its wise to put getters and make this struct a friend of DXConstantMemPool (?)
	struct Allocation
	{
		allocation_id_t allocation_id = 0;

		D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle{};
		uint8_t* mapped_memory = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS gpu_address{};
	};

public:
	DXConstantMemPool(ID3D12Device* dev, uint16_t element_size, uint32_t num_elements, uint64_t handle_size);

	std::optional<Allocation> allocate();
	void deallocate(Allocation& alloc);


private:
	cptr<ID3D12DescriptorHeap> m_descriptors;		// Non-shader visible descriptor heap!
	D3D12_CPU_DESCRIPTOR_HANDLE m_base_cpu_desc_handle;

	cptr<ID3D12Resource> m_constant_buffer;
	uint8_t* m_base_cpu_adr;
	D3D12_GPU_VIRTUAL_ADDRESS m_base_gpu_adr;

	// we should have a linked list
	/*
		struct Node
		{
			Node* next;
			Allocation alloc_data;
		}

		use std::queue - we simply push back resolved allocations
		by looking into std::queue impl. it doesnt seem to call shrink_to_fit which is good!
	*/

	std::set<allocation_id_t> m_allocations_in_use;			//	to verify that we are deallocating something that we should (e.g covering for free-after-free)
	std::queue<Allocation> m_free_allocations;


};