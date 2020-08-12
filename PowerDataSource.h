#pragma once

#include <string>

class PowerDataSource
{
public:
	using accumulate_t = unsigned long;

	virtual const std::string name() const = 0;
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
