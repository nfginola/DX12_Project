#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <stdint.h>
#include <string>
#include <map>

class GPUProfiler
{
public:
	enum class QueueType
	{
		eDirectOrCompute,
		eCopy
	};

	struct ProfileData
	{
		std::string name;
		double sec_elapsed = 0;

		uint64_t start_tick = 0;
		uint64_t end_tick = 0;
		uint64_t gpu_freq = 0;

		uint32_t query_pair_idx = 0;
	};

public:
	GPUProfiler(ID3D12Device* dev, QueueType queue_type, uint32_t max_fif, uint32_t max_profiles = 100);
	~GPUProfiler() = default;

	void profile_begin(ID3D12GraphicsCommandList* cmdl, ID3D12CommandQueue* queue, const std::string& name);
	void profile_end(ID3D12GraphicsCommandList* cmdl, const std::string& name);

	const std::map<std::string, ProfileData>& get_profiles();

	void frame_begin(uint32_t frame_idx);
	void frame_end(ID3D12GraphicsCommandList* cmdl);

	const GPUProfiler::ProfileData& get_curr_scope_profile();

private:
	static constexpr size_t QUERY_SIZE = sizeof(UINT64);	// Timestamp ticks are UINT64 https://docs.microsoft.com/en-us/windows/win32/direct3d12/timing

	uint32_t m_max_fif;
	uint32_t m_max_profiles_per_frame;
	uint32_t m_max_queries_per_frame;
	uint32_t m_curr_frame_idx;
	ProfileData* m_curr_scope_profile;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_readback_buf;
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_qheap;
	std::map<std::string, ProfileData> m_profiles;

	bool m_in_frame;
};
