#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Units.h"

class PowerDataSource;
typedef std::shared_ptr<PowerDataSource> PowerDataSourcePtr;

class PowerDataSource
{
public:
	using accumulate_t = unsigned long;

	// Implement this in child classes:
	//   static std::string sourceName();
	//   static std::vector<std::string> detectAvailableCounters();
	//   static PowerDataSourcePtr openCounter(const std::string & counterName);
	//   static void registerPossibleAliases();

	virtual units::power::watt_t read() = 0;

	PowerDataSource() :
		m_acc(0)
	{
		;;
	}

	virtual ~PowerDataSource()
	{
		;;
	}

	void reset_acc()
	{
		m_acc = 0;
	}

	void accumulate()
	{
		m_acc += read().to<accumulate_t>();
	}

	accumulate_t accumulator() const
	{
		return m_acc;
	}

	std::string name() const
	{
		return m_name;
	}

	void setName(const std::string & name)
	{
		m_name = name;
	}

	// For continuous printing

	virtual int read_mW_string(char *buf, size_t buflen) {
		return snprintf(buf, buflen, "%d\n", units::power::milliwatt_t(read()).to<int>());
	}

protected:
	accumulate_t m_acc;

private:
	std::string m_name;
};
