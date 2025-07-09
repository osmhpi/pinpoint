#include "PinPoint.h"
#include "Registry.h"

#include "Settings.h"

namespace pinpoint {

void setup()
{
	Registry::setup();
}

void setup_for_continous_printing(std::ostream & output_stream, bool print_headers, bool print_timestamps)
{
	setup();
	settings::output_stream.rdbuf(output_stream.rdbuf());
	settings::continuous_print_flag = true;
	settings::continuous_header_flag = print_headers;
	settings::countinous_timestamp_flag = print_timestamps;
}

PowerDataSourcePtr openCounter(const std::string & name)
{
	return Registry::openCounter(name);
}

}