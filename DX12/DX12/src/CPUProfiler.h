#pragma once
#include "Stopwatch.h"
#include <string>
#include <map>
#include <array>

class CPUProfiler
{
public:
	static constexpr uint64_t MAX_FRAME_LATENCY = 5;	// Holds as far as 5 frames back of CPU times
public:
	struct ProfileData
	{
	public:
		std::string name;
		double sec_elapsed = 0.0;

	private:
		friend class CPUProfiler;
		std::array<Stopwatch, MAX_FRAME_LATENCY> stopwatch;		// Impl. detail
	};

public:
	CPUProfiler(uint8_t latency);
	~CPUProfiler() = default;

	void profile_begin(const std::string& name);
	void profile_end(const std::string& name);

	const std::map<std::string, ProfileData>& get_profiles();

	const CPUProfiler::ProfileData& get_curr_scope_profile();

	void frame_begin();
	void frame_end();

private:
	std::map<std::string, ProfileData> m_profiles;
	ProfileData* m_curr_scope_profile;
	bool m_in_frame;
	uint64_t m_curr_frame;
	uint8_t m_latency;

};

