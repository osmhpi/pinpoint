#include "RAPL.h"

#include "Registry.h"

#include <cstring>
#include <fstream>
#include <map>

struct RAPLEventInfo
{
	double joules_per_tick;
	uint32_t type;
	uint64_t config;
};

static std::map<std::string,RAPLEventInfo> s_validEvents;

#if defined(__linux__)

#include <dirent.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
				int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

static const std::string deviceInfoPath = "/sys/bus/event_source/devices/power/";
static const std::string searchPath = deviceInfoPath + "events/";
static const std::string eventFilePrefix = "energy-";

struct RAPLDetail
{
	struct perf_event_attr attr;
	RAPLEventInfo *event_info;
	int fd;
};

static std::vector<std::string> detect_event_files()
{
	std::vector<std::string> eventFiles;

	struct dirent *entry = nullptr;
	std::shared_ptr<DIR> dir(opendir(searchPath.c_str()), [](DIR *dir) {if (dir) closedir(dir);});
	if (!dir)
		return eventFiles;

	while ((entry = readdir(dir.get()))) {
		const std::string name(entry->d_name);
		if (name.substr(0, eventFilePrefix.size()) == eventFilePrefix
				&& name.find('.') == std::string::npos)
			eventFiles.push_back(name.substr(eventFilePrefix.size()));
	}
	return eventFiles;
}

template<typename T>
static T read_event_file(const std::string & prefix, const std::string & name, const std::string & suffix)
{
	std::ifstream eventFile("/sys/bus/event_source/devices/power/" + prefix + name + suffix);

	if (!eventFile)
		throw std::runtime_error("failed to read RAPL info (" + suffix + ") for " + name);
	T res;
	eventFile >> res;
	return res;
}

#include <iostream>

static bool test_event_file(const std::string & name)
{
	RAPLEventInfo info;

	info.type = read_event_file<uint32_t>("", "type", "");
	auto config = read_event_file<std::string>("events/energy-", name, "");
	const std::string configPrefix = "event=0x";
	if (config.find(configPrefix) != 0)
		throw std::runtime_error("unexpected RAPL config string '" + config + "' for " + name);

	info.config = std::stoi(config.substr(configPrefix.size(), std::string::npos), nullptr, 16);
	info.joules_per_tick = read_event_file<double>("events/energy-", name, ".scale");

	auto unit = read_event_file<std::string>("events/energy-", name, ".unit");
	if (unit.compare("Joules") != 0)
		throw std::runtime_error("unexpected RAPL unit '" +std::string(unit) + "' for " + name);

	s_validEvents[name] = info;
	return true;
}

std::vector<std::string> RAPL::detectAvailableCounters()
{
	std::vector<std::string> counters;
	for (const auto & name: detect_event_files())
		if (test_event_file(name))
			counters.emplace_back(name);
	return counters;
}

RAPL::RAPL(const std::string & name) :
	EnergyDataSource(),
	m_detail(new RAPLDetail)
{
	memset(m_detail, 0, sizeof(*m_detail));
	m_detail->event_info = &s_validEvents.at(name);

	m_detail->attr.type = m_detail->event_info->type;
	m_detail->attr.size = sizeof(m_detail->attr);
	m_detail->attr.config = m_detail->event_info->config;

	m_detail->fd = perf_event_open(&m_detail->attr, -1, 0, -1, 0);
	if (m_detail->fd < 0) {
		throw std::runtime_error("Cannot open RAPL event " + name + " (" + strerror(errno) + ")");
	}

	initial_read();
}

EnergySample RAPL::read_energy()
{
	uint64_t current_ticks;
	if (::read(m_detail->fd, &current_ticks, sizeof(uint64_t)) != sizeof(uint64_t)) {
		throw std::runtime_error("Cannot read RAPL event " + name() + " (" + strerror(errno) + ")");
	}

	units::energy::joule_t joules(current_ticks * m_detail->event_info->joules_per_tick);
	return EnergySample(joules);
}

#else

struct RAPLDetail
{
	;;
};

std::vector<std::string> RAPL::detectAvailableCounters()
{
	return std::vector<std::string>();
}

RAPL::RAPL(const std::string & name) :
	m_detail(new RAPLDetail)
{

}

EnergySample RAPL::read_energy()
{
	return EnergySample();
}

#endif

/***********************************************************************/

PowerDataSourcePtr RAPL::openCounter(const std::string &counterName)
{
	if (s_validEvents.find(counterName) == s_validEvents.end())
		return nullptr;

	return PowerDataSourcePtr(new RAPL(counterName));
}

void RAPL::registerPossibleAliases()
{
	Registry::registerAlias<RAPL>("CPU", "cores");
	Registry::registerAlias<RAPL>("GPU", "gpu");
	Registry::registerAlias<RAPL>("RAM", "ram");
}

RAPL::~RAPL()
{
	delete m_detail;
}
