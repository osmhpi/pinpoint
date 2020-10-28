#include "EnergyDataSource.h"

#include <atomic>
#include <deque>

struct EnergyDataSourceDetail
{
	std::atomic<bool> has_read;
	EnergySample previous, current;

	EnergyDataSourceDetail() :
		has_read(false)
	{
		;;
	}
};

EnergyDataSource::EnergyDataSource() :
	PowerDataSource(),
	m_detail(new EnergyDataSourceDetail)
{
	;;
}

EnergyDataSource::~EnergyDataSource()
{
	delete m_detail;
}

PowerSample EnergyDataSource::read()
{
	// This function is only called in continuous printing mode
	m_detail->previous = m_detail->current;
	m_detail->current = read_energy();

	if (m_detail->has_read.exchange(true) == false) {
		return units::power::watt_t(0.0);
	}

	auto energydiff = m_detail->current.value - m_detail->previous.value;
	auto timediff = as_unit_seconds(m_detail->current.timestamp - m_detail->previous.timestamp);

	return PowerSample(m_detail->current.timestamp, energydiff / timediff);
}

void EnergyDataSource::accumulate()
{
	m_detail->current = read_energy();
}

units::energy::joule_t EnergyDataSource::accumulator() const
{
	return m_detail->current.value;
}
