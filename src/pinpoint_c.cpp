#include "EnergyDataSource.h"
#include "PowerDataSource.h"
#include "Registry.h"

#include <cstring>
#include <memory>

extern "C" {

#include "pinpoint_c.h"

}

template<typename ElementT, typename ToStdString>
char **build_string_list(const std::vector<ElementT> & elements, ToStdString toStdStr)
{
	char **result = (char **)calloc(elements.size() + 1, sizeof(char *));
	for (size_t i = 0; i < elements.size(); i++) {
		const std::string & s = toStdStr(elements[i]);
		result[i] = (char *) calloc(s.size() + 1, sizeof(char));
		strncpy(result[i], s.c_str(), s.size());
	}
	return result;
}

static const std::string & unpack_str (const std::string & s) { return s; }
static const std::string & unpack_pair (const std::pair<std::string,std::string> & p) { return p.first; }

extern "C" {

int pinpoint_setup(void)
{
	Registry::setup();
	return 0;
}

char **pinpoint_available_counters(void)
{
	return build_string_list<>(Registry::availableCounters(), unpack_str);
}

char **pinpoint_available_aliases(void)
{
	return build_string_list<>(Registry::availableAliases(), unpack_pair);
}

pinpoint_source_t pinpoint_open_source(const char *name)
{
	try {
		 PowerDataSourcePtr src = Registry::openCounter(std::string(name));
		 if (!src) {
			return nullptr;
		 }

		 PowerDataSourcePtr *handle = new PowerDataSourcePtr(src);
		 return static_cast<pinpoint_source_t>(handle);
	} catch (...) {
		return nullptr;
	}
}

void pinpoint_close_source(pinpoint_source_t src)
{
	if (!src) {
		return;
	}

	try {
		PowerDataSourcePtr *handle = static_cast<PowerDataSourcePtr*>(src);
		delete handle;
	} catch (...) {
		;;
	}
}

size_t pinpoint_source_name(pinpoint_source_t src, char *dst_buf, size_t buflen)
{
	if (!src) {
		return 0;
	}

	try {
		PowerDataSourcePtr *handle = static_cast<PowerDataSourcePtr*>(src);
		std::string name = (*handle)->name();
		size_t copy_size = std::min(buflen, name.size());
		strncpy(dst_buf, name.c_str(), copy_size);
		return copy_size;
	} catch (...) {
		return 0;
	}
}

void pinpoint_read_energy(pinpoint_source_t src, pinpoint_sample_t *dst)
{
	if (!src || !dst) {
		return;
	}

	//try {
		PowerDataSourcePtr *handle = static_cast<PowerDataSourcePtr*>(src);
		std::shared_ptr<EnergyDataSource> e_handle = std::dynamic_pointer_cast<EnergyDataSource>(*handle);
		const EnergySample sample = (*e_handle).read_energy();
		dst->value = sample.in_base_unit();
		sample.save_timespec(&dst->timestamp);
	//} catch (...) {
		return;
//	}
}

void pinpoint_read_power(pinpoint_source_t src, pinpoint_sample_t *dst)
{
	if (!src || !dst) {
		return;
	}

	try {
		PowerDataSourcePtr *handle = static_cast<PowerDataSourcePtr*>(src);
		const PowerSample sample = (*handle)->read();
		dst->value = sample.in_base_unit();
		sample.save_timespec(&dst->timestamp);
	} catch (...) {
		return;
	}
}

void pinpoint_reset_acc(pinpoint_source_t src)
{
	try {
		PowerDataSourcePtr *handle = static_cast<PowerDataSourcePtr*>(src);
		(*handle)->reset_acc();
	} catch (...) {
		;;
	}
}

} // extern "C"
