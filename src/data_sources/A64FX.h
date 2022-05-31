#pragma once

#include "EnergyDataSource.h"

struct A64FXDetail;

class A64FX: public EnergyDataSource
{
public:
	static std::string sourceName()
	{
		return "a64fx";
	}

	static std::vector<std::string> detectAvailableCounters();
	static PowerDataSourcePtr openCounter(const std::string & counterName);
	static Aliases possibleAliases();

	virtual ~A64FX();

	virtual EnergySample read_energy();

private:
	struct A64FXDetail *m_detail;
	A64FX(const std::string & name);
};
