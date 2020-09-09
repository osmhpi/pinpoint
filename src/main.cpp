#include "data_sources/MCP_EasyPower.h"
#include "data_sources/JetsonCounter.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

/**********************************************************/

template<char delimiter>
class WordDelimitedBy : public std::string
{};

template<char delimiter>
std::istream& operator>>(std::istream& is, WordDelimitedBy<delimiter>& output)
{
	std::getline(is, output, delimiter);
	return is;
}

template<char delimiter>
std::vector<std::string> str_split(const std::string & text)
{
	std::istringstream iss(text);
	return std::vector<std::string>
			((std::istream_iterator<WordDelimitedBy<delimiter>>(iss)),
		      std::istream_iterator<WordDelimitedBy<delimiter>>());
}

struct ProgArgs
{
	// Configurable via command line options
	bool continuous_print_flag;
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
		continuous_print_flag(false),
		energy_delayed_product(false),
		devices{"MCP1"},
		runs(1),
		delay(0),
		interval(500),
		before(0),
		after(0),
		workload_and_args(nullptr)
	{
		int c;
		while ((c = getopt (argc, argv, "hcpe:r:d:i:b:a:")) != -1) {
			switch (c) {
				case 'h':
				case '?':
					printHelpAndExit(argv[0]);
					break;
				case 'c':
					continuous_print_flag = true;
					break;
				case 'p':
					energy_delayed_product = true;
					break;
				case 'e':
					devices = str_split<','>(optarg);
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
		std::cout << "Usage: " << progname << " -h|[-c|-p] [-e dev1,dev2,...] ([-r|-d|-i|-b|-a] N)* [--] workload [args]" << std::endl;
		std::cout << "\t-h Print this help and exit" << std::endl;
		std::cout << "\t-c Continuously print power levels (mW) to stdout (skip energy stats)" << std::endl;
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
	using result_t = std::vector<PowerDataSource::accumulate_t>;
	std::vector<PowerDataSourcePtr> devices;

	Sampler(std::chrono::milliseconds interval, const std::vector<std::string> & devNames, bool continuous_print_flag = false) :
		m_interval(interval),
		m_startable(false),
		m_done(false),
		m_ticks(0)
	{
		devices.reserve(devNames.size());

		for (const auto & name: devNames) {
			if (!name.compare("MCP1"))
				devices.emplace_back(new MCP_EasyPower("/dev/ttyACM0"));
			else if (!name.compare("CPU"))
				devices.emplace_back(new JetsonCounter(TEGRA_CPU_DEV, name));
			else if (!name.compare("GPU"))
				devices.emplace_back(new JetsonCounter(TEGRA_GPU_DEV, name));
			else if (!name.compare("SOC"))
				devices.emplace_back(new JetsonCounter(TEGRA_SOC_DEV, name));
			else if (!name.compare("DDR"))
				devices.emplace_back(new JetsonCounter(TEGRA_DDR_DEV, name));
			else if (!name.compare("IN"))
				devices.emplace_back(new JetsonCounter(TEGRA_IN_DEV, name));
			else
				std::runtime_error("Unknown device \"" + name + "\"");
		}

		std::function<void()> atick  = [this]{accumulate_tick();};
		std::function<void()> cptick = [this]{continuous_print_tick();};

		m_worker = std::thread([this, continuous_print_flag, atick, cptick]{ run(
			continuous_print_flag ? cptick : atick
		); });
	}

	void start(std::chrono::milliseconds delay = std::chrono::milliseconds(0))
	{
		std::this_thread::sleep_for(delay);
		m_startable = true;
		m_start_signal.notify_one();
	}

	result_t stop(std::chrono::milliseconds delay = std::chrono::milliseconds(0))
	{
		std::this_thread::sleep_for(delay);
		m_done = true;

		// Finish loop in case we didn't start
		start();
		m_worker.join();

		result_t result;
		std::transform(devices.cbegin(), devices.cend(),
			std::back_inserter(result), [](const PowerDataSourcePtr & tdi) { return tdi->accumulator(); });
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

	std::atomic<bool> m_startable;
	std::atomic<bool> m_done;
	long m_ticks;

	void run(std::function<void()> tick)
	{
		std::unique_lock<std::mutex> lk(m_start_mutex);
		m_start_signal.wait(lk, [this]{ return m_startable.load(); });

		while (!m_done.load()) {
			// FIXME: tiny skid by scheduling + now(). Global start instead?
			auto entry = std::chrono::high_resolution_clock::now();
			tick();
			m_ticks++;
			std::this_thread::sleep_until(entry + m_interval);
		}
	}

	void accumulate_tick()
	{
		for (auto & dev: devices) {
			dev->accumulate();
		}
	}

	void continuous_print_tick()
	{
		static char buf[255];
		size_t avail = sizeof(buf);
		size_t pos = 0;
		size_t nbytes;
		for (auto & dev: devices) {
			nbytes = dev->read_string(buf + pos, avail);
			pos += nbytes;
			avail -= nbytes;
			buf[pos - 1] = ',';
		}
		buf[pos - 1] = '\0';
		puts(buf);
	}
};

/**********************************************************/

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

	void run()
	{
		for (unsigned int i = 0; i < m_args.runs; i++) {
			if (m_args.continuous_print_flag && m_args.runs > 1)
				std::cout << "### Run " << i << std::endl;
			m_results.push_back(run_single());
			std::this_thread::sleep_for(m_args.delay);
		}
	}

	void printResult()
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

		std::vector<double> means(m_args.devices.size(), 0);
		double mean_time = 0;
		for (const auto & res: m_results) {
			for (size_t i = 0; i < means.size(); i++) {
				means[i] += calcResult(res, i) / m_args.runs;
			}
			mean_time += res.workload_wall_time.count() / m_args.runs;
		}

		std::vector<double> variances(m_args.devices.size(), 0);
		double variance_time = 0;
		for (const auto & res: m_results) {
			for (size_t i = 0; i < variances.size(); i++) {
				double vi = calcResult(res, i) - means[i];
				variances[i] += vi * vi / m_args.runs;
			}

			double vti = res.workload_wall_time.count() - mean_time;
			variance_time += vti * vti / m_args.runs;
		}

		std::vector<double> stddev_percents(m_args.devices.size(), 0);
		for (size_t i = 0; i < stddev_percents.size(); i++) {
			stddev_percents[i] = (std::sqrt(variances[i]) / means[i]) * 100;
		}
		double stddev_percent_time = (std::sqrt(variance_time) / mean_time) * 100;

		for (size_t i = 0; i < means.size(); i++) {
			std::cerr << "\t"
				<< std::fixed << std::setprecision(2)
				<< means[i] << " " << m_args.unit << "\t"
				<< m_args.devices[i];
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


private:
	const ProgArgs & m_args;
	std::vector<Result> m_results;

	Result run_single()
	{
		Sampler sampler(m_args.interval, m_args.devices, m_args.continuous_print_flag);

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

	double calcResult(const Result & res, const int sample_index)
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
