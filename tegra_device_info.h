#pragma once

#include <stdio.h>

#define TEGRA_GPU_DEV    "/sys/bus/i2c/devices/1-0040/iio_device/in_power0_input"
#define TEGRA_CPU_DEV    "/sys/bus/i2c/devices/1-0040/iio_device/in_power1_input"
#define TEGRA_SOC_DEV    "/sys/bus/i2c/devices/1-0040/iio_device/in_power2_input"
#define TEGRA_CV_DEV     "/sys/bus/i2c/devices/1-0041/iio_device/in_power0_input"
#define TEGRA_VDDRQ_DEV  "/sys/bus/i2c/devices/1-0041/iio_device/in_power1_input"
#define TEGRA_SYS5V_DEV  "/sys/bus/i2c/devices/1-0041/iio_device/in_power2_input"

#define TEGRA_NAME_LEN      5
#define TEGRA_FILENAME_LEN 255

struct tegra_device_info
{
	char name[TEGRA_NAME_LEN];
	char filename[TEGRA_FILENAME_LEN];
	FILE *fp;
};

extern int tegra_open_device(struct tegra_device_info *tdi, const char *name, const char *filename);
extern int tegra_read(struct tegra_device_info *tdi, char *buf, size_t buflen);
extern int tegra_read_int(struct tegra_device_info *tdi);
extern int tegra_close(struct tegra_device_info *tdi);
