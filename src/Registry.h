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

		bool available() const
		{
			return !availableCounters.empty();
		}
	};

	static void setup();

	template<typename DataSourceT>
	static int registerSource()
	{
		SourceInfo sourceInfo;
		sourceInfo.availableCounters = DataSourceT::detectAvailableCounters();
		sourceInfo.openCounter = [](const std::string & counterName){
			return DataSourceT::openCounter(counterName);
		};
		auto res = registerSource(DataSourceT::sourceName(), sourceInfo);
		DataSourceT::registerPossibleAliases();
		return res;
	}

	template<typename DataSourceT>
	static int registerAlias(const std::string & aliasName, const std::string & counterName)
	{
		return registerAlias(aliasName, DataSourceT::sourceName(), counterName);
	}

	static int registerAlias(const std::string & aliasName, const std::string & sourceName, const std::string & counterName);

	static std::vector<std::string> availableCounters();
	static std::vector<std::pair<std::string,std::string>> availableAliases();
	static PowerDataSourcePtr openCounter(const std::string & name);

private:
	static int registerSource(const std::string & sourceName, const SourceInfo & sourceInfo);
	static bool isAvailable(const std::string & sourceName, const std::string & counterName);

};
