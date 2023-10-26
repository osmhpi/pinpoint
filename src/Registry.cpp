#include "Registry.h"

#include <algorithm>
#include <map>
#include <utility>
#include <set>

static std::map<std::string,Registry::SourceInfo> s_sources;
static std::map<std::string,std::pair<std::string,std::string>> s_aliases;

/**************************************************************/

void Registry::setup()
{
	for (auto & src: s_sources) {
		SourceInfo & si = src.second;
		si.setup(si);
	}
}

/**************************************************************/

int Registry::registerSource(const std::string & sourceName, const SourceInfo & sourceInfo)
{
	s_sources[sourceName] = sourceInfo;
	return s_sources.size();
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

std::vector<std::pair<std::string,std::string>> Registry::availableAliases()
{
	std::vector<std::pair<std::string,std::string>> result;

	for (const auto & alias: s_aliases) {
		result.push_back(std::make_pair(alias.first,
										alias.second.first + ":" + alias.second.second));
	}
	return result;
}

PowerDataSourcePtr Registry::openCounter(const std::string & name)
{
	std::string sourceName, counterName;

	if (name.find(':') != std::string::npos) {
		sourceName = name.substr(0, name.find(':'));
		counterName = name.substr(name.find(':') + 1, name.size());

		if (!isAvailable(sourceName, counterName))
			return nullptr;
	} else {
		if (s_aliases.find(name) == s_aliases.end())
			return nullptr;

		const auto & alias = s_aliases.at(name);
		sourceName = alias.first;
		counterName = alias.second;
	}

	PowerDataSourcePtr dataSource = s_sources[sourceName].openCounter(counterName);
	dataSource->setName(name);
	s_sources[sourceName].has_at_least_one_open_counter = true;

	return dataSource;
}

void Registry::callInitializeExperimentsOnOpenSources()
{
	for (auto & name_si: s_sources) {
		if (name_si.second.has_at_least_one_open_counter) {
			name_si.second.has_at_least_one_open_counter = false;
			name_si.second.initializeExperiment();
		}
	}
}

bool Registry::isAvailable(const std::string & sourceName, const std::string & counterName)
{
	if (s_sources.find(sourceName) == s_sources.end())
		return false;

	const auto & srcAvailCntrs = s_sources[sourceName].availableCounters;
	return std::find(srcAvailCntrs.begin(), srcAvailCntrs.end(), counterName) != srcAvailCntrs.end();
}

int Registry::registerAlias(const std::string &aliasName, const std::string &sourceName, const std::string &counterName)
{
	// TODO: What happens with multiple CPUs? Should alias stand for a list of counters? With priority? Or all at once?
	if (!isAvailable(sourceName, counterName) || (s_aliases.find(aliasName) != s_aliases.end())) {
		return -1;
	}

	s_aliases[aliasName] = std::make_pair(sourceName, counterName);
	return s_aliases.size();
}
