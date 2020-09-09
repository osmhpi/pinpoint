#pragma once

#include "Sampler.h"
#include "ProgArgs.h"

#include <vector>

class Experiment
{
public:
	struct Result
	{
		using time_res = std::chrono::duration<double, std::ratio<1>>;
		Sampler::result_t samples;
		time_res workload_wall_time;
	};

	Experiment(const ProgArgs & args) :
	    m_args(args)
	{
		;;
	}

	void run();
	void printResult();


private:
	const ProgArgs & m_args;
	std::vector<Result> m_results;

	Result run_single();
	double calcResult(const Result & res, const int sample_index);
};
