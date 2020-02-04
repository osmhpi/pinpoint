#include "osmtegrastats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int tegra_open_device(struct tegra_device_info *tdi, char *name, char *filename)
{
	bzero(tdi, sizeof(*tdi));
	strncpy(tdi->name, name, TEGRA_NAME_LEN);
	strncpy(tdi->filename, filename, TEGRA_FILENAME_LEN);
	tdi->fp = fopen(tdi->filename, "r");
	return tdi->fp != NULL;
}

int tegra_read(struct tegra_device_info *tdi, char *buf, size_t buflen)
{
	size_t pos;
	rewind(tdi->fp);
	pos = fread(buf, sizeof(char), buflen, tdi->fp);
	if (pos > 0)
		buf[pos-1] = '\0';
	return pos;
}

int tegra_read_int(struct tegra_device_info *tdi)
{
	char buf[255];
	tegra_read(tdi, buf, 255);
	return atoi(buf);
}

int tegra_close(struct tegra_device_info *tdi)
{
	fclose(tdi->fp);
	return 1;
}

int main(int argc, char *argv[])
{
	long interval = 500;
	int i;
	char buf[255] = {};
	struct tegra_device_info devices[5];

	if (argc > 1)
		interval = atol(argv[1]);

	tegra_open_device(&devices[0], "CPU", TEGRA_CPU_DEV);
	tegra_open_device(&devices[1], "GPU", TEGRA_GPU_DEV);
	tegra_open_device(&devices[2], "SOC", TEGRA_SOC_DEV);
	tegra_open_device(&devices[3], "DDR", TEGRA_DDR_DEV);
	tegra_open_device(&devices[4], "IN",  TEGRA_IN_DEV );

	while (1) {
		for (i = 0; i < 4; i++) {
			tegra_read(&devices[i], buf, 255);
			printf("%s:%s,", devices[i].name, buf);
		}
		tegra_read(&devices[4], buf, 255);
		printf("%s:%s", devices[4].name, buf);
		puts("");
		usleep(interval * 1000);
	}
}
