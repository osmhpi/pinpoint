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
	units::energy::joule_t integral(0);

	if (!m_detail->samples.empty()) {
		// we take lower Darboux integral ...[since we measure at start of interval]
		for (size_t i = 0; i < m_detail->samples.size() - 1; i++) {
			auto time_diff = as_unit_seconds(m_detail->samples[i+1].timestamp - m_detail->samples[i].timestamp);
			integral += m_detail->samples[i].value * time_diff;
		}

		// For the last sample we assume the sleeping interval of configured length finished
		integral += m_detail->samples.back().value * as_unit_seconds(settings::interval);
	}

	return integral;
}
