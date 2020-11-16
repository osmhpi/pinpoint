#include "JetsonCounter.h"
#include "Registry.h"

#include <map>
#include <fstream>

struct JetsonCounterDetail
{
	std::string filename;
	FILE *fp;
};

static std::map<std::string, std::string> railNameToFileName;

static const std::vector<std::string> searchPaths = {
	"/sys/bus/i2c/devices/0-0040/", // Tegra TX2
	"/sys/bus/i2c/devices/0-0041/",
	"/sys/bus/i2c/devices/1-0040/", // Xavier AGX
	"/sys/bus/i2c/devices/1-0041/"
};

static void test_iio_device(const std::string & path, const std::string & counter)
{
	std::string powerFileName = path + "iio_device/in_power" + counter + "_input";
	std::ifstream railNameFile(path + "iio_device/rail_name_" + counter);
	std::ifstream powerInputFile(powerFileName);

	if (!railNameFile || !powerInputFile)
		return;

	std::string railName;
	std::getline(railNameFile, railName);
	railNameToFileName[railName] = powerFileName;
}

static void searchFolder(const std::string & path)
{
	std::ifstream deviceNameFile(path + "iio_device/name");
	if (!deviceNameFile)
		return;

	std::string deviceName;
	std::getline(deviceNameFile, deviceName);

	if (deviceName.compare("ina3221x") != 0)
		return;

	for (int i = 0; i <= 2; i++) {
		test_iio_device(path, std::to_string(i));
	}
}

std::vector<std::string> JetsonCounter::detectAvailableCounters()
{
	std::vector<std::string> result;
	railNameToFileName.clear();

	for (const auto & path: searchPaths)
		searchFolder(path);

	for (const auto & imap: railNameToFileName) {
		result.emplace_back(imap.first);
	}
	return result;
}


Aliases JetsonCounter::possibleAliases()
{
	return {
		// Tegra TX2
		{"CPU", "VDD_SYS_CPU"},
		{"DDR", "VDD_SYS_DDR"},
		{"MEM", "VDD_SYS_DDR"},
		{"GPU", "VDD_SYS_GPU"},
		{"SOC", "VDD_SYS_SOC"},
		{"IN",  "VDD_IN"},
		{"WIFI", "VDD_4V0_WIFI"},

		// Xavier AGX
		{"CPU", "CPU"},
		{"DDR", "VDDRQ"},
		{"MEM", "VDDRQ"},
		{"GPU", "GPU"},
		{"SOC", "SOC"},
		{"CV",  "CV"},
		// The SYS5V rail is nonsense, hence skipped
	};
}

PowerDataSourcePtr JetsonCounter::openCounter(const std::string & counterName)
{
	return PowerDataSourcePtr(new JetsonCounter(railNameToFileName.at(counterName)));
}

JetsonCounter::JetsonCounter(const std::string & filename) :
	PowerDataSource(),
	m_detail(new JetsonCounterDetail)
{
	m_detail->filename = filename;
	m_detail->fp = fopen(m_detail->filename.c_str(), "r");
	if (m_detail->fp == NULL)
		throw std::runtime_error("Cannot open " + m_detail->filename);
}

JetsonCounter::~JetsonCounter()
{
	fclose(m_detail->fp);
	delete  m_detail;
}

int JetsonCounter::read_mW_string(char *buf, size_t buflen)
{
	size_t pos;
	rewind(m_detail->fp);
	pos = fread(buf, sizeof(char), buflen, m_detail->fp);
	if (pos > 0)
		buf[pos-1] = '\0';
	return pos;
}

PINPOINT_REGISTER_DATA_SOURCE(JetsonCounter)
