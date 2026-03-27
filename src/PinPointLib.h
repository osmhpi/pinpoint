#pragma once

#include "PowerDataSource.h"
#include "Sampler.h"

namespace pinpoint {

extern void setup();
extern void setup_for_continous_printing(std::ostream & output_stream, bool print_headers = true, bool print_timestamp = true);

extern PowerDataSourcePtr openCounter(const std::string & name);

using PowerSamplerExperiment = Sampler;

}