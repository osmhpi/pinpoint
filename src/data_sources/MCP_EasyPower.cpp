#include "MCP_EasyPower.h"
#include "Registry.h"

#include <array>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <unistd.h>

extern "C" {
	#include "mcp_com.h"
}

#define MCP_USB_VENDOR_ID  0x04d8
#define MCP_USB_PRODUCT_ID 0x00dd

/*******************************************************************/

struct OpenMCPDevice
{
	int fd;
	std::array<int, 2> data;
	std::array<bool, 2> has_read;

	OpenMCPDevice(const std::string & filename) :
		fd(-1),
		data{0, 0},
		has_read{true, true}
	{
		if (!(fd = f511_init(filename.c_str())))
			throw std::runtime_error("Cannot open " + filename);
	}

	int read(const unsigned int channel)
	{
		// FIXME: Possible data race (though atm not called from different threads)
		if (has_read[channel]) {
			if (0 != f511_get_power(&data[0], &data[1], fd))
				throw std::runtime_error("Cannot get power from MCP.");
			has_read[0] = false;
			has_read[1] = false;
		}
		has_read[channel] = true;
		return data[channel];
	}

	~OpenMCPDevice() {
		if (fd >= 0)
			close(fd);
	}
};

struct MCP_EasyPowerDetail
{
	std::shared_ptr<OpenMCPDevice> device;
	unsigned int channel;
};

/*******************************************************************/

struct MCP_DeviceInfo
{
	std::string filename;
	std::unique_ptr<std::mutex> mutexp;
	std::weak_ptr<OpenMCPDevice> device;

	MCP_DeviceInfo(const std::string & device_filename) :
		filename(device_filename),
		mutexp(new std::mutex)
	{
		;;
	}
};

static std::vector<MCP_DeviceInfo> s_validDevices;

static std::vector<std::string> detect_serial_devices()
{
	std::vector<std::string> deviceFiles;
#ifdef linux
	// Looking for cdc_acm serial files (registered as /dev/ttyACM*)
	const std::string filePrefix = "ttyACM";

	struct dirent *entry = nullptr;
	std::shared_ptr<DIR> dir(opendir("/dev"), [](DIR *dir) {if (dir) closedir(dir);});
	if (!dir)
		return deviceFiles;

	while ((entry = readdir(dir.get()))) {
		const std::string name(entry->d_name);
		if (name.substr(0, filePrefix.size()) == filePrefix)
			deviceFiles.push_back("/dev/" + name);
	}
#else
	;;
#endif
	return deviceFiles;
}

static bool isAccessibleMCPFile(const std::string & deviceFile)
{
	{
		std::ifstream f(deviceFile);
		if (!f) return false; // Fails if not accessible (e.g. wrong permissions)
	}

	// TODO: Inspect udevadm for vendor and product id
	return true;
}

std::vector<std::string> MCP_EasyPower::detectAvailableCounters()
{
	std::vector<std::string> counters;

	for (const auto & deviceFile: detect_serial_devices()) {
		if (isAccessibleMCPFile(deviceFile)) {
			auto devNum = s_validDevices.size();
			s_validDevices.push_back(deviceFile);
			counters.push_back("dev" + std::to_string(devNum) + "ch1");
			counters.push_back("dev" + std::to_string(devNum) + "ch2");
		}
	}
	return counters;
}

PowerDataSourcePtr MCP_EasyPower::openCounter(const std::string &counterName)
{
	// FIXME: Yikes
	unsigned int dev, ch;
	sscanf(counterName.c_str(), "dev%uch%u", &dev, &ch);
	if (dev >= s_validDevices.size())
		return nullptr;

	if (ch < 1 || ch > 2)
		return nullptr;

	return PowerDataSourcePtr(new MCP_EasyPower(dev, ch));
}

Aliases MCP_EasyPower::possibleAliases()
{
	return {
		{"EXT_IN", "dev0ch1"},
		{"MCP1", "dev0ch1"},
		{"MCP2", "dev0ch2"},
	};
}

/*******************************************************************/


MCP_EasyPower::MCP_EasyPower(const unsigned int dev, const unsigned int channel) :
	PowerDataSource(),
	m_detail(new MCP_EasyPowerDetail())
{
	m_detail->channel = channel;

	{
		std::lock_guard<std::mutex> lk(*s_validDevices[dev].mutexp.get());
		m_detail->device = s_validDevices[dev].device.lock();

		if (!m_detail->device) {
			m_detail->device = std::make_shared<OpenMCPDevice>(s_validDevices[dev].filename);
			s_validDevices[dev].device = m_detail->device;
		}
	}
}

MCP_EasyPower::~MCP_EasyPower()
{
	delete m_detail;
}

PowerSample MCP_EasyPower::read()
{
	// MCP returns data in 10mW steps
	return PowerSample(units::power::centiwatt_t(m_detail->device->read(m_detail->channel - 1)));
}

PINPOINT_REGISTER_DATA_SOURCE(MCP_EasyPower)
