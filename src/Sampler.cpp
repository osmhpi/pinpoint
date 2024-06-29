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

Sampler::Sampler(std::chrono::milliseconds interval, const std::vector<std::string> & counterOrAliasNames) :
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

	if (settings::continuous_print_flag && settings::continuous_header_flag) {
		for (auto & s : counterOrAliasNames) m_detail->csv_header += s + ",";
		m_detail->csv_header.back() = '\n';
	}

	if (settings::continuous_print_flag && settings::countinous_timestamp_flag) {
		settings::output_stream << std::fixed << std::setprecision(4);
	}

	m_detail->worker = std::thread([=]{ run(
		settings::continuous_print_flag ? cptick : atick
	); });

	Registry::callInitializeExperimentsOnOpenSources();
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
	PowerSample::timestamp_t timestamp;

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
		// I wanted to use duration<float, ratio<1>>, but this resulted in weird constant epoch
		auto ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
		settings::output_stream << ms_since_epoch * 0.001 << ",";
	}
	settings::output_stream << buf << std::endl;
}
