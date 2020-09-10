#include "Registry.h"

#include "data_sources/JetsonCounter.h"
#include "data_sources/MCP_EasyPower.h"

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

