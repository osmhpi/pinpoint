#pragma once

#include "Units.h"

template<class ValueT, class ClockT>
struct Sample
{
	using value_t = ValueT;
	using clock_t = ClockT;
	using timestamp_t = std::chrono::time_point<ClockT>;

	timestamp_t timestamp;
	ValueT value;

	Sample()
	{
		;;
	}

	Sample(const ValueT & _value) :
		timestamp(ClockT::now()),
		value(_value)
	{
		;;
	}

	Sample(const timestamp_t & _timestamp, const ValueT & _value) :
		timestamp(_timestamp),
		value(_value)
	{
		;;
	}

};

using PowerSample = Sample<units::power::watt_t, std::chrono::high_resolution_clock>;
using EnergySample = Sample<units::energy::joule_t, std::chrono::high_resolution_clock>;

