#pragma once

#include "PowerDataSource.h"

#include <stdexcept>
#include <cstdio>

#define TEGRA_GPU_DEV  "/sys/bus/i2c/devices/0-0040/iio_device/in_power0_input"
#define TEGRA_SOC_DEV  "/sys/bus/i2c/devices/0-0040/iio_device/in_power1_input"
#define TEGRA_WIFI_DEV "/sys/bus/i2c/devices/0-0040/iio_device/in_power2_input"
#define TEGRA_IN_DEV   "/sys/bus/i2c/devices/0-0041/iio_device/in_power0_input"
#define TEGRA_CPU_DEV  "/sys/bus/i2c/devices/0-0041/iio_device/in_power1_input"
#define TEGRA_DDR_DEV  "/sys/bus/i2c/devices/0-0041/iio_device/in_power2_input"

class JetsonCounter: public PowerDataSource
{
public:
	static std::string sourceName()
	{
		return "jetson";
	}

	static std::vector<std::string> detectAvailableCounters();
	static PowerDataSourcePtr openCounter(const std::string & counterName);

	virtual ~JetsonCounter()
	{
		fclose(m_fp);
	}

	virtual int read_string(char *buf, size_t buflen)
	{
		size_t pos;
		rewind(m_fp);
		pos = fread(buf, sizeof(char), buflen, m_fp);
		if (pos > 0)
			buf[pos-1] = '\0';
		return pos;
	}
	
	virtual int read()
	{
		char buf[255];
		read_string(buf, sizeof(buf));
		return atoi(buf);
	}

private:
	std::string m_filename;
	FILE *m_fp;

	JetsonCounter(const std::string & filename) :
		PowerDataSource(),
		m_filename(filename)
	{
		m_fp = fopen(m_filename.c_str(), "r");
		if (m_fp == NULL)
			std::runtime_error("Cannot open " + m_filename);
	}
};
