#pragma once

#include <chrono>

#define DISABLE_PREDEFINED_UNITS
#define ENABLE_PREDEFINED_POWER_UNITS
#define ENABLE_PREDEFINED_ENERGY_UNITS
#define ENABLE_PREDEFINED_TIME_UNITS
#include "3rdparty/units.h"

namespace units {

UNIT_ADD_WITH_METRIC_PREFIXES(edp, joule_second, joule_seconds, Js, unit<std::ratio<1>, units::compound_unit<units::energy::joules, units::time::seconds>>);

}

inline units::time::second_t as_unit_seconds(const std::chrono::duration<double> & std_seconds)
{
	return units::time::second_t(std_seconds.count());
}


// Just required for proper formated abbrevations in io

using namespace units::power;
using namespace units::energy;
using namespace units::time;
using namespace units::edp;
