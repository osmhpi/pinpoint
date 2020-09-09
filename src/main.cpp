#include "Sampler.h"
#include "Experiment.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <thread>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

/**********************************************************/


int main(int argc, char *argv[])
{
	ProgArgs args(argc, argv);
	Experiment experiment(args);

	experiment.run();
	experiment.printResult();

	return 0;
}
