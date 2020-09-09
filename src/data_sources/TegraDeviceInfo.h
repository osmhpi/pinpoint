#pragma once

#include "PowerDataSource.h"

#include <stdexcept>
#include <cstdlib>

extern "C" {
	#include "tegra_device_info.h"
}

class TegraDeviceInfo: public PowerDataSource
{
public:
	TegraDeviceInfo(const std::string & filename, const std::string & name = "UNDEF"):
		PowerDataSource()
	{
		if (!tegra_open_device(&m_tdi, name.c_str(), filename.c_str()))
			std::runtime_error("Cannot open " + filename);
	}

	virtual ~TegraDeviceInfo()
	{
		tegra_close(&m_tdi);
	}

	virtual const std::string name() const
	{
		return std::string(m_tdi.name);
	}

	virtual int read_string(char *buf, size_t buflen)
	{
		return tegra_read(&m_tdi, buf, buflen);
	}
	
	virtual int read()
	{
		char buf[255];
		tegra_read(&m_tdi, buf, 255);
		return atoi(buf);
	}

private:
	struct tegra_device_info m_tdi;
};
