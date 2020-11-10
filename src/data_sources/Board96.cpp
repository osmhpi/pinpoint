#include "Board96.h"
#include "Registry.h"

#include <fstream>

struct Board96Detail {
  std::ifstream ifstrm;
};

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
  m_detail->ifstrm.seekg(std::ios_base::beg);
  int raw;
  m_detail->ifstrm >> raw;

  return PowerSample(units::power::microwatt_t(raw));
}

Board96::Board96(const std::string filename) {
  m_detail = new Board96Detail();
  m_detail->ifstrm = std::ifstream("/sys/class/hwmon/hwmon0/power1_input");
}

Board96::~Board96() {
  delete m_detail;
}
