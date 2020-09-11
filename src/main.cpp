#include "Experiment.h"
#include "Sampler.h"
#include "Registry.h"

int main(int argc, char *argv[])
{
	ProgArgs args(argc, argv);
	Registry::setup();

	args.validate();

	Experiment experiment(args);

	experiment.run();
	experiment.printResult();

	return 0;
}
