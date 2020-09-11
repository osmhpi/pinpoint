#pragma once

#include "PowerDataSource.h"

#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

extern "C" {
	#include "mcp_com.h"
}

class MCP_EasyPower: public PowerDataSource
{
public:
	using accumulate_t = int;

	static std::string sourceName()
	{
		return "mcp";
	}

	static std::vector<std::string> detectAvailableCounters()
	{
		return std::vector<std::string>();
	}

	static PowerDataSourcePtr openCounter(const std::string & counterName)
	{
		(void)counterName;
		return PowerDataSourcePtr(nullptr);
	}

	static void registerPossibleAliases()
	{
		;;
	}

	MCP_EasyPower(const std::string & filename) :
		PowerDataSource()
	{
		if (!(m_fd = f511_init(filename.c_str())))
			throw std::runtime_error("Cannot open " + filename);

	}

	virtual ~MCP_EasyPower()
	{
		close(m_fd);
	}

	virtual int read()
	{
		int ch1, ch2;
		f511_get_power(&ch1, &ch2, m_fd);
		return ch1 * 10; // MCP returns data in 10mW steps
	}

	virtual int read_string(char *buf, size_t buflen)
	{
		return snprintf(buf, buflen, "%d\n", read());
	}

private:
	int m_fd;

};
