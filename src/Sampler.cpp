#include "Sampler.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <thread>


struct SamplerDetail
{
	std::chrono::milliseconds interval;
	std::thread worker;

	std::condition_variable start_signal;
	std::mutex start_mutex;

	std::atomic<bool> startable;
	std::atomic<bool> done;

	long ticks;

	SamplerDetail(std::chrono::milliseconds sampling_interval):
		interval(sampling_interval),
		startable(false),
		done(false),
		ticks(0)
	{
		;;
	}
};


Sampler::Sampler(std::chrono::milliseconds interval, const std::vector<std::string> & devNames, bool continuous_print_flag) :
	m_detail(new SamplerDetail(interval))
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

	m_detail->worker = std::thread([=]{ run(
		continuous_print_flag ? cptick : atick
	); });
}

Sampler::~Sampler()
{
	delete m_detail;
}

long Sampler::ticks() const
{
	return m_detail->ticks;
}

void Sampler::start(std::chrono::milliseconds delay)
{
	std::this_thread::sleep_for(delay);
	m_detail->startable = true;
	m_detail->start_signal.notify_one();
}

Sampler::result_t Sampler::stop(std::chrono::milliseconds delay)
{
	std::this_thread::sleep_for(delay);
	m_detail->done = true;

	// Finish loop in case we didn't start
	start();
	m_detail->worker.join();

	result_t result;
	std::transform(devices.cbegin(), devices.cend(),
		std::back_inserter(result), [](const PowerDataSourcePtr & tdi) { return tdi->accumulator(); });
	return result;
}

void Sampler::run(std::function<void()> tick)
{
	std::unique_lock<std::mutex> lk(m_detail->start_mutex);
	m_detail->start_signal.wait(lk, [this]{ return m_detail->startable.load(); });

	while (!m_detail->done.load()) {
		// FIXME: tiny skid by scheduling + now(). Global start instead?
		auto entry = std::chrono::high_resolution_clock::now();
		tick();
		m_detail->ticks++;
		std::this_thread::sleep_until(entry + m_detail->interval);
	}
}

void Sampler::accumulate_tick()
{
	for (auto & dev: devices) {
		dev->accumulate();
	}
}

void Sampler::continuous_print_tick()
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
