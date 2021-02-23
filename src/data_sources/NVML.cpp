#include "NVML.h"
#include "Registry.h"

#include <map>
#include <string>
#include <sstream>
#include <algorithm>


#if defined(__linux__) && defined (USE_NVML)

#include <nvml.h>


std::vector<std::string> NVML::detectAvailableCounters()
{

	nvmlReturn_t result;
	unsigned int device_count;
	std::vector<std::string> counters;

	result = nvmlInit();
	if (NVML_SUCCESS != result)
	{
		std::ostringstream error_stream;
		error_stream << "Failed to initialize nvml: ";
		error_stream << nvmlErrorString(result);
		throw std::runtime_error(error_stream.str());
	}

	result = nvmlDeviceGetCount(&device_count);
	if (NVML_SUCCESS != result)
	{
		std::ostringstream error_stream;
		error_stream << "Failed to query device count: ";
		error_stream << nvmlErrorString(result);
		throw std::runtime_error(error_stream.str());
	}

	for (int i = 0; i < device_count; i++)
	{
		nvmlDevice_t device;
		char device_name[64];

		result = nvmlDeviceGetHandleByIndex(i, &device);
		if (NVML_SUCCESS != result)
		{
			std::ostringstream error_stream;
			error_stream << "Failed to query device by index: ";
			error_stream << nvmlErrorString(result);
			throw std::runtime_error(error_stream.str());
		}

		result = nvmlDeviceGetName(device, device_name, 64);
		if (NVML_SUCCESS != result)
		{
			std::ostringstream error_stream;
			error_stream << "Failed to query device name: ";
			error_stream << nvmlErrorString(result);
			throw std::runtime_error(error_stream.str());
		}

		// transform device name according to pinpoint naming convention
		std::string str(device_name);
		std::transform(str.begin(), str.end(), str.begin(), [](char ch) {
				return ch == ' ' ? '_' : ch;
				});
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		str += "_";
		str += std::to_string(i);

		counters.push_back(str);
		NVMLDetail::validEvents[str] = i;
	}
	
	result = nvmlShutdown();
	if (NVML_SUCCESS != result)
	{
		std::ostringstream error_stream;
		error_stream << "Failed to shutdown nvml: ";
		error_stream << nvmlErrorString(result);
		throw std::runtime_error(error_stream.str());
	}

	return counters;
}

NVML::NVML(const std::string & name) :
	PowerDataSource()
{
	m_detail.index = NVMLDetail::validEvents[name];
}

PowerSample NVML::read()
{
	nvmlReturn_t result;
	nvmlDevice_t device;
	unsigned int power;

	result = nvmlInit();
	if (NVML_SUCCESS != result)
	{
		std::ostringstream error_stream;
		error_stream << "Failed to initialize nvml: ";
		error_stream << nvmlErrorString(result);
		throw std::runtime_error(error_stream.str());
	}

	result = nvmlDeviceGetHandleByIndex(m_detail.index, &device);
	if (NVML_SUCCESS != result)
	{
		std::ostringstream error_stream;
		error_stream << "Failed to query device by index: ";
		error_stream << nvmlErrorString(result);
		throw std::runtime_error(error_stream.str());
	}

	result = nvmlDeviceGetPowerUsage(device, &power);
	if (NVML_SUCCESS != result)
	{
		std::ostringstream error_stream;
		error_stream << "Failed to query device power usage: ";
		error_stream << nvmlErrorString(result);
		throw std::runtime_error(error_stream.str());
	}

	result = nvmlShutdown();
	if (NVML_SUCCESS != result)
	{
		std::ostringstream error_stream;
		error_stream << "Failed to shutdown nvml: ";
		error_stream << nvmlErrorString(result);
		throw std::runtime_error(error_stream.str());
	}

	return PowerSample(units::power::milliwatt_t(power));
}


// #elif defined(__x86_64__) && defined(__WIN32) && defined (USE_NVML)
//
// no Windows implementation yet ..
//
// #elif defined(__x86_64__) && defined(__APPLE__) && defined(__MACH__) && defined (USE_NVML)
//
// no OS X implementation yet ..
//
#else

std::vector<std::string> NVML::detectAvailableCounters()
{
	return std::vector<std::string>();
}

NVML::NVML(const std::string & name) :
	PowerDataSource()
{
	m_detail.index = NVMLDetail::validEvents[name];
}

PowerSample NVML::read()
{
	return PowerSample();
}

#endif

/***********************************************************************/

std::map<std::string,DeviceIndex> NVMLDetail::validEvents;

PowerDataSourcePtr NVML::openCounter(const std::string &counterName)
{
	if (NVMLDetail::validEvents.find(counterName) == NVMLDetail::validEvents.end())
		return nullptr;

	return PowerDataSourcePtr(new NVML(counterName));
}

Aliases NVML::possibleAliases()
{
	Aliases aliases;

	// With multiple GPUs we favor using the nvml:-prefix over Aliases
	if (NVMLDetail::validEvents.size() == 1) {
		for (auto device: NVMLDetail::validEvents)
		{
			aliases["GPU"] = device.first;
		}
	}

	return aliases;
}

NVML::~NVML()
{
	;;
}

PINPOINT_REGISTER_DATA_SOURCE(NVML)
