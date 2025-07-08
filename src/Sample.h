#pragma once

#include "Units.h"
#include <chrono>

extern "C" {
	#include <time.h>
}

template<class ValueT, class ClockT>
struct Sample
{
	using value_t = ValueT;
	using clock_t = ClockT;
	using timestamp_t = std::chrono::time_point<ClockT>;

	timestamp_t timestamp;
	ValueT value;

	static timestamp_t now()
	{
		return ClockT::now();
	}

	Sample()
	{
		;;
	}

	Sample(const ValueT & _value) :
		timestamp(Sample::now()),
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

	// For C wrapper

	double in_base_unit() const
	{ return ValueT(value).template to<double>(); }

	void save_timespec(struct timespec *ts) const
	{
		auto sec = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch());
		auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(sec - timestamp.time_since_epoch());

		ts->tv_sec = sec.count();
		ts->tv_nsec = nsec.count();
	}
};

using PowerSample = Sample<units::power::watt_t, std::chrono::high_resolution_clock>;
using EnergySample = Sample<units::energy::joule_t, std::chrono::high_resolution_clock>;

