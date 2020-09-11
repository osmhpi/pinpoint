#include "Experiment.h"
#include "Sampler.h"
#include "Registry.h"

int main(int argc, char *argv[])
{
	ProgArgs args(argc, argv);
	Registry::setup();

	args.validate();

	try	{
		Experiment experiment(args);

		experiment.run();
		experiment.printResult();
	} catch (const std::exception & e) {
		std::cerr << "[ERROR] " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
