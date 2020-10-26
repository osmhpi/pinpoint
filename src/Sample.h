#pragma once

#include "Units.h"

template<class ValueT, class ClockT>
struct Sample
{
    std::chrono::time_point<ClockT> timestamp;
    ValueT value;

    Sample(const ValueT & _value) :
        timestamp(ClockT::now()),
        value(_value)
    {
        ;;
    }

    Sample(const std::chrono::time_point<ClockT> & _timestamp, const ValueT & _value) :
        timestamp(_timestamp),
        value(_value)
    {
        ;;
    }

};

using PowerSample = Sample<units::power::watt_t, std::chrono::high_resolution_clock>;
using EnergySample = Sample<units::energy::joule_t, std::chrono::high_resolution_clock>;

