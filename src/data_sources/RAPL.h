#pragma once

#include "EnergyDataSource.h"

struct RAPLDetail;

class RAPL: public EnergyDataSource
{
public:
	static std::string sourceName()
	{
		return "rapl";
	}

	static std::vector<std::string> detectAvailableCounters();
	static PowerDataSourcePtr openCounter(const std::string & counterName);
	static void registerPossibleAliases();

	virtual ~RAPL();

	virtual EnergySample read_energy();

private:
	struct RAPLDetail *m_detail;
	RAPL(const std::string & name);
};
