#include "PowerDataSource.h"

#include "Settings.h"

#include <deque>
#include <numeric>

struct PowerDataSourceDetail
{
	std::string name;
	std::deque<PowerSample> samples;
};


PowerDataSource::PowerDataSource() :
	m_detail(new PowerDataSourceDetail)
{
	;;
}

PowerDataSource::~PowerDataSource()
{
	delete m_detail;
}

std::string PowerDataSource::name() const
{
	return m_detail->name;
}

void PowerDataSource::setName(const std::string & name)
{
	m_detail->name = name;
}

void PowerDataSource::reset_acc()
{
	m_detail->samples.clear();
}

void PowerDataSource::accumulate()
{
	auto sample = read();
	m_detail->samples.push_back(sample);
}

units::energy::joule_t PowerDataSource::accumulator() const
{
	// FIXME: Use difference of measured sample timestamp
	// we take lower Darboux integral ... :( [since we measure at start of interval]

	auto interval = as_unit_seconds(settings::interval);

	units::energy::joule_t integral(0);
	for (const auto & sample: m_detail->samples) {
		integral += sample.value * interval;
	}
	return integral;
}
