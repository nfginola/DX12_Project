#include "pch.h"
#include "CPUProfiler.h"

CPUProfiler::CPUProfiler() :
	m_curr_scope_profile(nullptr),
	m_in_frame(false),
	m_curr_frame(0)
{
}

void CPUProfiler::profile_begin(const std::string& name)
{
	CPUProfiler::ProfileData* profile = nullptr;
	if (auto it = m_profiles.find(name); it == m_profiles.cend())
	{
		auto& new_profile = m_profiles[name];
		new_profile.name = name;
		profile = &new_profile;
	}
	else
	{
		profile = &it->second;
	}
	profile->sec_elapsed = 0.0;
	profile->stopwatch[m_curr_frame].start();
}

void CPUProfiler::profile_end(const std::string& name)
{
	assert(m_in_frame);
	CPUProfiler::ProfileData* profile = nullptr;
	if (auto it = m_profiles.find(name); it == m_profiles.cend())
	{
		assert(false);
	}
	else
	{
		profile = &it->second;
	}
	profile->stopwatch[m_curr_frame].stop();
}

const std::map<std::string, CPUProfiler::ProfileData>& CPUProfiler::get_profiles(uint64_t latency)
{
	assert(latency <= MAX_FRAME_LATENCY);
	uint64_t stopwatch_idx = (m_curr_frame + (MAX_FRAME_LATENCY - latency)) % MAX_FRAME_LATENCY;		// Wrap around to go backwards 'latency' frames

	// Resolve times for given frame latency
	for (auto& [_, profile] : m_profiles)
		profile.sec_elapsed = profile.stopwatch[stopwatch_idx].elapsed(Stopwatch::Unit::eSecond);

	return m_profiles;
}

const CPUProfiler::ProfileData& CPUProfiler::get_curr_scope_profile()
{
	return *m_curr_scope_profile;
}

void CPUProfiler::frame_begin()
{
	m_in_frame = true;
}

void CPUProfiler::frame_end()
{
	// Verify that the times recorded this frame have all been stopped
	for (const auto& [_, profile] : m_profiles)
		assert(!profile.stopwatch[m_curr_frame].running());

	m_in_frame = false;
	m_curr_frame = (m_curr_frame + 1) % CPUProfiler::MAX_FRAME_LATENCY;
}
