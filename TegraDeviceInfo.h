#pragma once

#include <stdexcept>
#include <string>
#include <cstdlib>

extern "C" {
	#include "tegra_device_info.h"
}

struct TegraDeviceInfo
{
	TegraDeviceInfo(const std::string & filename, const std::string & name = "UNDEF")
	{
		if (!tegra_open_device(&m_tdi, name.c_str(), filename.c_str()))
			std::runtime_error("Cannot open " + filename);
	}

	~TegraDeviceInfo()
	{
		tegra_close(&m_tdi);
	}

	int read()
	{
		char buf[255];
		tegra_read(&m_tdi, buf, 255);
		return atoi(buf);
	}

private:
	struct tegra_device_info m_tdi;

};
