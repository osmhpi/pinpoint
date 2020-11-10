#pragma once

#include <PowerDataSource.h>

#include <fstream>

class Board96: public PowerDataSource
{
public:
  static std::string sourceName()
  {
    return "board96";
  }

  static std::vector<std::string> detectAvailableCounters();
  static PowerDataSourcePtr openCounter(const std::string & counterName);
  static void registerPossibleAliases();

  virtual PowerSample read() override;

private:
  Board96(std::string filename);

  std::ifstream ifstrm;
};
