#pragma once

extern "C" {
	#include "tegra_device_info.h"
}

struct TegraDeviceInfo
{

private:
	struct tegra_device_info m_tdi;

};
