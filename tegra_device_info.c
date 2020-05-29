#include "tegra_device_info.h"

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

