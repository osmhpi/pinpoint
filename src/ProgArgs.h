#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

#include <unistd.h>

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

struct ProgArgs
{
	// Configurable via command line options
	bool continuous_print_flag;
	bool energy_delayed_product;
	std::vector<std::string> devices;
	unsigned int runs;
	std::chrono::milliseconds delay;
	std::chrono::milliseconds interval;
	std::chrono::milliseconds before;
	std::chrono::milliseconds after;
	char **workload_and_args;

	// Inferred value
	std::string unit;

	ProgArgs(int argc, char *argv[]):
	    continuous_print_flag(false),
	    energy_delayed_product(false),
	    devices{"MCP1"},
	    runs(1),
	    delay(0),
	    interval(500),
	    before(0),
	    after(0),
	    workload_and_args(nullptr)
	{
		int c;
		while ((c = getopt (argc, argv, "hcpe:r:d:i:b:a:")) != -1) {
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
				    devices = str_split<','>(optarg);
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
			    default:
				    printHelpAndExit(argv[0], 1);
			}
		}

		if (!(optind < argc)) {
			std::cerr << "Missing workload" << std::endl;
			exit(1);
		}

		workload_and_args = &argv[optind];

		/////// Infer other stuff
		unit = energy_delayed_product ? "mJs" : "mJ";
	}

	void printHelpAndExit(char *progname, int exitcode = 0)
	{
		std::cout << "Usage: " << progname << " -h|[-c|-p] [-e dev1,dev2,...] ([-r|-d|-i|-b|-a] N)* [--] workload [args]" << std::endl;
		std::cout << "\t-h Print this help and exit" << std::endl;
		std::cout << "\t-c Continuously print power levels (mW) to stdout (skip energy stats)" << std::endl;
		std::cout << "\t-p Measure energy-delayed product (measure joules if not provided)" << std::endl;
		std::cout << "\t-e Comma seperated list of measured devices (default: all)" << std::endl;
		std::cout << "\t-r Number of runs (default: " << runs << ")" << std::endl;
		std::cout << "\t-d Delay between runs in ms (default: " << delay.count() << ")" << std::endl;
		std::cout << "\t-i Sampling interval in ms (default: " << interval.count() << ")" << std::endl;
		std::cout << "\t-b Start measurement N ms before worker creation (negative values will delay start)" << std::endl;
		std::cout << "\t-a Continue measurement N ms after worker exited" << std::endl;
		exit(exitcode);
	}

};
