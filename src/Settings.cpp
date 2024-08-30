#include "Settings.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <unistd.h>
#include <getopt.h>

namespace settings {

bool continuous_print_flag = false;
bool continuous_timestamp_flag = false; // print the timestamp before each measurement if using -c
bool no_workload_flag = false; // skip execution of workload and print continuously with -c (needs -c to be set)
bool continuous_header_flag = false;
bool countinous_timestamp_flag = false;
bool energy_delayed_product = false;
bool print_counter_list = false;

bool print_total_flag = false;

std::vector<std::string> counters;
unsigned int runs = 1;
std::chrono::milliseconds delay(0);
std::chrono::milliseconds interval(50);
std::chrono::milliseconds before(0);
std::chrono::milliseconds after(0);
char **workload_and_args = nullptr;

uid_t uid = settings::UID_NOT_SET;

namespace _private {
	static std::ofstream output_file;
}

std::ostream & output_stream = std::cerr;

// --------------------------------------------------------------

template<char delimiter>
class WordDelimitedBy : public std::string
{};

template<char delimiter>
std::istream& operator>>(std::istream& is, WordDelimitedBy<delimiter>& output)
{
	std::getline(is, output, delimiter);
	return is;
}

template<char delimiter>
std::vector<std::string> str_split(const std::string & text)
{
	std::istringstream iss(text);
	return std::vector<std::string>
			((std::istream_iterator<WordDelimitedBy<delimiter>>(iss)),
			  std::istream_iterator<WordDelimitedBy<delimiter>>());
}

void printHelpAndExit(char *progname, int exitcode = 0)
{
	std::cout << "Usage: " << progname << " -h|[-c [--header] [--timestamp]|-p] [-e dev1,dev2,...] ([-r|-d|-i|-b|-a] N)* [--] workload [args]" << std::endl;
	std::cout << "\t-h Print this help and exit" << std::endl;
	std::cout << "\t-l Print a list of available counters and exit" << std::endl;
	std::cout << "\t-c Continuously print power levels (mW) to stdout (skip energy stats)" << std::endl;
	std::cout << "\t-p Measure energy-delayed product (measure joules if not provided)" << std::endl;
	std::cout << "\t-e Comma seperated list of measured counters (default: all available)" << std::endl;
	std::cout << "\t-r Number of runs (default: " << runs << ")" << std::endl;
	std::cout << "\t-d Delay between runs in ms (default: " << delay.count() << ")" << std::endl;
	std::cout << "\t-i Sampling interval in ms (default: " << interval.count() << ")" << std::endl;
	std::cout << "\t-b Start measurement N ms before worker creation (negative values will delay start)" << std::endl;
	std::cout << "\t-a Continue measurement N ms after worker exited" << std::endl;
	std::cout << "\t-n Disable execution of workload. Only works with -c" << std::endl;
	std::cout << "\t-o Output file (default: stderr)" << std::endl;
	std::cout << "\t-U Run the workload under this uid" << std::endl;
	std::cout << std::endl;
	std::cout << "\t--header If continuously printing, print the counter names before each run" << std::endl;
	std::cout << "\t--timestamp If continuously printing, print the maximum timestamp (timer epoch) of each sample group" << std::endl;
	std::cout << "\t--total If continuously printing, also print total stats" << std::endl;
	exit(exitcode);
}

// --------------------------------------------------------------

enum Longopt {
	header = 256,
	timestamp = 257,
	total = 258,
};

static struct option longopts[] = {
	{"header", no_argument, NULL, header},
	{"timestamp", no_argument, NULL, timestamp},
	{"total", no_argument, NULL, total},
	{0, 0, 0, 0}
};

void readProgArgs(int argc, char *argv[])
{
	int c;
	while ((c = getopt_long (argc, argv, "hlcpne:r:d:i:b:a:o:U:", longopts, NULL)) != -1) {
		switch (c) {
			case 'h':
			case '?':
				printHelpAndExit(argv[0]);
				break;
			case 'c':
				continuous_print_flag = true;
				break;
			case 'p':
				energy_delayed_product = true;
				break;
			case 'e':
				counters = str_split<','>(optarg);
				break;
			case 'r':
				runs = atoi(optarg);
				if (runs < 1) {
					std::cerr << "Invalid number of runs" << std::endl;
					exit(1);
				}
				break;
			case 'd':
				delay = std::chrono::milliseconds(atoi(optarg));
				break;
			case 'i':
				interval = std::chrono::milliseconds(atoi(optarg));
				break;
			case 'a':
				after = std::chrono::milliseconds(atoi(optarg));
				break;
			case 'b':
				before = std::chrono::milliseconds(atoi(optarg));
				break;
			case 'l':
				print_counter_list = true;
				break;
			case 'o':
				_private::output_file.open(optarg);
				if (!_private::output_file.is_open()) {
					std::cerr << "Cannot open output file \"" << optarg << "\"" << std::endl;
					exit(1);
				}
				output_stream.rdbuf(_private::output_file.rdbuf());
				break;
			case 'U':
				uid = atoi(optarg);
				break;
			case 'n':
			     no_workload_flag = true;
			     break;
			case header:
				continuous_header_flag = true;
				break;
			case timestamp:
				countinous_timestamp_flag = true;
				break;
			case total:
				print_total_flag = true;
				break;
			default:
				printHelpAndExit(argv[0], 1);
		}
	}

	if (!(optind < argc) || no_workload_flag) {
		workload_and_args = nullptr;
	} else {
		workload_and_args = &argv[optind];
	}
}

void validate()
{
	if (print_counter_list) {
		std::cout << "List of available counters (to be used in -e):" << std::endl;
		for (const auto & counter: Registry::availableCounters()) {
			std::cout << "\t" << counter << std::endl;
		}

		std::cout << std::endl << "List of available aliases:" << std::endl;
		for (const auto & alias: Registry::availableAliases()) {
			std::cout << "\t" << alias.first << " -> " << alias.second << std::endl;
		}
		exit(0);
	}

	if (no_workload_flag && !continuous_print_flag) {
		std::cerr << "-n only works if continous output (-c) is enabled." << std::endl;
		exit(1);
	}
	
	if (!workload_and_args && !(no_workload_flag && continuous_print_flag)) {
		std::cerr << "Missing workload" << std::endl;
		exit(1);
	}

	if (counters.empty()) {
		// If no counter selected (default), open them all
		counters = Registry::availableCounters();
		if (counters.empty()) {
			std::cerr << "No counters available on this system" << std::endl;
		}
	 }
}

}
