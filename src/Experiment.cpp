#include "Experiment.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

void Experiment::run()
{
	for (unsigned int i = 0; i < settings::runs; i++) {
		if (settings::continuous_print_flag && settings::runs > 1)
			std::cout << "### Run " << i << std::endl;
		m_results.push_back(run_single());
		std::this_thread::sleep_for(settings::delay);
	}
}

void Experiment::printResult()
{
	if (settings::continuous_print_flag)
		return;

	std::cerr << "Energy counter stats for '";
	for (char **a = settings::workload_and_args; a && *a; ++a) {
		std::cerr << *a << " ";
	}

	std::cerr << "\b':" << std::endl;
	std::cerr << "[interval: " << settings::interval.count() << "ms, before: "
							   << settings::before.count() << "ms, after: "
							   << settings::after.count() << "ms, delay: "
							   << settings::delay.count() << "ms, runs: "
							   << settings::runs << "]" << std::endl;
	std::cerr << std::endl;

	std::vector<double> means(settings::counters.size(), 0);
	double mean_time = 0;
	for (const auto & res: m_results) {
		for (size_t i = 0; i < means.size(); i++) {
			means[i] += calcResult(res, i) / settings::runs;
		}
		mean_time += res.workload_wall_time.count() / settings::runs;
	}

	std::vector<double> variances(settings::counters.size(), 0);
	double variance_time = 0;
	for (const auto & res: m_results) {
		for (size_t i = 0; i < variances.size(); i++) {
			double vi = calcResult(res, i) - means[i];
			variances[i] += vi * vi / settings::runs;
		}

		double vti = res.workload_wall_time.count() - mean_time;
		variance_time += vti * vti / settings::runs;
	}

	std::vector<double> stddev_percents(settings::counters.size(), 0);
	for (size_t i = 0; i < stddev_percents.size(); i++) {
		stddev_percents[i] = (std::sqrt(variances[i]) / means[i]) * 100;
	}
	double stddev_percent_time = (std::sqrt(variance_time) / mean_time) * 100;

	for (size_t i = 0; i < means.size(); i++) {
		std::cerr << "\t"
			<< std::fixed << std::setprecision(2)
			<< means[i] << " " << settings::unit << "\t"
			<< settings::counters[i];
		if (settings::runs > 1) std::cerr << "\t"
			<< "( +- " << stddev_percents[i] << "% )";

		std::cerr << std::endl;
	}
	std::cerr << std::endl;

	std::cerr << "\t"
		<< std::fixed << std::setprecision(8)
		<< mean_time << " seconds time elapsed ";
	if (settings::runs > 1) std::cerr
		<< std::fixed << std::setprecision(2)
		<< "( +- " << stddev_percent_time << "% )";
	std::cerr << std::endl;

	std::cerr << std::endl;
}

Experiment::Result Experiment::run_single()
{
	Sampler sampler(settings::interval, settings::counters, settings::continuous_print_flag);

	if (settings::before.count() > 0) {
		sampler.start();
		std::this_thread::sleep_for(settings::before);
	}

	auto start_time = std::chrono::high_resolution_clock::now();
	pid_t workload;
	if ((workload = fork())) {
		sampler.start(std::max(-settings::before, std::chrono::milliseconds(0)));
		waitpid(workload, NULL, 0);
	} else {
		execvp(settings::workload_and_args[0], settings::workload_and_args);
	}

	auto end_time = std::chrono::high_resolution_clock::now();

	Result result;
	result.energyBySource = sampler.stop(std::chrono::milliseconds(settings::after));
	result.workload_wall_time = std::chrono::duration_cast<Result::time_res>(end_time - start_time);
	return result;
}

double Experiment::calcResult(const Experiment::Result & res, const int counterIndex)
{
	using sec_double = std::chrono::duration<double, std::ratio<1>>;

	units::energy::joule_t energy = res.energyBySource[counterIndex];
	units::edp::joule_second_t edp = energy * as_unit_seconds(std::chrono::duration_cast<sec_double>(res.workload_wall_time));

	return settings::energy_delayed_product ? edp.to<double>() : energy.to<double>();
}
