#pragma once

#include "PowerDataSource.h"

struct MCP_EasyPowerDetail;

class MCP_EasyPower: public PowerDataSource
{
public:
	static std::string sourceName()
	{
		return "mcp";
	}

	static std::vector<std::string> detectAvailableCounters();
	static PowerDataSourcePtr openCounter(const std::string & counterName);
	static void registerPossibleAliases();

	virtual ~MCP_EasyPower();

	virtual int read();

	virtual int read_string(char *buf, size_t buflen)
	{
		return snprintf(buf, buflen, "%d\n", read());
	}

private:
	struct MCP_EasyPowerDetail *m_detail;

	MCP_EasyPower(const std::string & filename, const unsigned int channel);
};
