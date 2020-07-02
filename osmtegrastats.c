#include "tegra_device_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	long interval = 500;
	int i, pos, avail, nbytes;
	char buf[255];
	struct tegra_device_info devices[5];

	if (argc > 1)
		interval = atol(argv[1]);

	tegra_open_device(&devices[0], "CPU", TEGRA_CPU_DEV);
	tegra_open_device(&devices[1], "GPU", TEGRA_GPU_DEV);
	tegra_open_device(&devices[2], "SOC", TEGRA_SOC_DEV);
	tegra_open_device(&devices[3], "VDDRQ", TEGRA_VDDRQ_DEV);

	while (1) {
		avail = sizeof(buf);
		pos = 0;
		for (i = 0; i < 5; i++) {
			nbytes = tegra_read(&devices[i], buf + pos, avail);
			pos += nbytes;
			avail -= nbytes;
			buf[pos - 1] = ',';
		}
		buf[pos - 1] = '\0';
		puts(buf);
		// some clock skew :(
		usleep(interval * 1000);
	}
}
