#pragma once

#include <PowerDataSource.h>

// This class can likely be used for all INA2xx chips with minor adjustments
// See https://www.kernel.org/doc/html/latest/hwmon/ina2xx.html

struct INA226Detail;

class INA226: public PowerDataSource
{
public:
	static std::string sourceName()
	{
		return "ina226";
	}

	static std::vector<std::string> detectAvailableCounters();
	static PowerDataSourcePtr openCounter(const std::string & counterName);
	static Aliases possibleAliases();

	virtual PowerSample read() override;

	virtual ~INA226();

private:
	INA226(const std::string &filename);

	struct INA226Detail *m_detail;
};
