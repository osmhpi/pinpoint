#include "Board96.h"
#include "Registry.h"

#include <iostream>

std::vector<std::string> Board96::detectAvailableCounters()
{
  return std::vector<std::string> {"power1_input"};
}

PowerDataSourcePtr Board96::openCounter(const std::string & counterName)
{
  return PowerDataSourcePtr(new Board96(counterName));
}

void Board96::registerPossibleAliases()
{
  Registry::registerAlias<Board96>("IN", "power1_input");
}

PowerSample Board96::read() {
  ifstrm.seekg(std::ios_base::beg);
  int raw;
  ifstrm >> raw;

  return PowerSample(units::power::microwatt_t(raw));
}

Board96::Board96(const std::string filename) {
  ifstrm = std::ifstream("/sys/class/hwmon/hwmon0/power1_input");
}
