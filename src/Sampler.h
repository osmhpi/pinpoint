#pragma once

#include <chrono>
#include <functional>
#include <vector>

#include "data_sources/MCP_EasyPower.h"
#include "data_sources/JetsonCounter.h"

struct SamplerDetail;

struct Sampler
{
	using result_t = std::vector<PowerDataSource::accumulate_t>;
	std::vector<PowerDataSourcePtr> devices;

	Sampler(std::chrono::milliseconds interval, const std::vector<std::string> & devNames, bool continuous_print_flag = false);
	virtual ~Sampler();

	void start(std::chrono::milliseconds delay = std::chrono::milliseconds(0));
	result_t stop(std::chrono::milliseconds delay = std::chrono::milliseconds(0));

	long ticks() const;

private:
	SamplerDetail *m_detail;

	void run(std::function<void()> tick);

	void accumulate_tick();
	void continuous_print_tick();
};
