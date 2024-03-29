#pragma once

#include "PowerDataSource.h"

#include <functional>
#include <string>
#include <vector>

class Registry
{
public:
	struct SourceInfo
	{
		std::vector<std::string> availableCounters;
		std::function<PowerDataSourcePtr(const std::string &)> openCounter;

		// Postpone registration to Registry::setup
		std::function<void(SourceInfo & si)> setup;
		std::function<void()> initializeExperiment;

		bool available() const
		{
			return !availableCounters.empty();
		}

		bool has_at_least_one_open_counter;
	};

	static void setup();

	template<typename DataSourceT>
	static int registerSource()
	{
		SourceInfo sourceInfo;

		sourceInfo.has_at_least_one_open_counter = false;
		sourceInfo.setup = [](SourceInfo & si){
			si.availableCounters = DataSourceT::detectAvailableCounters();
			for (const auto & alias: DataSourceT::possibleAliases()) {
				registerAlias(alias.first, DataSourceT::sourceName(), alias.second);
			}
			si.initializeExperiment = [](){ DataSourceT::initializeExperiment(); };
		};

		sourceInfo.openCounter = [](const std::string & counterName){
			return DataSourceT::openCounter(counterName);
		};

		return registerSource(DataSourceT::sourceName(), sourceInfo);
	}

	static std::vector<std::string> availableCounters();
	static std::vector<std::pair<std::string,std::string>> availableAliases();
	static PowerDataSourcePtr openCounter(const std::string & name);

	static void callInitializeExperimentsOnOpenSources();

private:
	static int registerSource(const std::string & sourceName, const SourceInfo & sourceInfo);
	static bool isAvailable(const std::string & sourceName, const std::string & counterName);

	static int registerAlias(const std::string & aliasName, const std::string & sourceName, const std::string & counterName);
};

#define PINPOINT_REGISTER_DATA_SOURCE(ClassName) \
	static int __pp_datasource_id_##ClassName = Registry::registerSource<ClassName>();
