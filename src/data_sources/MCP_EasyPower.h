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

	virtual units::power::watt_t read();

private:
	struct MCP_EasyPowerDetail *m_detail;

	MCP_EasyPower(const unsigned int dev, const unsigned int channel);
};
