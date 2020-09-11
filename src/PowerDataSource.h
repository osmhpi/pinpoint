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

	std::string name() const
	{
		return m_name;
	}

	void setName(const std::string & name)
	{
		m_name = name;
	}

protected:
	accumulate_t m_acc;

private:
	std::string m_name;
};
