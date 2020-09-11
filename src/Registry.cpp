#include "Registry.h"

#include "data_sources/JetsonCounter.h"
#include "data_sources/MCP_EasyPower.h"

#include <algorithm>

std::map<std::string,Registry::SourceInfo> Registry::s_sources;

void Registry::setup()
{
	Registry::registerSource<JetsonCounter>();
	Registry::registerSource<MCP_EasyPower>();
}

std::vector<std::string> Registry::availableCounters()
{
	std::vector<std::string> result;

	for (const auto & src: s_sources) {
		for (const auto & counterNames: src.second.availableCounters) {
			result.emplace_back(src.first + ":" + counterNames);
		}
	}
	return result;
}

PowerDataSourcePtr Registry::openCounter(const std::string & name)
{
	// TODO: Add Aliases

	std::string sourceName = name.substr(0, name.find(":"));
	std::string counterName = name.substr(name.find(":") + 1, name.size());

	if (s_sources.find(sourceName) == s_sources.end())
		return nullptr;

	const auto & sourceAvailableCounters = s_sources[sourceName].availableCounters;

	if (std::find(sourceAvailableCounters.begin(), sourceAvailableCounters.end(), counterName) == sourceAvailableCounters.end())
		return nullptr;

	PowerDataSourcePtr dataSource = s_sources[sourceName].openCounter(sourceName);
	dataSource->setName(name);
	return dataSource;
}
