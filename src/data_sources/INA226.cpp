#include "INA226.h"

#include "Registry.h"

#include <map>

std::map<std::string, std::string> counterNameToFileName;

#if defined(__linux__)

#include <fstream>
#include <dirent.h>

struct INA226Detail
{
	std::ifstream ifstrm;
};

std::string searchPath = "/sys/class/hwmon/";

std::vector<std::string> INA226::detectAvailableCounters()
{
	std::vector<std::string> counters;

	std::shared_ptr<DIR> dir(opendir(searchPath.c_str()), [](DIR *dir) {if (dir) closedir(dir);});
	if (!dir) return counters;

	struct dirent *entry;
	while ((entry = readdir(dir.get()))) {
		if (entry->d_type != DT_LNK) continue;

		auto devicePath = searchPath + entry->d_name + "/";
		std::ifstream nameFile(devicePath + "name");

		std::string deviceName;
		nameFile >> deviceName;

		if (deviceName == "ina226") {
			std::string counterName = "power-";
			counterName += entry->d_name;

			counters.push_back(counterName);
			counterNameToFileName.insert({counterName, devicePath + "power1_input"});
		}
	}

	return counters;
}

PowerSample INA226::read() {
	m_detail->ifstrm.seekg(std::ios_base::beg);
	int val;
	m_detail->ifstrm >> val;

	return PowerSample(units::power::microwatt_t(val));
}

INA226::INA226(const std::string &filename) :
	PowerDataSource(),
	m_detail(new INA226Detail)
{
	m_detail->ifstrm = std::ifstream(filename);

	if (!m_detail->ifstrm.is_open()) {
		throw std::runtime_error("Cannot open INA226 file " + filename);
	}
}

#else

struct INA226Detail
{
	;
};

std::vector<std::string> INA226::detectAvailableCounters() {
	return std::vector<std::string>();
}

INA226::INA226(const std::string &filename) :
	m_detail(new INA226Detail)
{

}

PowerSample INA226::read()
{
	return PowerSample();
}

#endif

PowerDataSourcePtr INA226::openCounter(const std::string &counterName)
{
	return PowerDataSourcePtr(new INA226(counterNameToFileName.at(counterName)));
}

Aliases INA226::possibleAliases()
{
	if (counterNameToFileName.size() >= 1) {
		return {{"IN", counterNameToFileName.cbegin()->first}};
	}

	return {};
}

INA226::~INA226()
{
	delete m_detail;
}
