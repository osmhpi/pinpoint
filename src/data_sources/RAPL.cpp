#include "RAPL.h"
#include "Registry.h"

#include <cstring>
#include <fstream>
#include <map>

#if defined(__x86_64__) && defined(__linux__)

#include <dirent.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
				int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

struct RAPLEventInfo
{
	double joules_per_tick;
	uint32_t type;
	uint64_t config;
};

static const std::string deviceInfoPath = "/sys/bus/event_source/devices/power/";
static const std::string searchPath = deviceInfoPath + "events/";
static const std::string eventFilePrefix = "energy-";

struct RAPLDetail
{
	static std::map<std::string,RAPLEventInfo> validEvents;

	struct perf_event_attr attr;
	double joules_per_tick;
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

	RAPLDetail::validEvents[name] = info;
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
	const auto & event_info = RAPLDetail::validEvents[name];
	m_detail->joules_per_tick = event_info.joules_per_tick;

	memset(&m_detail->attr, 0, sizeof(m_detail->attr));
	m_detail->attr.type = event_info.type;
	m_detail->attr.size = sizeof(m_detail->attr);
	m_detail->attr.config = event_info.config;

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

	units::energy::joule_t joules(current_ticks * m_detail->joules_per_tick);
	return EnergySample(joules);
}

#elif defined(__x86_64__) && defined(__APPLE__) && defined(__MACH__)

/* To understand further details on how to access RAPL counters on macOS see
 * xnu/xnu-<version>/osfmk/i386/Diagnostics.c at https://opensource.apple.com/
 *
 * Initial architecure and parts of the code in this section are based on
 * Mozilla's tools/power/rapl.c by Nicholas Nethercote and other authors
 * (subject to the terms of the Mozilla Public License, v. 2.0).
 *
 */


#include <array>
#include <limits>
#include <mutex>

#include <sys/types.h>
#include <sys/sysctl.h>

#define dgPowerStat 17

#define CPU_RTIME_BINS (12)
#define CPU_ITIME_BINS (CPU_RTIME_BINS)

#define PKG_ENERGY_IDX   0
#define CORES_ENERGY_IDX 1
#define GPU_ENERGY_IDX   2
#define RAM_ENERGY_IDX   3

typedef struct {
	uint64_t caperf;
	uint64_t cmperf;
	uint64_t ccres[6];
	uint64_t crtimes[CPU_RTIME_BINS];
	uint64_t citimes[CPU_ITIME_BINS];
	uint64_t crtime_total;
	uint64_t citime_total;
	uint64_t cpu_idle_exits;
	uint64_t cpu_insns;
	uint64_t cpu_ucc;
	uint64_t cpu_urc;
#   if DIAG_ALL_PMCS           // Added in 10.10.2 (xnu-2782.10.72).
	uint64_t gpmcs[4];
#   endif /* DIAG_ALL_PMCS */
} core_energy_stat_t;

typedef struct {
	uint64_t pkes_version;
	uint64_t pkg_cres[2][7];
	uint64_t pkg_power_unit;
	uint64_t pkg_energy;
	uint64_t pp0_energy;
	uint64_t pp1_energy;
	uint64_t ddr_energy;
	uint64_t llc_flushed_cycles;
	uint64_t ring_ratio_instantaneous;
	uint64_t IA_frequency_clipping_cause;
	uint64_t GT_frequency_clipping_cause;
	uint64_t pkg_idle_exits;
	uint64_t pkg_rtimes[CPU_RTIME_BINS];
	uint64_t pkg_itimes[CPU_ITIME_BINS];
	uint64_t mbus_delay_time;
	uint64_t mint_delay_time;
	uint32_t ncpus;
	core_energy_stat_t cest[];
} pkg_energy_statistics_t;

static int diagCall64(uint64_t aMode, void* aBuf) {
	// We cannot use syscall() here because it doesn't work with diagnostic
	// system calls -- it raises SIGSYS if you try. So we have to use asm.

	// The 0x40000 prefix indicates it's a diagnostic system call. The 0x01
	// suffix indicates the syscall number is 1, which also happens to be the
	// only diagnostic system call. See osfmk/mach/i386/syscall_sw.h for more
	// details.
	static const uint64_t diagCallNum = 0x4000001;
	uint64_t rv;

	__asm__ __volatile__(
		"syscall"

		// Return value goes in "a" (%rax).
		: /* outputs */ "=a"(rv)

		// The syscall number goes in "0", a synonym (from outputs) for "a"
		// (%rax). The syscall arguments go in "D" (%rdi) and "S" (%rsi).
		: /* inputs */ "0"(diagCallNum), "D"(aMode), "S"(aBuf)

		// The |syscall| instruction clobbers %rcx, %r11, and %rflags ("cc"). And
		// this particular syscall also writes memory (aBuf).
		: /* clobbers */ "rcx", "r11", "cc", "memory");
	return rv;
}

using RAPLEventInfo = int;

struct RAPLDetail
{
	// shared info
	static std::map<std::string,RAPLEventInfo> validEvents;

	static bool has_ramQuirk;
	static const double kQuirkyRamJoulesPerTick;
	static pkg_energy_statistics_t *pkes;

	static const uint64_t kOutdated;
	static EnergySample::timestamp_t measurement_timepoint;
	static std::array<uint64_t, 4> startTicks;
	static std::array<uint64_t, 4> currentTicks;

	static void read_ticks()
	{
		static const uint64_t supported_version = 1;
		// Write an unsupported version number into pkes_version so that the check
		// below cannot succeed by dumb luck.
		pkes->pkes_version = supported_version - 1;

		// diagCall64() returns 1 on success, and 0 on failure (which can only happen
		// if the mode is unrecognized, e.g. in 10.7.x or earlier versions).
		if (diagCall64(dgPowerStat, pkes) != 1) {
			throw std::runtime_error("diagCall64() failed");
		}
		measurement_timepoint = EnergySample::clock_t::now();

		if (pkes->pkes_version != 1) {
			throw std::runtime_error("unexpected pkes_version: " + std::to_string(pkes->pkes_version));
		}

		currentTicks[PKG_ENERGY_IDX]   = pkes->pkg_energy;
		currentTicks[CORES_ENERGY_IDX] = pkes->pp0_energy;
		currentTicks[GPU_ENERGY_IDX]   = pkes->pp1_energy;
		currentTicks[RAM_ENERGY_IDX]   = pkes->ddr_energy;
	}

	// per counter-info
	int index;
	double joulesPerTick;
};

bool RAPLDetail::has_ramQuirk = false;
const double RAPLDetail::kQuirkyRamJoulesPerTick = 1.0 / 65536;
pkg_energy_statistics_t *RAPLDetail::pkes = nullptr;

const uint64_t RAPLDetail::kOutdated = std::numeric_limits<uint64_t>::max();
EnergySample::timestamp_t RAPLDetail::measurement_timepoint;
std::array<uint64_t, 4> RAPLDetail::startTicks;
std::array<uint64_t, 4> RAPLDetail::currentTicks;

std::vector<std::string> RAPL::detectAvailableCounters()
{
	bool has_gpu;
	bool has_ram;

	int cpuModel;
	size_t size = sizeof(cpuModel);
	if (sysctlbyname("machdep.cpu.model", &cpuModel, &size, NULL, 0) != 0) {
		throw std::runtime_error("sysctlbyname(\"machdep.cpu.model\") failed");
	}

	switch (cpuModel) {
		case 42:  // Sandy Bridge
		case 58:  // Ivy Bridge
			// Supports package, cores, GPU.
			has_gpu = true;
			has_ram = false;
			break;

		case 63:  // Haswell X
		case 79:  // Broadwell X
		case 85:  // Skylake X
		case 86:  // Broadwell D
			// Supports package, cores, RAM. Has the units quirk.
			has_gpu = false;
			has_ram = true;
			RAPLDetail::has_ramQuirk = true;
			break;

		case 45:  // Sandy Bridge X
		case 62:  // Ivy Bridge X
			// Supports package, cores, RAM.
			has_gpu = false;
			has_ram = true;
			break;

		case 60:  // Haswell
		case 61:  // Broadwell
		case 69:  // Haswell L
		case 70:  // Haswell G
		case 71:  // Broadwell G
			// Supports package, cores, GPU, RAM.
			has_gpu = true;
			has_ram = true;
			break;

		case 78:  // Skylake L
		case 94:  // Skylake
		case 142:  // Kaby Lake L
		case 158:  // Kaby Lake
		case 102:  // Cannon Lake L
		case 125:  // Ice Lake
		case 126:  // Ice Lake L
		case 165:  // Comet Lake
		case 166:  // Comet Lake L
			// Supports package, cores, GPU, RAM, PSYS.
			// XXX: this tool currently doesn't measure PSYS.
			has_gpu = true;
			has_ram = true;
			break;

		default:
			throw std::runtime_error("unknown CPU model: " + std::to_string(cpuModel));
			break;
	}

	int logicalcpu_max;
	size = sizeof(logicalcpu_max);
	if (sysctlbyname("hw.logicalcpu_max", &logicalcpu_max, &size, NULL, 0) != 0) {
		throw std::runtime_error("sysctlbyname(\"hw.logicalcpu_max\") failed");
	}

	// Over-allocate by 1024 bytes per CPU to allow for the uncertainty around
	// core_energy_stat_t::gpmcs and for any other future extensions to that
	// struct. (The fields we read all come before the core_energy_stat_t
	// array, so it won't matter to us whether gpmcs is present or not.)
	size_t pkesSize = sizeof(pkg_energy_statistics_t) +
					  logicalcpu_max * sizeof(core_energy_stat_t) +
					  logicalcpu_max * 1024;
	RAPLDetail::pkes = (pkg_energy_statistics_t *)calloc(1, pkesSize);

	RAPLDetail::validEvents["pkg"]   = PKG_ENERGY_IDX;
	RAPLDetail::validEvents["cores"] = CORES_ENERGY_IDX;

	if (has_gpu) {
		RAPLDetail::validEvents["gpu"] = GPU_ENERGY_IDX;
	}

	if (has_ram) {
		RAPLDetail::validEvents["ram"] = RAM_ENERGY_IDX;
	}

	std::vector<std::string> counters;
	for (const auto & validEvent : RAPLDetail::validEvents)
		counters.push_back(validEvent.first);

	return counters;
}

RAPL::RAPL(const std::string & name) :
	EnergyDataSource(),
	m_detail(new RAPLDetail)
{
	m_detail->index = RAPLDetail::validEvents[name];

	m_detail->read_ticks();
	m_detail->startTicks[m_detail->index] = m_detail->currentTicks[m_detail->index];

	uint32_t energyStatusUnits = (RAPLDetail::pkes->pkg_power_unit >> 8) & 0x1f;
	m_detail->joulesPerTick = (m_detail->index == RAM_ENERGY_IDX && RAPLDetail::has_ramQuirk) ?
									  RAPLDetail::kQuirkyRamJoulesPerTick :
									  (1. / (1 << energyStatusUnits));

	// Reuses cached value from above
	initial_read();
}

EnergySample RAPL::read_energy()
{
	if ((m_detail->currentTicks[m_detail->index]) == RAPLDetail::kOutdated) {
		m_detail->read_ticks();
	}

	auto tickdiff = m_detail->currentTicks[m_detail->index] - m_detail->startTicks[m_detail->index];
	m_detail->currentTicks[m_detail->index] = RAPLDetail::kOutdated;

	return EnergySample(m_detail->measurement_timepoint, units::energy::joule_t(tickdiff) * m_detail->joulesPerTick);
}

#elif defined(__x86_64__) && defined(__FreeBSD__)

/* 
	Based on MSR part of rapl-read.c 
		originally by Zhang Rui <rui.zhang@intel.com> 2011
		and Vince Weaver -- vincent.weaver @ maine.edu 2015

		https://web.eece.maine.edu/~vweaver/projects/rapl/index.html

*/

using RAPLEventInfo = int;

#include <fcntl.h>
#include <unistd.h>
#include <utility>

#include <sys/cpuctl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#define MSR_RAPL_POWER_UNIT		0x606

/*
 * Platform specific RAPL Domains.
 * Note that PP1 RAPL Domain is supported on 062A only
 * And DRAM RAPL Domain is supported on 062D only
 */
/* Package RAPL Domain */
#define MSR_PKG_RAPL_POWER_LIMIT	0x610
#define MSR_PKG_ENERGY_STATUS		0x611
#define MSR_PKG_PERF_STATUS		0x613
#define MSR_PKG_POWER_INFO		0x614

/* PP0 RAPL Domain */
#define MSR_PP0_POWER_LIMIT		0x638
#define MSR_PP0_ENERGY_STATUS		0x639
#define MSR_PP0_POLICY			0x63A
#define MSR_PP0_PERF_STATUS		0x63B

/* PP1 RAPL Domain, may reflect to uncore devices */
#define MSR_PP1_POWER_LIMIT		0x640
#define MSR_PP1_ENERGY_STATUS		0x641
#define MSR_PP1_POLICY			0x642

/* DRAM RAPL Domain */
#define MSR_DRAM_POWER_LIMIT		0x618
#define MSR_DRAM_ENERGY_STATUS		0x619
#define MSR_DRAM_PERF_STATUS		0x61B
#define MSR_DRAM_POWER_INFO		0x61C

/* PSYS RAPL Domain */
#define MSR_PLATFORM_ENERGY_STATUS	0x64d

/* RAPL UNIT BITMASK */
#define POWER_UNIT_OFFSET	0
#define POWER_UNIT_MASK		0x0F

#define ENERGY_UNIT_OFFSET	0x08
#define ENERGY_UNIT_MASK	0x1F00

#define TIME_UNIT_OFFSET	0x10
#define TIME_UNIT_MASK		0xF000


enum CPUModel {
	SANDYBRIDGE = 42,
	SANDYBRIDGE_EP = 45,
	IVYBRIDGE = 58,
	IVYBRIDGE_EP = 62,
	HASWELL = 60,
	HASWELL_ULT = 69,
	HASWELL_GT3E = 70,
	HASWELL_EP = 63,
	BROADWELL = 61,
	BROADWELL_GT3E = 71,
	BROADWELL_EP = 79,
	BROADWELL_DE = 86,
	SKYLAKE = 78,
	SKYLAKE_HS = 94,
	SKYLAKE_X = 85,
	KNIGHTS_LANDING = 87,
	KNIGHTS_MILL = 133,
	KABYLAKE_MOBILE = 142,
	KABYLAKE = 158,
	CANNONLAKE_L = 102,
	ICELAKE = 125,
	ICELAKE_L = 126,
	COMETLAKE = 165,
	COMETLAKE_L = 166,
	ATOM_SILVERMONT = 55,
	ATOM_AIRMONT = 76,
	ATOM_MERRIFIELD = 74,
	ATOM_MOOREFIELD = 90,
	ATOM_GOLDMONT = 92,
	ATOM_GEMINI_LAKE = 122,
	ATOM_DENVERTON = 95,

	UNSUPPORTED = -1
};

struct CPUCtlFd
{
	CPUCtlFd() :
		m_fd(-1)
	{
		;;
	}

	explicit CPUCtlFd(const int core)
	{
		m_fd = open(("/dev/cpuctl" + std::to_string(core)).c_str(), O_RDONLY);
		if (m_fd < 0) {
			switch (errno) {
				case ENXIO:
					throw std::runtime_error("No such CPU" + std::to_string(core));
					break;
				case EIO:
					throw std::runtime_error("CPU " + std::to_string(core) + " does not support MSRs");
					break;
				default:
					throw std::runtime_error("Opening /dev/cpuctl" + std::to_string(core) + " failed");
					break;
			}
		}
	}

	virtual ~CPUCtlFd()
	{
		if (!(m_fd < 0))
			close(m_fd);
	}

	uint64_t read_msr(int which)
	{
		cpuctl_msr_args_t data;
		data.msr = which;

		if (ioctl(m_fd, CPUCTL_RDMSR, &data) < 0) {
			throw std::runtime_error(std::string("rapl:ioctl:rdmsr ") + ::strerror(errno));
		}

		return data.data;
	}

	CPUCtlFd & operator=(CPUCtlFd && other) 
	{
		m_fd = other.m_fd;
		other.m_fd = -1;
		return *this;
	}

	operator int() const { return m_fd; }

private:
	int m_fd;
};

struct RAPLDetail
{
	static bool has_ramQuirk;
	static const double kQuirkyRamJoulesPerTick;

	static int ncpu;
	static int npackage;
	static std::vector<int> package_map;

	static bool detect_packages();

	static std::map<std::string,RAPLEventInfo> validEvents;

	int package;
	RAPLEventInfo status_msr;
	uint64_t tickBefore;
	double joulesPerTick;
	CPUCtlFd fd;
	uint64_t startTicks;

	uint64_t read_ticks()
	{
		return fd.read_msr(status_msr);
	}
};

bool RAPLDetail::has_ramQuirk = false;
const double RAPLDetail::kQuirkyRamJoulesPerTick = 1.0 / 65536;


int RAPLDetail::ncpu = 0;
int RAPLDetail::npackage = 0;
std::vector<int> RAPLDetail::package_map;


static CPUModel detect_cpu(void)
{
	CPUCtlFd fd(0);
	cpuctl_cpuid_args_t cpuid_data;

	auto do_cpuid = [&](int level){
		cpuid_data.level = level;
		if (ioctl(fd, CPUCTL_CPUID, &cpuid_data) < 0) {
			throw std::runtime_error(std::string("rapl:ioctl:cpuid ") + ::strerror(errno));
		}
	};

	do_cpuid(0);
	std::swap(cpuid_data.data[2], cpuid_data.data[3]);
	char *vendor = (char *)(&cpuid_data.data[1]);
	if (strncmp(vendor, "GenuineIntel", 12)) {
		return CPUModel::UNSUPPORTED;
	}

	do_cpuid(1);
	uint8_t family = (cpuid_data.data[0] >> 8) & 0xF;
	if (family != 6) {
		return CPUModel::UNSUPPORTED;
	}

	// If familiy is 6 (it is) or 15, we need to combine model id (bit4..7)
	// and extended model (bit16..19)
	uint8_t model = ((cpuid_data.data[0] >> 12) & 0xF0) |
		 ((cpuid_data.data[0] >> 4) & 0x0F);

	return static_cast<CPUModel>(model);
}

bool RAPLDetail::detect_packages()
{
	size_t size = sizeof(RAPLDetail::ncpu);
	if (sysctlbyname("hw.ncpu", &RAPLDetail::ncpu, &size, nullptr, 0) == -1) {
		return false;
	}

	// Right now we support only single socket systems
	RAPLDetail::npackage = 1;
	RAPLDetail::package_map.resize(RAPLDetail::npackage);
	RAPLDetail::package_map[0] = 0;

	return true;
}

std::vector<std::string> RAPL::detectAvailableCounters()
{
	CPUModel cpuModel = detect_cpu();
	if (cpuModel == CPUModel::UNSUPPORTED || !RAPLDetail::detect_packages())
		return std::vector<std::string>();

	// has_pkg always true, as far I've seen
	bool has_pp0 = false; // = usually cores
	bool has_pp1 = false; // = usually gpu
	bool has_psys = false;
	bool has_ram = false;

	switch (cpuModel) {
		case CPUModel::SANDYBRIDGE_EP:
		case CPUModel::IVYBRIDGE_EP:
			has_pp0 = true;
			has_ram = true;
			break;

		case CPUModel::HASWELL_EP:
		case CPUModel::BROADWELL_EP:
		case CPUModel::SKYLAKE_X:
			has_pp0 = true;
			has_ram = true;
			RAPLDetail::has_ramQuirk = true;
			break;

		case CPUModel::KNIGHTS_LANDING:
		case CPUModel::KNIGHTS_MILL:
			has_ram = true;
			RAPLDetail::has_ramQuirk = true;
			break;

		case CPUModel::SANDYBRIDGE:
		case CPUModel::IVYBRIDGE:
			has_pp0 = true;
			has_pp1 = true;
			break;

		case CPUModel::HASWELL:
		case CPUModel::HASWELL_ULT:
		case CPUModel::HASWELL_GT3E:
		case CPUModel::BROADWELL:
		case CPUModel::BROADWELL_GT3E:
		case CPUModel::ATOM_GOLDMONT:
		case CPUModel::ATOM_GEMINI_LAKE:
		case CPUModel::ATOM_DENVERTON:
			has_pp0 = true;
			has_pp1 = true;
			has_ram = true;
			break;

		case CPUModel::SKYLAKE:
		case CPUModel::SKYLAKE_HS:
		case CPUModel::KABYLAKE:
		case CPUModel::KABYLAKE_MOBILE:
		case CPUModel::CANNONLAKE_L:
		case CPUModel::ICELAKE:
		case CPUModel::ICELAKE_L:
		case CPUModel::COMETLAKE:
		case CPUModel::COMETLAKE_L:
			has_pp0 = true;
			has_pp1 = true;
			has_ram = true;
			has_psys = true;
			break;

		default:
			throw std::runtime_error("unknown CPU model: " + std::to_string(cpuModel));
			break;
	}


	RAPLDetail::validEvents["pkg"]   = MSR_PKG_ENERGY_STATUS;

	if (has_pp0) {
		RAPLDetail::validEvents["cores"] = MSR_PP0_ENERGY_STATUS;
	}

	if (has_pp1) {
		RAPLDetail::validEvents["gpu"] = MSR_PP1_ENERGY_STATUS;
	}

	if (has_ram) {
		RAPLDetail::validEvents["ram"] = MSR_DRAM_ENERGY_STATUS;
	}

	if (has_psys) {
		RAPLDetail::validEvents["psys"] = MSR_PLATFORM_ENERGY_STATUS;
	}

	std::vector<std::string> counters;
	for (const auto & validEvent : RAPLDetail::validEvents)
		counters.push_back(validEvent.first);

	return counters;
}

RAPL::RAPL(const std::string & name) :
	EnergyDataSource(),
	m_detail(new RAPLDetail)
{
	m_detail->status_msr = RAPLDetail::validEvents[name];
	// Single socket system only
	m_detail->package = 0;
	m_detail->fd = std::move(CPUCtlFd(RAPLDetail::package_map[0]));

	m_detail->startTicks = m_detail->read_ticks();

	uint32_t energyStatusUnits = (m_detail->fd.read_msr(MSR_RAPL_POWER_UNIT) >> 8) & 0x1f;
	m_detail->joulesPerTick = (m_detail->status_msr == MSR_DRAM_ENERGY_STATUS && RAPLDetail::has_ramQuirk) ?
									  RAPLDetail::kQuirkyRamJoulesPerTick :
									  (1. / (1 << energyStatusUnits));

	initial_read();
}

EnergySample RAPL::read_energy()
{
	auto currentTicks = m_detail->read_ticks();
	auto tickdiff = currentTicks - m_detail->startTicks;

	return EnergySample(units::energy::joule_t(tickdiff) * m_detail->joulesPerTick);
}

#else

using RAPLEventInfo = int;

struct RAPLDetail
{
	static std::map<std::string,RAPLEventInfo> validEvents;
};

std::vector<std::string> RAPL::detectAvailableCounters()
{
	return std::vector<std::string>();
}

RAPL::RAPL(const std::string & name) :
	EnergyDataSource(),
	m_detail(new RAPLDetail)
{
	(void)name;
}

EnergySample RAPL::read_energy()
{
	return EnergySample();
}

#endif

/***********************************************************************/

std::map<std::string,RAPLEventInfo> RAPLDetail::validEvents;

PowerDataSourcePtr RAPL::openCounter(const std::string &counterName)
{
	if (RAPLDetail::validEvents.find(counterName) == RAPLDetail::validEvents.end())
		return nullptr;

	return PowerDataSourcePtr(new RAPL(counterName));
}

Aliases RAPL::possibleAliases()
{
	return {
		{"CPU", "cores"},
		{"GPU", "gpu"},
		{"RAM", "ram"},
	};
}

RAPL::~RAPL()
{
	delete m_detail;
}

PINPOINT_REGISTER_DATA_SOURCE(RAPL)
