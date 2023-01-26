#include "Sampler.h"
#include "Settings.h"
#include "Registry.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <iomanip>
#include <thread>


struct SamplerDetail
{
	std::chrono::milliseconds interval;
	std::thread worker;

	std::condition_variable start_signal;
	std::mutex start_mutex;

	std::atomic<bool> startable;
	std::atomic<bool> done;

	std::string csv_header = "";

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

Sampler::Sampler(std::chrono::milliseconds interval, const std::vector<std::string> & counterOrAliasNames, bool continuous_print_flag, bool continuous_header_flag) :
	m_detail(new SamplerDetail(interval))
{
	counters.reserve(counterOrAliasNames.size());

	for (const auto & name: counterOrAliasNames) {
		PowerDataSourcePtr counter = Registry::openCounter(name);
		if (!counter) {
			throw std::runtime_error("Unknown counter \"" + name + "\"");
		}
		counters.push_back(counter);
	}

	std::function<void()> atick  = [this]{accumulate_tick();};
	std::function<void()> cptick = [this]{continuous_print_tick();};

	if (continuous_print_flag && continuous_header_flag) {
		for (auto & s : counterOrAliasNames) m_detail->csv_header += s + ",";
		m_detail->csv_header.back() = '\n';
	}

	if (continuous_print_flag && settings::countinous_timestamp_flag) {
		std::cout << std::fixed << std::setprecision(4);
	}

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
	std::transform(counters.cbegin(), counters.cend(),
		std::back_inserter(result), [](const PowerDataSourcePtr & tdi) { return tdi->accumulator(); });
	return result;
}

void Sampler::run(std::function<void()> tick)
{
	std::unique_lock<std::mutex> lk(m_detail->start_mutex);
	m_detail->start_signal.wait(lk, [this]{ return m_detail->startable.load(); });

	fputs(m_detail->csv_header.c_str(), stdout);

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
	for (auto & dev: counters) {
		dev->accumulate();
	}
}

void Sampler::continuous_print_tick()
{
	static char buf[255];
	size_t avail = sizeof(buf);
	size_t pos = 0;
	size_t nbytes;
	PowerSample::timestamp_t timestamp = PowerSample::now();

	for (auto & dev: counters) {
		const PowerDataSource::time_and_strlen ts = dev->read_mW_string(buf + pos, avail);
		timestamp = std::max(timestamp, ts.first);
		nbytes = ts.second;
		pos += nbytes;
		avail -= nbytes;
		buf[pos - 1] = ',';
	}
	buf[pos - 1] = '\0';
	if (settings::countinous_timestamp_flag) {
		std::chrono::duration<float, std::ratio<1>> s_since_epoch = timestamp.time_since_epoch();
		std::cout << s_since_epoch.count() << ",";
	}
	std::cout << buf << std::endl;
}
