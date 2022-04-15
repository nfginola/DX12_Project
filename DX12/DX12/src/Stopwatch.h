#pragma once
#include <chrono>

class Stopwatch
{
public:
	enum class Unit
	{
		eSecond,
		eMillisecond
	};

public:
	Stopwatch() = default;
	~Stopwatch() = default;

	void start();
	void stop();
	double elapsed(Unit unit = Unit::eSecond);
	bool running() const;

private:
	std::chrono::steady_clock::time_point m_start, m_end;
	bool m_running = false;
};

