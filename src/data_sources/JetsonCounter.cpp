#include "JetsonCounter.h"

#include <map>
#include <fstream>

static std::map<std::string, std::string> railNameToFileName;

static std::vector<std::string> searchPaths = {
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
