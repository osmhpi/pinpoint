#pragma once

#include "Registry.h"

#include <chrono>
#include <vector>



namespace settings {


// Configurable via command line options
extern bool continuous_print_flag;
extern bool continuous_header_flag;
extern bool	countinous_timestamp_flag;
extern bool energy_delayed_product;
extern bool print_counter_list;

extern std::vector<std::string> counters;
extern unsigned int runs;
extern std::chrono::milliseconds delay;
extern std::chrono::milliseconds interval;
extern std::chrono::milliseconds before;
extern std::chrono::milliseconds after;
extern char **workload_and_args;

// Inferred value
extern std::string unit;

extern void readProgArgs(int argc, char *argv[]);
extern void validate();


} // namespace settings
