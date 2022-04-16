#include "pch.h"
#include "GPUProfiler.h"
#include <cassert>

template <typename T>
using cptr = Microsoft::WRL::ComPtr<T>;

GPUProfiler::GPUProfiler(ID3D12Device* dev, QueueType queue_type, uint8_t max_fif, uint32_t max_profiles) :
	m_max_fif(max_fif),
	m_max_profiles_per_frame(max_profiles),
	m_max_queries_per_frame(max_profiles * 2),
	m_curr_frame_idx(0),
	m_curr_scope_profile(nullptr),
	m_in_frame(false)
{
	const auto total_max_queries = max_profiles * 2 * max_fif;

	// Create query heap
	{
		D3D12_QUERY_HEAP_DESC desc{};
		desc.Type = queue_type == QueueType::eDirectOrCompute ? D3D12_QUERY_HEAP_TYPE_TIMESTAMP : D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP;
		desc.Count = total_max_queries;
		auto hr = dev->CreateQueryHeap(&desc, IID_PPV_ARGS(m_qheap.GetAddressOf()));
		if (FAILED(hr))
			assert(false);
	}

	// Create readback bufer
	{
		D3D12_HEAP_PROPERTIES hp{};
		hp.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC rd{};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		rd.Width = total_max_queries * QUERY_SIZE;
		rd.Height = 1;
		rd.DepthOrArraySize = 1;
		rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN;
		rd.SampleDesc.Count = 1;
		rd.SampleDesc.Quality = 0;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;	// requirement for buffers
		rd.Flags = D3D12_RESOURCE_FLAG_NONE;

		// Can be kept in copy dest state since we're always copying to it from ResolveQuery
		auto hr = dev->CreateCommittedResource(
			&hp, D3D12_HEAP_FLAG_NONE,
			&rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, 
			IID_PPV_ARGS(m_readback_buf.GetAddressOf()));
	}

}

void GPUProfiler::profile_begin(ID3D12GraphicsCommandList* cmdl, ID3D12CommandQueue* queue, const std::string& name)
{
	assert(m_profiles.size() < m_max_profiles_per_frame);
	assert(m_in_frame);
	GPUProfiler::ProfileData* profile = nullptr;
	if (auto it = m_profiles.find(name); it == m_profiles.cend())
	{
		auto& new_profile = m_profiles[name];
		new_profile.name = name;
		new_profile.query_pair_idx = (uint32_t)m_profiles.size() - 1;
		profile = &new_profile;
	}
	else
	{
		profile = &it->second;
	}
	
	profile->start_tick = profile->end_tick = 0;
	profile->sec_elapsed = 0.0;

	// query every time incase of changes (?)
	auto hr = queue->GetTimestampFrequency(&profile->gpu_freq);
	if (FAILED(hr))
		assert(false);

	const auto start_idx = (m_curr_frame_idx * m_max_queries_per_frame) + profile->query_pair_idx * 2 + 0;		// 0 for start in (start, end) pair
	cmdl->EndQuery(m_qheap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, start_idx);

	m_curr_scope_profile = profile;
}

void GPUProfiler::profile_end(ID3D12GraphicsCommandList* cmdl, const std::string& name)
{
	assert(m_in_frame);
	GPUProfiler::ProfileData* profile = nullptr;
	if (auto it = m_profiles.find(name); it == m_profiles.cend())
	{
		assert(false);
	}
	else
	{
		profile = &it->second;
	}

	const auto end_idx = (m_curr_frame_idx * m_max_queries_per_frame) + profile->query_pair_idx * 2 + 1;		// 1 for end in (start, end) pair
	cmdl->EndQuery(m_qheap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, end_idx);
}

const std::map<std::string, GPUProfiler::ProfileData>& GPUProfiler::get_profiles()
{
	assert(m_in_frame);
	return m_profiles;
}

void GPUProfiler::frame_begin(uint32_t frame_idx)
{
	assert(m_in_frame == false);
	m_in_frame = true;
	m_curr_frame_idx = frame_idx;

	// Map readback and store in profile
	const auto read_start = frame_idx * m_max_queries_per_frame * QUERY_SIZE;
	const auto read_end = read_start + m_profiles.size() * 2 * QUERY_SIZE;
	const D3D12_RANGE read_range{ read_start, read_end };
	uint64_t* readback_data = nullptr;
	auto hr = m_readback_buf->Map(0, &read_range, (void**)&readback_data);
	if (FAILED(hr))
		assert(false);
	
	// Offset to this frames readback part
	const uint32_t bytes_offset_to_frame = frame_idx * m_max_profiles_per_frame * 2;
	const uint64_t* const readback_data_this_frame = &readback_data[bytes_offset_to_frame];
	for (auto& [_, profile] : m_profiles)
	{
		profile.start_tick = readback_data_this_frame[profile.query_pair_idx * 2];
		profile.end_tick = readback_data_this_frame[profile.query_pair_idx * 2 + 1];

		const auto tick_dt = profile.end_tick - profile.start_tick;
		profile.sec_elapsed = tick_dt / (double)profile.gpu_freq;
	}

	D3D12_RANGE no_write{};
	m_readback_buf->Unmap(0, &no_write);
}

void GPUProfiler::frame_end(ID3D12GraphicsCommandList* cmdl)
{
	assert(m_in_frame == true);
		 
	// Resolve subquery to readback buffer
	cmdl->ResolveQueryData(m_qheap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
		m_curr_frame_idx * m_max_queries_per_frame,		// start idx
		(uint32_t)m_profiles.size() * 2,					// num queries to resolve
		m_readback_buf.Get(),					// target 
		m_curr_frame_idx * m_max_queries_per_frame * QUERY_SIZE);	// offset (in bytes) from target 

	m_in_frame = false;
}

const GPUProfiler::ProfileData& GPUProfiler::get_curr_scope_profile()
{
	return *m_curr_scope_profile;
}
