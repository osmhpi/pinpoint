#include "TegraDeviceInfo.h"
#include <iostream>
#include <unistd.h>

/**********************************************************/

struct ProgArgs
{

	bool energy_squared_flag;
	int interval;
	int before;
	int after;
	char **command_and_args;

	ProgArgs(int argc, char *argv[]):
		energy_squared_flag(false),
		interval(500),
		before(0),
		after(0),
		command_and_args(nullptr)
	{
		int c;
		while ((c = getopt (argc, argv, "hsi:b:a:")) != -1) {
			switch (c) {
				case 'h':
					printHelpAndExit(argv[0]);
					break;
				case 's':
					energy_squared_flag = true;
					break;
				case 'i':
					interval = atoi(optarg);
					break;
				case 'a':
					after = atoi(optarg);
					break;
				case 'b':
					before = atoi(optarg);
					break;
				default:
					exit(1);
			}
		}
		
		if (!(optind < argc)) {
			std::cerr << "Missing workload" << std::endl;
			exit(1);
		}

		command_and_args = &argv[optind];
	}

	void printHelpAndExit(char *progname)
	{
		std::cout << "Usage: " << progname << "[-h|[-s] [-i interval] [-b N] [-a N] -- workload [args]]" << std::endl;
		std::cout << "\t-h Print this help and exit" << std::endl;
		std::cout << "\t-s Measure energy-squared product (measure joules if not provided)" << std::endl;
		std::cout << "\t-i Sampling interval in ms (default: " << interval << ")" << std::endl;
		std::cout << "\t-b Start measurement N ms before child execution (negative values will delay)" << std::endl;
		std::cout << "\t-a Continue measurement N ms after child exited" << std::endl;
		exit(0);
	}

};

/**********************************************************/

int main(int argc, char *argv[])
{
	ProgArgs args(argc, argv);

	std::cout << args.interval << std::endl;
	std::cout << args.energy_squared_flag << std::endl;
	std::cout << args.before << std::endl;
	std::cout << args.after << std::endl;
	while (args.command_and_args && *args.command_and_args) {
		std::cout << *args.command_and_args << std::endl;
		args.command_and_args++;
	}

	return 0;
}
