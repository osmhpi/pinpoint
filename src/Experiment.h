#pragma once

#include "Sampler.h"
#include "Settings.h"

#include <vector>

class Experiment
{
public:
	struct Result
	{
		using time_res = std::chrono::duration<double, std::ratio<1>>;
		Sampler::result_t energyBySource;
		time_res workload_wall_time;
	};

	void run();
	void printResult();


private:
	std::vector<Result> m_results;

	Result run_single();
	double calcResult(const Result & res, const int sample_index);
};
