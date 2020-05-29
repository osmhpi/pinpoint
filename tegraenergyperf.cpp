#include "TegraDeviceInfo.h"
#include <iostream>
#include <unistd.h>

int main()
{
	TegraDeviceInfo tdiCPU(TEGRA_CPU_DEV);

	while (1) {
		std::cout << tdiCPU.read() << std::endl;
		usleep(500 * 1000);
	}

	return 0;
}
