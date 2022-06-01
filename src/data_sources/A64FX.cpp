#include "A64FX.h"
#include "Registry.h"

#include <cstring>
#include <fstream>
#include <map>
#include <algorithm>

#if defined(__aarch64__) && defined(__linux__)

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
				int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

static inline std::string &trim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

static std::map<std::string, std::string> getCpuAttributesMap() 
{
        std::ifstream cpuinfoFile("/proc/cpuinfo");
        if (!cpuinfoFile)
                throw std::runtime_error("failed to read /proc/cpuinfo");

        std::map<std::string, std::string> cpuAttributesMap;
        std::string line;

        while (std::getline(cpuinfoFile, line)) {
                std::string::size_type keyPos = 0;
                std::string::size_type keyEnd;
                std::string::size_type valPos;
                std::string::size_type valEnd;

                if((keyEnd = line.find(":", keyPos)) != std::string::npos) {
                        if((valPos = line.find_first_not_of(": ", keyEnd)) == std::string::npos)
                                continue;
                        valEnd = line.length();
                        std::string keyString = line.substr(keyPos, keyEnd-keyPos);
                        cpuAttributesMap.emplace(trim(keyString), line.substr(valPos, valEnd - valPos));
                }
        }
        cpuinfoFile.close();
        return cpuAttributesMap;
}

static bool detectA64FXCPU()
{
	auto cpuAttributesMap = getCpuAttributesMap();

	unsigned int cpuImplementer = 0;
	unsigned int cpuPart = 0;

	auto cpuImplementerIterator = cpuAttributesMap.find("CPU implementer");
	if(cpuImplementerIterator != cpuAttributesMap.end())
		cpuImplementer = std::stoul(cpuImplementerIterator->second, 0, 16);
	auto cpuPartIterator = cpuAttributesMap.find("CPU part");
	if(cpuPartIterator != cpuAttributesMap.end())
		cpuPart = std::stoul(cpuPartIterator->second, 0, 16);

	if(cpuImplementer == 0x46 && cpuPart == 0x001)
		return true;
	return false;
}



struct A64FXEventInfo
{
	double joules_per_tick;
	uint64_t config;
};

struct A64FXDetail
{
	static std::map<std::string,A64FXEventInfo> validEvents;

	struct perf_event_attr attr;
	double joules_per_tick;
	int fd;
};

std::vector<std::string> A64FX::detectAvailableCounters()
{
	std::vector<std::string> counters;
	if(detectA64FXCPU()) {
		A64FXEventInfo info_core;
		info_core.joules_per_tick = 8.04e-9;
		info_core.config = 0x01e0;
		A64FXDetail::validEvents["EA_CORE"] = info_core;
		counters.emplace_back("EA_CORE");

		A64FXEventInfo info_l2;
		info_l2.joules_per_tick = 32.8e-9;
		info_l2.config = 0x03e0;
		A64FXDetail::validEvents["EA_L2"] = info_l2;
		counters.emplace_back("EA_L2");

		A64FXEventInfo info_memory;
		info_memory.joules_per_tick = 271.0e-9;
		info_memory.config = 0x03e8;
		A64FXDetail::validEvents["EA_MEMORY"] = info_memory;
		counters.emplace_back("EA_MEMORY");
	}
	return counters;
}

A64FX::A64FX(const std::string & name) :
	EnergyDataSource(),
	m_detail(new A64FXDetail)
{
	const auto & event_info = A64FXDetail::validEvents[name];
	m_detail->joules_per_tick = event_info.joules_per_tick;

	memset(&m_detail->attr, 0, sizeof(m_detail->attr));
	m_detail->attr.type = PERF_TYPE_RAW;
	m_detail->attr.size = sizeof(m_detail->attr);
	m_detail->attr.config = event_info.config;

	m_detail->fd = perf_event_open(&m_detail->attr, -1, 0, -1, 0);
	if (m_detail->fd < 0) {
		throw std::runtime_error("Cannot open A64FX event " + name + " (" + strerror(errno) + ")");
	}

	initial_read();
}

EnergySample A64FX::read_energy()
{
	uint64_t current_ticks;
	if (::read(m_detail->fd, &current_ticks, sizeof(uint64_t)) != sizeof(uint64_t)) {
		throw std::runtime_error("Cannot read A64FX event " + name() + " (" + strerror(errno) + ")");
	}

	units::energy::joule_t joules(current_ticks * m_detail->joules_per_tick);
	return EnergySample(joules);
}

#else

using A64FXEventInfo = int;

struct A64FXDetail
{
	static std::map<std::string,A64FXEventInfo> validEvents;
};

std::vector<std::string> A64FX::detectAvailableCounters()
{
	return std::vector<std::string>();
}

A64FX::A64FX(const std::string & name) :
	EnergyDataSource(),
	m_detail(new A64FXDetail)
{
	(void)name;
}

EnergySample A64FX::read_energy()
{
	return EnergySample();
}

#endif

/***********************************************************************/

std::map<std::string,A64FXEventInfo> A64FXDetail::validEvents;

PowerDataSourcePtr A64FX::openCounter(const std::string &counterName)
{
	if (A64FXDetail::validEvents.find(counterName) == A64FXDetail::validEvents.end())
		return nullptr;

	return PowerDataSourcePtr(new A64FX(counterName));
}

Aliases A64FX::possibleAliases()
{
	return {
		{"CPU", "EA_CORE"},
		{"L2", "EA_L2"},
		{"RAM", "EA_MEMORY"},
	};
}

A64FX::~A64FX()
{
	delete m_detail;
}

PINPOINT_REGISTER_DATA_SOURCE(A64FX)
