#pragma once

#include <memory>
#include <vector>

#include "Sample.h"

struct PowerDataSourceDetail;

class PowerDataSource;
typedef std::shared_ptr<PowerDataSource> PowerDataSourcePtr;

class PowerDataSource
{
public:
	// Implement this in child classes:
	//   static std::string sourceName();
	//   static std::vector<std::string> detectAvailableCounters();
	//   static PowerDataSourcePtr openCounter(const std::string & counterName);
	//   static void registerPossibleAliases();

	virtual PowerSample read() = 0;

	PowerDataSource();

	virtual ~PowerDataSource();

	void reset_acc();
	void accumulate();
	units::energy::joule_t accumulator() const;

	std::string name() const;
	void setName(const std::string & name);

	// For continuous printing

	virtual int read_mW_string(char *buf, size_t buflen) {
		return snprintf(buf, buflen, "%d\n", units::power::milliwatt_t(read().value).to<int>());
	}

private:
	std::string m_name;
	PowerDataSourceDetail *m_detail;
};
