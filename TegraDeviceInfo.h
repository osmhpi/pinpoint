#pragma once

#include <stdexcept>
#include <string>
#include <cstdlib>

extern "C" {
	#include "tegra_device_info.h"
}

class TegraDeviceInfo
{
public:
	using accumulate_t = unsigned long;

	TegraDeviceInfo(const std::string & filename, const std::string & name = "UNDEF"):
		m_acc(0)
	{
		if (!tegra_open_device(&m_tdi, name.c_str(), filename.c_str()))
			std::runtime_error("Cannot open " + filename);
	}

	~TegraDeviceInfo()
	{
		tegra_close(&m_tdi);
	}

	const std::string name() const
	{
		return std::string(m_tdi.name);
	}

	int raw_read(char *buf, size_t buflen)
	{
		return tegra_read(&m_tdi, buf, buflen);
	}
	
	int read()
	{
		char buf[255];
		tegra_read(&m_tdi, buf, 255);
		return atoi(buf);
	}

	void reset_acc()
	{
		m_acc = 0;
	}

	void accumulate()
	{
		m_acc += read();
	}

	accumulate_t accumulator() const
	{
		return m_acc;
	}

private:
	struct tegra_device_info m_tdi;
	accumulate_t m_acc;

};
