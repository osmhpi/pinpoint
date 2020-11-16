#pragma once

#include "PowerDataSource.h"

#include <stdexcept>
#include <cstdio>

struct JetsonCounterDetail;

class JetsonCounter: public PowerDataSource
{
public:
	static std::string sourceName()
	{
		return "jetson";
	}

	static std::vector<std::string> detectAvailableCounters();
	static PowerDataSourcePtr openCounter(const std::string & counterName);
	static Aliases possibleAliases();

	virtual ~JetsonCounter();
	virtual int read_mW_string(char *buf, size_t buflen);

	virtual PowerSample read()
	{
		char buf[255];
		read_mW_string(buf, sizeof(buf));
		return PowerSample(units::power::milliwatt_t(atoi(buf)));
	}

private:
	JetsonCounterDetail *m_detail;

	JetsonCounter(const std::string & filename);
};
