#include "TegraDeviceInfo.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

/**********************************************************/

struct ProgArgs
{
	// Configurable via command line options
	bool energy_delayed_product;
	std::vector<std::string> devices;
	unsigned int runs;
	std::chrono::milliseconds delay;
	std::chrono::milliseconds interval;
	std::chrono::milliseconds before;
	std::chrono::milliseconds after;
	char **workload_and_args;

	// Inferred value
	std::string unit;

	ProgArgs(int argc, char *argv[]):
		energy_delayed_product(false),
		devices{"CPU", "GPU", "SOC", "DDR", "IN"},
		interval(500),
		runs(1),
		delay(0),
		before(0),
		after(0),
		workload_and_args(nullptr)
	{
		int c;
		while ((c = getopt (argc, argv, "hpe:r:d:i:b:a:")) != -1) {
			switch (c) {
				case 'h':
				case '?':
					printHelpAndExit(argv[0]);
					break;
				case 'p':
					energy_delayed_product = true;
					std::cerr << "Not implemented" << std::endl;
					exit(2);
					break;
				case 'e':
					std::cerr << "Not implemented" << std::endl;
					exit(2);
					break;
				case 'r':
					runs = atoi(optarg);
					if (runs < 1) {
						std::cerr << "Invalid number of runs" << std::endl;
						exit(1);
					}
					break;
				case 'd':
					delay = std::chrono::milliseconds(atoi(optarg));
					break;
				case 'i':
					interval = std::chrono::milliseconds(atoi(optarg));
					break;
				case 'a':
					after = std::chrono::milliseconds(atoi(optarg));
					break;
				case 'b':
					before = std::chrono::milliseconds(atoi(optarg));
					break;
				default:
					printHelpAndExit(argv[0], 1);
			}
		}

		if (!(optind < argc)) {
			std::cerr << "Missing workload" << std::endl;
			exit(1);
		}

		workload_and_args = &argv[optind];

		/////// Infer other stuff
		unit = energy_delayed_product ? "mJs" : "mJ";
	}

	void printHelpAndExit(char *progname, int exitcode = 0)
	{
		std::cout << "Usage: " << progname << "-h|[-p] -e dev1,dev2,... ([-r|-d|-i|-b|-a] N)* [--] workload [args]" << std::endl;
		std::cout << "\t-h Print this help and exit" << std::endl;
		std::cout << "\t-p Measure energy-delayed product (measure joules if not provided)" << std::endl;
		std::cout << "\t-e Comma seperated list of measured devices (default: all)" << std::endl;
		std::cout << "\t-r Number of runs (default: " << runs << ")" << std::endl;
		std::cout << "\t-d Delay between runs in ms (default: " << delay.count() << ")" << std::endl;
		std::cout << "\t-i Sampling interval in ms (default: " << interval.count() << ")" << std::endl;
		std::cout << "\t-b Start measurement N ms before worker creation (negative values will delay start)" << std::endl;
		std::cout << "\t-a Continue measurement N ms after worker exited" << std::endl;
		exit(exitcode);
	}

};

/**********************************************************/

struct Sampler
{
	using result_t = std::vector<TegraDeviceInfo::accumulate_t>;
	std::vector<TegraDeviceInfo> devices;

	Sampler(std::chrono::milliseconds interval, const std::vector<std::string> & devNames) :
		m_interval(interval),
		m_done(false),
		m_ticks(0)
	{
		devices.reserve(devNames.size());

		for (const auto & name: devNames) {
			if (!name.compare("CPU"))
				devices.emplace_back(TEGRA_CPU_DEV, name);
			else if (!name.compare("GPU"))
				devices.emplace_back(TEGRA_GPU_DEV, name);
			else if (!name.compare("SOC"))
				devices.emplace_back(TEGRA_SOC_DEV, name);
			else if (!name.compare("DDR"))
				devices.emplace_back(TEGRA_DDR_DEV, name);
			else if (!name.compare("IN"))
				devices.emplace_back(TEGRA_IN_DEV, name);
			else
				std::runtime_error("Unknown device \"" + name + "\"");
		}

		m_worker = std::thread([this]{ tick(); });
	}

	void start(std::chrono::milliseconds delay = std::chrono::milliseconds(0))
	{
		std::this_thread::sleep_for(delay);
		m_start_signal.notify_one();
	}

	result_t stop(std::chrono::milliseconds delay = std::chrono::milliseconds(0))
	{
		std::this_thread::sleep_for(delay);
		m_done.store(true);

		// Finish loop in case we didn't start
		m_start_signal.notify_one();
		m_worker.join();

		result_t result;
		std::transform(devices.cbegin(), devices.cend(),
			std::back_inserter(result), [](const TegraDeviceInfo & tdi) { return tdi.accumulator(); });
		return result;
	}

	long ticks() const
	{
		return m_ticks;
	}

private:
	std::chrono::milliseconds m_interval;
	std::thread m_worker;

	std::condition_variable m_start_signal;
	std::mutex m_start_mutex;

	std::atomic<bool> m_done;
	long m_ticks;

	void tick()
	{
		std::unique_lock<std::mutex> lk(m_start_mutex);
		m_start_signal.wait(lk);

		while (!m_done.load()) {
			// FIXME: tiny skid by scheduling + now(). Global start instead?
			auto entry = std::chrono::high_resolution_clock::now();
			for (auto & dev: devices) {
				dev.accumulate();
			}
			m_ticks++;
			std::this_thread::sleep_until(entry + m_interval);
		}
	}

};

/**********************************************************/

class Experiment
{
public:
	Experiment(const ProgArgs & args) :
		m_args(args)
	{
		;;
	}

	void run()
	{
		for (int i = 0; i < m_args.runs; i++) {
			m_results.push_back(run_single());
			std::this_thread::sleep_for(m_args.delay);
		}
	}

	void printResult()
	{
		std::cerr << "Tegra energy counter stats for '";
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

		std::vector<double> means(m_args.devices.size(), 0);
		for (const auto & res: m_results) {
			for (int i = 0; i < means.size(); i++) {
				means[i] += calcResult(res[i]) / m_args.runs;
			}
		}

		std::vector<double> variances(m_args.devices.size(), 0);
		for (const auto & res: m_results) {
			for (int i = 0; i < variances.size(); i++) {
				double vi = calcResult(res[i]) - means[i];
				variances[i] += vi * vi / m_args.runs;
			}
		}

		std::vector<double> stddev_percents(m_args.devices.size(), 0);
		for (int i = 0; i < stddev_percents.size(); i++) {
			stddev_percents[i] = (std::sqrt(variances[i]) / means[i]) * 100;
		}

		for (int i = 0; i < means.size(); i++) {
			std::cerr << "\t"
				<< std::fixed << std::setprecision(2)
				<< means[i] << " " << m_args.unit << "\t"
				<< m_args.devices[i];
			if (m_args.runs > 1) std::cerr << "\t"
				<< "( +- " << stddev_percents[i] << "% )";

			std::cerr << std::endl;
		}

		std::cerr << std::endl;
	}


private:
	const ProgArgs & m_args;
	std::vector<Sampler::result_t> m_results;

	Sampler::result_t run_single()
	{
		Sampler sampler(m_args.interval, m_args.devices);

		if (m_args.before.count() > 0) {
			sampler.start();
			std::this_thread::sleep_for(m_args.before);
		}

		pid_t workload;
		if ((workload = fork())) {
			sampler.start(std::max(-m_args.before, std::chrono::milliseconds(0)));
			waitpid(workload, NULL, 0);
		} else {
			execvp(m_args.workload_and_args[0], m_args.workload_and_args);
		}

		return sampler.stop(std::chrono::milliseconds(m_args.after));
	}

	double calcResult(TegraDeviceInfo::accumulate_t value)
	{
		// Assuming:
		//   * constant interval
		//   * accumulate_t contains sum of milli-Watt measurements
		//   * we take lower Darboux integral ... :( [since we measure at start of interval]

		// TODO: implement EDP
		auto interval_in_s = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(m_args.interval);
		return (double) value * interval_in_s.count();
	}
};

/**********************************************************/


int main(int argc, char *argv[])
{
	ProgArgs args(argc, argv);
	Experiment experiment(args);

	experiment.run();
	experiment.printResult();

	return 0;
}
