#include "Experiment.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <thread>

#include <sys/wait.h>

void Experiment::run()
{
	for (unsigned int i = 0; i < m_args.runs; i++) {
		if (m_args.continuous_print_flag && m_args.runs > 1)
			std::cout << "### Run " << i << std::endl;
		m_results.push_back(run_single());
		std::this_thread::sleep_for(m_args.delay);
	}
}

void Experiment::printResult()
{
	if (m_args.continuous_print_flag)
		return;

	std::cerr << "Energy counter stats for '";
	for (char **a = m_args.workload_and_args; a && *a; ++a) {
		std::cerr << *a << " ";
	}

	std::cerr << "\b':" << std::endl;
	std::cerr << "[interval: " << m_args.interval.count() << "ms, before: "
							   << m_args.before.count() << " ms, after: "
							   << m_args.after.count() << " ms, delay: "
							   << m_args.delay.count() << " ms, runs: "
							   << m_args.runs << "]" << std::endl;
	std::cerr << std::endl;

	std::vector<double> means(m_args.counters.size(), 0);
	double mean_time = 0;
	for (const auto & res: m_results) {
		for (size_t i = 0; i < means.size(); i++) {
			means[i] += calcResult(res, i) / m_args.runs;
		}
		mean_time += res.workload_wall_time.count() / m_args.runs;
	}

	std::vector<double> variances(m_args.counters.size(), 0);
	double variance_time = 0;
	for (const auto & res: m_results) {
		for (size_t i = 0; i < variances.size(); i++) {
			double vi = calcResult(res, i) - means[i];
			variances[i] += vi * vi / m_args.runs;
		}

		double vti = res.workload_wall_time.count() - mean_time;
		variance_time += vti * vti / m_args.runs;
	}

	std::vector<double> stddev_percents(m_args.counters.size(), 0);
	for (size_t i = 0; i < stddev_percents.size(); i++) {
		stddev_percents[i] = (std::sqrt(variances[i]) / means[i]) * 100;
	}
	double stddev_percent_time = (std::sqrt(variance_time) / mean_time) * 100;

	for (size_t i = 0; i < means.size(); i++) {
		std::cerr << "\t"
			<< std::fixed << std::setprecision(2)
			<< means[i] << " " << m_args.unit << "\t"
			<< m_args.counters[i];
		if (m_args.runs > 1) std::cerr << "\t"
			<< "( +- " << stddev_percents[i] << "% )";

		std::cerr << std::endl;
	}
	std::cerr << std::endl;

	std::cerr << "\t"
		<< std::fixed << std::setprecision(8)
		<< mean_time << " seconds time elapsed ";
	if (m_args.runs > 1) std::cerr
		<< std::fixed << std::setprecision(2)
		<< "( +- " << stddev_percent_time << "% )";
	std::cerr << std::endl;

	std::cerr << std::endl;
}

Experiment::Result Experiment::run_single()
{
	Sampler sampler(m_args.interval, m_args.counters, m_args.continuous_print_flag);

	if (m_args.before.count() > 0) {
		sampler.start();
		std::this_thread::sleep_for(m_args.before);
	}

	auto start_time = std::chrono::high_resolution_clock::now();
	pid_t workload;
	if ((workload = fork())) {
		sampler.start(std::max(-m_args.before, std::chrono::milliseconds(0)));
		waitpid(workload, NULL, 0);
	} else {
		execvp(m_args.workload_and_args[0], m_args.workload_and_args);
	}

	auto end_time = std::chrono::high_resolution_clock::now();

	Result result;
	result.samples = sampler.stop(std::chrono::milliseconds(m_args.after));
	result.workload_wall_time = std::chrono::duration_cast<Result::time_res>(end_time - start_time);
	return result;
}

double Experiment::calcResult(const Experiment::Result & res, const int sample_index)
{
	using sec_double = std::chrono::duration<double, std::ratio<1>>;
	// Assuming:
	//   * constant interval
	//   * accumulate_t contains sum of milli-Watt measurements
	//   * we take lower Darboux integral ... :( [since we measure at start of interval]

	PowerDataSource::accumulate_t power_sum_mW = res.samples[sample_index];
	double energy_mJ = (double) power_sum_mW * std::chrono::duration_cast<sec_double>(m_args.interval).count();
	double edp_mJs = energy_mJ * std::chrono::duration_cast<sec_double>(res.workload_wall_time).count();

	return m_args.energy_delayed_product ? edp_mJs : energy_mJ;
}
