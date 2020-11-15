#include "Board96.h"

#include "Registry.h"

#include <fstream>
#include <map>
#include <memory>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct Board96Detail {
  std::ifstream ifstrm;
};

std::string searchPath = "/sys/class/hwmon/";

std::map<std::string, std::string> counterNameToFileName;

std::vector<std::string> Board96::detectAvailableCounters()
{
  std::vector<std::string> counters;

  std::shared_ptr<DIR> dir(opendir(searchPath.c_str()), [](DIR *dir) {if (dir) closedir(dir);});
  if (!dir) return counters;

  struct dirent *entry;
  while ((entry = readdir(dir.get()))) {
    if (entry->d_type != DT_LNK) continue;

    auto devicePath = searchPath + entry->d_name + "/";
    std::ifstream nameFile(devicePath + "name");

    std::string deviceName;
    nameFile >> deviceName;

    if (deviceName == "ina226") {
      std::string counterName = "power-";
      counterName += entry->d_name;

      counters.push_back(counterName);
      counterNameToFileName.insert({counterName, devicePath + "power1_input"});
    }
  }

  return counters;
}

PowerDataSourcePtr Board96::openCounter(const std::string &counterName)
{
  return PowerDataSourcePtr(new Board96(counterNameToFileName.at(counterName)));
}

void Board96::registerPossibleAliases()
{
  Registry::registerAlias<Board96>("IN", "power1");
}

PowerSample Board96::read() {
  m_detail->ifstrm.seekg(std::ios_base::beg);
  int val;
  m_detail->ifstrm >> val;

  return PowerSample(units::power::microwatt_t(val));
}

Board96::Board96(const std::string &filename) {
  m_detail = new Board96Detail();
  m_detail->ifstrm = std::ifstream(filename);
}

Board96::~Board96() {
  delete m_detail;
}
