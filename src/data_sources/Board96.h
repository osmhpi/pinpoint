#pragma once

#include <PowerDataSource.h>

#include <fstream>

struct Board96Detail;

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

  virtual ~Board96();

private:
  Board96(std::string filename);

  struct Board96Detail *m_detail;
};
