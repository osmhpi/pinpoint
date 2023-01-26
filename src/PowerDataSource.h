#pragma once

#include "Alias.h"
#include "Sample.h"

#include <memory>
#include <vector>
#include <utility>

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
	//   static Aliases possibleAliases();

	PowerDataSource();
	virtual ~PowerDataSource();

	virtual PowerSample read() = 0;

	void reset_acc();
	virtual void accumulate();
	virtual units::energy::joule_t accumulator() const;

	std::string name() const;
	void setName(const std::string & name);

	using time_and_strlen = std::pair<PowerSample::timestamp_t, int>;
	// For continuous printing
	virtual time_and_strlen read_mW_string(char *buf, size_t buflen) {
		const auto sample = read();
		const int strlen = snprintf(buf, buflen, "%d\n", units::power::milliwatt_t(sample.value).to<int>());
		return std::make_pair(sample.timestamp, strlen);
	}

private:
	PowerDataSourceDetail *m_detail;
};
