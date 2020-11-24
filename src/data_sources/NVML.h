#pragma once

#include "PowerDataSource.h"

using DeviceIndex = int;

struct NVMLDetail
{
	DeviceIndex index;
	static std::map<std::string, DeviceIndex> validEvents;
};

class NVML: public PowerDataSource
{
public:
	static std::string sourceName()
	{
		return "nvml";
	}

	static std::vector<std::string> detectAvailableCounters();
	static PowerDataSourcePtr openCounter(const std::string & counterName);
	static Aliases possibleAliases();

	virtual ~NVML();

	virtual PowerSample read();

private:
	struct NVMLDetail m_detail;
	NVML(const std::string & name);
};
