#pragma once

#if defined(__APPLE__) && defined(__MACH__) && defined(__AARCH64EL__)

#include <EnergyDataSource.h>

struct AppleMDetail;

class AppleM: public EnergyDataSource
{
public:
	static std::string sourceName()
	{
		return "applem";
	}

	static std::vector<std::string> detectAvailableCounters();
	static PowerDataSourcePtr openCounter(const std::string & counterName);
	static Aliases possibleAliases();

	virtual EnergySample read_energy() override;

	virtual ~AppleM();

	static void initializeExperiment();

private:
	AppleM(const std::string & key);

	struct AppleMDetail *m_detail;
};

#endif
