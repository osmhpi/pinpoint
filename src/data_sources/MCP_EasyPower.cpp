#include "MCP_EasyPower.h"

#include "Registry.h"

#include <cstdlib>
#include <fstream>
#include <dirent.h>
#include <memory>
#include <unistd.h>

extern "C" {
	#include "mcp_com.h"
}

#define MCP_USB_VENDOR_ID  0x04d8
#define MCP_USB_PRODUCT_ID 0x00dd

struct MCP_EasyPowerDetail
{
	int fd;
	unsigned int channel;

	MCP_EasyPowerDetail() :
		fd(-1)
	{
		;;
	}
};

/*******************************************************************/

static std::vector<std::string> s_validDeviceNames;

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
			counters.push_back("dev" + std::to_string(s_validDeviceNames.size()) + "ch1");
			s_validDeviceNames.push_back(deviceFile);
		}
	}
	return counters;
}

PowerDataSourcePtr MCP_EasyPower::openCounter(const std::string &counterName)
{
	// FIXME: Yikes
	unsigned int dev, ch;
	sscanf(counterName.c_str(), "dev%uch%u", &dev, &ch);
	if (dev >= s_validDeviceNames.size())
		return nullptr;

	if (ch < 1 || ch > 2)
		return nullptr;

	return PowerDataSourcePtr(new MCP_EasyPower(s_validDeviceNames[dev], ch));
}

void MCP_EasyPower::registerPossibleAliases()
{
	Registry::registerAlias<MCP_EasyPower>("EXT_IN", "dev0ch1");
	Registry::registerAlias<MCP_EasyPower>("MCP1", "dev0ch1");
	Registry::registerAlias<MCP_EasyPower>("MCP2", "dev0ch2");
	Registry::registerAlias<MCP_EasyPower>("MCP3", "dev1ch1");
	Registry::registerAlias<MCP_EasyPower>("MCP4", "dev1ch2");
}

/*******************************************************************/


MCP_EasyPower::MCP_EasyPower(const std::string & filename, const unsigned int channel) :
	PowerDataSource(),
	m_detail(new MCP_EasyPowerDetail())
{
	m_detail->channel = channel;
	if (!(m_detail->fd = f511_init(filename.c_str())))
		throw std::runtime_error("Cannot open " + filename);
}

MCP_EasyPower::~MCP_EasyPower()
{
	if (m_detail->fd >= 0)
		close(m_detail->fd);
	delete m_detail;
}

int MCP_EasyPower::read()
{
	int ch1, ch2;
	f511_get_power(&ch1, &ch2, m_detail->fd);
	return ch1 * 10; // MCP returns data in 10mW steps
}
