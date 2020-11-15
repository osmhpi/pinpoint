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
  // search /sys/class/hwmon/
  // find ina226 chips
  
  std::shared_ptr<DIR> dir(opendir(searchPath.c_str()), [](DIR *dir) {if (dir) closedir(dir);});
  if (!dir) return counters;

  struct dirent *entry;
  while ((entry = readdir(dir.get()))) {
    if (entry->d_type != DT_LNK) continue;

    struct stat sb;
    lstat(entry->d_name, &sb);

    std::string linkPath = searchPath + entry->d_name;
    size_t bufSize = PATH_MAX;
    char *buf = (char*) malloc(bufSize);
    ssize_t x = readlink(linkPath.c_str(), buf, bufSize);
    buf[x] = '\0';
    if (x == -1) {
      perror("readlink");
    }

    auto devicePath = searchPath + buf;
    std::ifstream nameFile(devicePath + "/name");

    std::string name;
    nameFile >> name;

    if (name == "ina226") {
      counterNameToFileName.insert({"power1", devicePath + "/power1_input"});
    }
  }

  return std::vector<std::string> {"power1"};
}

PowerDataSourcePtr Board96::openCounter(const std::string &counterName)
{
  std::cout << "found: " << counterNameToFileName.at(counterName) << std::endl;
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
