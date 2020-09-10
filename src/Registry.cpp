#include "Registry.h"

#include "data_sources/JetsonCounter.h"
#include "data_sources/MCP_EasyPower.h"

std::map<std::string,Registry::SourceInfo> Registry::s_sources;

void Registry::setup()
{
	Registry::registerSource<JetsonCounter>();
	Registry::registerSource<MCP_EasyPower>();
}
