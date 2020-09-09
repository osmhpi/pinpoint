#include "Sampler.h"
#include "Experiment.h"

int main(int argc, char *argv[])
{
	ProgArgs args(argc, argv);
	Experiment experiment(args);

	experiment.run();
	experiment.printResult();

	return 0;
}
