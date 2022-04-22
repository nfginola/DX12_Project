#include "pch.h"
#include "DXDescriptorPool.h"

DXDescriptorPool::DXDescriptorPool(cptr<ID3D12Device> dev, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t max_descriptors, bool gpu_visible) :
	m_dev(dev),
	m_heap_type(type),
	m_gpu_visible(gpu_visible),
	m_max_descriptors(max_descriptors),
	m_handle_size(dev->GetDescriptorHandleIncrementSize(type))
{
	assert(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);

	D3D12_DESCRIPTOR_HEAP_DESC dhd{};
	dhd.Type = type;
	dhd.NumDescriptors = max_descriptors;
	dhd.Flags = gpu_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	auto hr = dev->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(m_desc_heap.GetAddressOf()));
	if (FAILED(hr))
		assert(false);

	m_curr_cpu_unallocated_start = m_desc_heap->GetCPUDescriptorHandleForHeapStart();
	m_curr_gpu_unallocated_start = m_desc_heap->GetGPUDescriptorHandleForHeapStart();

	DescriptorChunk full_chunk{};
	full_chunk.cpu_start = m_desc_heap->GetCPUDescriptorHandleForHeapStart();;
	full_chunk.gpu_start = m_desc_heap->GetGPUDescriptorHandleForHeapStart();
	full_chunk.num_descriptors = max_descriptors;
	//m_free_chunks.push_back(full_chunk);


	m_free_chunks.push_front(full_chunk);
}

DXDescriptorAllocation DXDescriptorPool::allocate(uint32_t num_requested_descriptors)
{
	{
		//for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it)
		//{
		//	auto& free_chunk = *it;
		//	if (free_chunk.taken)
		//		continue;

		//	// first fit
		//	if (free_chunk.num_descriptors == num_requested_descriptors)
		//	{
		//		free_chunk.taken = true;
		//		return DXDescriptorAllocation(free_chunk.cpu_start, free_chunk.gpu_start, m_handle_size, num_requested_descriptors);
		//	}
		//	else if (free_chunk.num_descriptors > num_requested_descriptors)
		//	{
		//		DescriptorChunk new_chunk{};
		//		new_chunk.cpu_start = free_chunk.cpu_start;
		//		new_chunk.gpu_start = free_chunk.gpu_start;
		//		new_chunk.num_descriptors = num_requested_descriptors;
		//		new_chunk.taken = true;

		//		m_chunks.insert(it, new_chunk);

		//		// move the free chunk
		//		free_chunk.cpu_start.ptr += num_requested_descriptors * m_handle_size;
		//		free_chunk.gpu_start.ptr += num_requested_descriptors * m_handle_size;
		//		free_chunk.num_descriptors -= num_requested_descriptors;

		//		return DXDescriptorAllocation(new_chunk.cpu_start, new_chunk.gpu_start, m_handle_size, num_requested_descriptors);


		//	}
		//	else
		//		continue;
		//}

		//assert(false);
		//return {};
	}
	
	for (auto& chunk : m_free_chunks)
	{
		if (chunk.num_descriptors == num_requested_descriptors)
		{
			return DXDescriptorAllocation(chunk.cpu_start, chunk.gpu_start, m_handle_size, num_requested_descriptors);
		}
		else if (chunk.num_descriptors > num_requested_descriptors)
		{
			// grab memory
			auto to_ret = DXDescriptorAllocation(chunk.cpu_start, chunk.gpu_start, m_handle_size, num_requested_descriptors);

			// move the free chunk
			chunk.cpu_start.ptr += num_requested_descriptors * m_handle_size;
			chunk.gpu_start.ptr += num_requested_descriptors * m_handle_size;
			chunk.num_descriptors -= num_requested_descriptors;

			return to_ret;
		}
		else
			continue;
	}

	assert(false);
	return {};
}

void DXDescriptorPool::deallocate(DXDescriptorAllocation&& alloc)
{
	{
		//const auto alloc_gpu_end = alloc.gpu_handle().ptr + alloc.num_descriptors() * m_handle_size;
		//const auto alloc_gpu_start = alloc.gpu_handle().ptr;
		//for (auto it = m_chunks.begin(); it != m_chunks.end(); ++it)
		//{
		//	auto& chunk = *it;

		//	// if we found that current chunk is at the end of our deallocation, merge them
		//	if (alloc_gpu_end == chunk.gpu_start.ptr)
		//	{
		//		if (chunk.taken)
		//			break;

		//		chunk.taken = false;
		//		chunk.cpu_start = alloc.cpu_handle();
		//		chunk.gpu_start = alloc.gpu_handle();
		//		chunk.num_descriptors += alloc.num_descriptors();

		//		// remove element behind
		//		auto prev_it = std::next(it, -1);
		//		m_chunks.erase(prev_it);
		//		break;
		//	}
		//	// if we found that current chunk is at the beginning of our deallocation, merge them
		//	else if (alloc_gpu_start == chunk.gpu_start.ptr + chunk.num_descriptors * m_handle_size)
		//	{
		//		if (chunk.taken)
		//			break;

		//		chunk.num_descriptors += alloc.num_descriptors();
		//		chunk.taken = false;

		//		// remove element in front
		//		auto next_it = std::next(it, 1);
		//		m_chunks.erase(next_it);
		//		break;
		//	}
		//}

		//const auto alloc_gpu_end = alloc.gpu_handle().ptr + alloc.num_descriptors() * m_handle_size;
		//const auto alloc_gpu_start = alloc.gpu_handle().ptr;

		//auto it_start = m_chunks.begin();
		//std::advance(it_start, 1);
		//for (auto it = it_start; it != m_chunks.end(); ++it)
		//{
		//	const auto left_chunk_it = std::next(it, -1);
		//	const auto right_chunk_it = it;

		//	auto& left_chunk = *left_chunk_it;
		//	auto& right_chunk = *right_chunk_it;

		//	bool merged_with_left = false;
		//	// merge with left
		//	if (alloc_gpu_end == left_chunk.gpu_start.ptr && !left_chunk.taken )
		//	{
		//		merged_with_left = true;

		//		left_chunk.taken = false;
		//		left_chunk.cpu_start = alloc.cpu_handle();
		//		left_chunk.gpu_start = alloc.gpu_handle();
		//		left_chunk.num_descriptors += alloc.num_descriptors();

		//		// remove element behind (since we merged)
		//		// m_chunks.erase(left_chunk_it);
		//	}

		//	// merge with right
		//	if (alloc_gpu_start == right_chunk.gpu_start.ptr + right_chunk.num_descriptors * m_handle_size && !right_chunk.taken)
		//	{
		//		if (merged_with_left)
		//		{
		//			left_chunk.num_descriptors += right_chunk.num_descriptors;
		//			m_chunks.erase(right_chunk_it);
		//		}
		//		else
		//		{
		//			right_chunk.taken = false;
		//			right_chunk.cpu_start = alloc.cpu_handle();
		//			right_chunk.gpu_start = alloc.gpu_handle();
		//			right_chunk.num_descriptors += alloc.num_descriptors();
		//		}
		//	}
		//}
	}

	
	const auto alloc_gpu_start = alloc.gpu_handle().ptr;
	const auto alloc_gpu_end = alloc.gpu_handle().ptr + alloc.num_descriptors() * m_handle_size;

	auto it_start = m_free_chunks.begin();

	if (m_free_chunks.size() > 1)
	{
		std::advance(it_start, 1);
		for (auto it = it_start; it != m_free_chunks.end(); ++it)
		{
			const auto left_chunk_it = std::next(it, -1);
			const auto right_chunk_it = it;
			auto& left_chunk = *left_chunk_it;
			auto& right_chunk = *right_chunk_it;

			const auto left_chunk_end = left_chunk.gpu_start.ptr + left_chunk.num_descriptors * m_handle_size;
			const auto right_chunk_start = right_chunk.gpu_start.ptr;

			bool merge_left = alloc_gpu_start == left_chunk_end;
			bool merge_right = alloc_gpu_end == right_chunk_start;

			if (merge_left && merge_right)
			{
				left_chunk.num_descriptors += alloc.num_descriptors() + right_chunk.num_descriptors;

				// merged with left
				m_free_chunks.erase(right_chunk_it);
				break;
			}
			else if (merge_left)
			{
				left_chunk.num_descriptors += alloc.num_descriptors();
				break;
			}
			else if (merge_right)
			{
				right_chunk.cpu_start = alloc.cpu_handle();
				right_chunk.gpu_start = alloc.gpu_handle();
				right_chunk.num_descriptors += alloc.num_descriptors();
				break;
			}
			else
				assert(false);
		}
	}
	else
	{
		auto& chunk = *it_start;

		bool merge_left = alloc_gpu_start == chunk.gpu_start.ptr + chunk.num_descriptors * m_handle_size;
		bool merge_right = alloc_gpu_end == chunk.gpu_start.ptr;

		bool adr_before = alloc.gpu_handle().ptr < chunk.gpu_start.ptr;

		if (merge_left)
		{
			chunk.num_descriptors += alloc.num_descriptors();
		}
		else if (merge_right)
		{
			chunk.num_descriptors += alloc.num_descriptors();
			chunk.cpu_start = alloc.cpu_handle();
			chunk.gpu_start = alloc.gpu_handle();
		}
		else
		{
			DescriptorChunk disjoint_chunk{};
			disjoint_chunk.cpu_start = alloc.cpu_handle();
			disjoint_chunk.gpu_start = alloc.gpu_handle();
			disjoint_chunk.num_descriptors = alloc.num_descriptors();

			if (adr_before)
				m_free_chunks.push_front(disjoint_chunk);
			else
				m_free_chunks.push_back(disjoint_chunk);
		}
	}
}
