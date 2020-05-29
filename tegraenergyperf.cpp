#include "TegraDeviceInfo.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
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
	unsigned int runs;
	std::chrono::milliseconds interval;
	std::chrono::milliseconds before;
	std::chrono::milliseconds after;
	char **workload_and_args;

	// Inferred value
	std::string unit;

	ProgArgs(int argc, char *argv[]):
		energy_delayed_product(false),
		interval(500),
		runs(1),
		before(0),
		after(0),
		workload_and_args(nullptr)
	{
		int c;
		while ((c = getopt (argc, argv, "hpr:i:b:a:")) != -1) {
			switch (c) {
				case 'h':
				case '?':
					printHelpAndExit(argv[0]);
					break;
				case 'p':
					energy_delayed_product = true;
					break;
				case 'r':
					runs = atoi(optarg);
					if (runs < 1) {
						std::cerr << "Invalid number of runs" << std::endl;
						exit(1);
					}
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
		std::cout << "Usage: " << progname << "[-h|[-s] [-i INTERVAL] [-b N] [-a N] -- workload [args]]" << std::endl;
		std::cout << "\t-h Print this help and exit" << std::endl;
		std::cout << "\t-p Measure energy-delayed product (measure joules if not provided)" << std::endl;
		std::cout << "\t-r Number of runs (default: " << runs << ")" << std::endl;
		std::cout << "\t-i Sampling interval in ms (default: " << interval.count() << ")" << std::endl;
		std::cout << "\t-b Start measurement N ms before worker creation (negative values will delay)" << std::endl;
		std::cout << "\t-a Continue measurement N ms after worker exited" << std::endl;
		exit(exitcode);
	}

};

/**********************************************************/

struct Sampler
{
	std::vector<TegraDeviceInfo> devices;

	Sampler(std::chrono::milliseconds interval) :
		m_interval(interval),
		m_done(false),
		m_ticks(0)
	{
		devices.reserve(5);

		devices.emplace_back(TEGRA_CPU_DEV, "CPU");
		devices.emplace_back(TEGRA_GPU_DEV, "GPU");
		devices.emplace_back(TEGRA_SOC_DEV, "SOC");
		devices.emplace_back(TEGRA_DDR_DEV, "DDR");
		devices.emplace_back(TEGRA_IN_DEV , "IN");

		m_worker = std::thread([this]{ tick(); });
	}

	~Sampler()
	{
		stop();
		// Finish loop in case we didn't start
		m_start_signal.notify_one();
		m_worker.join();
	}

	void start(std::chrono::milliseconds delay = std::chrono::milliseconds(0))
	{
		std::this_thread::sleep_for(delay);
		m_start_signal.notify_one();
	}

	void stop(std::chrono::milliseconds delay = std::chrono::milliseconds(0))
	{
		std::this_thread::sleep_for(delay);
		m_done.store(true);
	}

	long ticks() const
	{
		return m_ticks;
	}

private:

	void tick()
	{
		// FIXME: tiny skid by scheduling + now(). Global start instead?

		std::unique_lock<std::mutex> lk(m_start_mutex);
		m_start_signal.wait(lk);

		while (!m_done.load()) {
			auto entry = std::chrono::high_resolution_clock::now();
			for (auto & dev: devices) {
				dev.accumulate();
			}
			m_ticks++;
			std::this_thread::sleep_until(entry + m_interval);
		}
	}

	std::chrono::milliseconds m_interval;
	std::thread m_worker;

	std::condition_variable m_start_signal;
	std::mutex m_start_mutex;

	std::atomic<bool> m_done;
	long m_ticks;
};

/**********************************************************/

int calcResult(TegraDeviceInfo::accumulate_t value, const ProgArgs & args)
{
	// Assuming:
	//   * constant interval
	//   * accumulate_t contains sum of milli-Watt measurements
	//   * we take lower Darboux integral ... :( [since we measure at start of interval]

	// TODO: implement EDP
	auto interval_in_s = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(args.interval);
	return (double) value * interval_in_s.count();
}

void printResult(const Sampler & sampler, const ProgArgs & args)
{
	std::cerr << "Tegra energy counter stats for '";
	for (char **a = args.workload_and_args; a && *a; ++a) {
		std::cerr << *a << " ";
	}

	std::cerr << "\b':" << std::endl;
	std::cerr << "[interval: " << args.interval.count() << "ms, before: " 
	                           << args.before.count() << " ms, after: " 
	                           << args.after.count() << " ms, runs: "
	                           << args.runs << "]" << std::endl;
	std::cerr << std::endl;

	for (const TegraDeviceInfo & tdi: sampler.devices) {
		std::cerr << "\t" << calcResult(tdi.accumulator(), args) <<  "\t" << tdi.name() << " (" << args.unit  << ")" << std::endl;
	}

	std::cerr << std::endl;
}

int main(int argc, char *argv[])
{
	ProgArgs args(argc, argv);
	Sampler sampler(args.interval);

	if (args.before.count() > 0) {
		sampler.start();
		std::this_thread::sleep_for(args.before);
	}

	pid_t workload;
	if ((workload = fork())) {
		sampler.start(std::max(-args.before, std::chrono::milliseconds(0)));
		waitpid(workload, NULL, 0);
	} else {
		execvp(args.workload_and_args[0], args.workload_and_args);
	}

	sampler.stop(std::chrono::milliseconds(args.after));
	printResult(sampler, args);

	return 0;
}
