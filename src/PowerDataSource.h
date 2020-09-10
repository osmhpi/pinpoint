#pragma once

#include <memory>
#include <string>
#include <vector>

class PowerDataSource;
typedef std::shared_ptr<PowerDataSource> PowerDataSourcePtr;

class PowerDataSource
{
public:
	using accumulate_t = unsigned long;

	// Implement this in child classes
	// static std::string sourceName();
	// static std::vector<std::string> detectAvailableCounters();
	// static PowerDataSourcePtr openCounter(const std::string & counterName);

	virtual const std::string counterName() const = 0;
	virtual int read() = 0;
	virtual int read_string(char *buf, size_t buflen) = 0;

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
		m_acc += read();
	}

	accumulate_t accumulator() const
	{
		return m_acc;
	}

protected:
	accumulate_t m_acc;
};
