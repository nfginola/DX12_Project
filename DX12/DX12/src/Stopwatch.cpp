#include "pch.h"
#include "Stopwatch.h"

void Stopwatch::start()
{
	assert(!m_running);
	m_start = m_end = std::chrono::high_resolution_clock::now();
	m_running = true;
}

void Stopwatch::stop()
{
	assert(m_running);
	m_end = std::chrono::high_resolution_clock::now();
	m_running = false;
}

double Stopwatch::elapsed(Unit unit)
{
	assert(!m_running);
	std::chrono::duration<double> diff = m_end - m_start;

	switch (unit)
	{
	case Unit::eSecond:
		return diff.count();
	case Unit::eMillisecond:
		return diff.count() * 1000.0;
	default:
		return diff.count();
	}

	m_running = false;
}

bool Stopwatch::running() const
{
	return m_running;
}
