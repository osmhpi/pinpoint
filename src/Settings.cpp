#include "Settings.h"

#include <iostream>
#include <iterator>
#include <sstream>
#include <unistd.h>

namespace settings {

bool continuous_print_flag = false;
bool energy_delayed_product = false;
bool print_counter_list = false;

std::vector<std::string> counters;
unsigned int runs = 1;
std::chrono::milliseconds delay(0);
std::chrono::milliseconds interval(50);
std::chrono::milliseconds before(0);
std::chrono::milliseconds after(0);
char **workload_and_args = nullptr;

std::string unit;

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
	std::cout << "Usage: " << progname << " -h|[-c|-p] [-e dev1,dev2,...] ([-r|-d|-i|-b|-a] N)* [--] workload [args]" << std::endl;
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
	exit(exitcode);
}

// --------------------------------------------------------------


void readProgArgs(int argc, char *argv[])
{
	int c;
	while ((c = getopt (argc, argv, "hlcpe:r:d:i:b:a:")) != -1) {
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
			default:
				printHelpAndExit(argv[0], 1);
		}
	}

	if (!(optind < argc)) {
		workload_and_args = nullptr;
	} else {
		workload_and_args = &argv[optind];
	}

	/////// Infer other stuff
	unit = energy_delayed_product ? "mJs" : "mJ";
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
	if (!workload_and_args) {
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
