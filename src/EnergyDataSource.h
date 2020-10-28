#pragma once

#include "PowerDataSource.h"

struct EnergyDataSourceDetail;

class EnergyDataSource : public PowerDataSource
{
public:
    EnergyDataSource();
    virtual ~EnergyDataSource();

    virtual EnergySample read_energy() = 0;

    // Implements PowerDataSource's read by deriving read_energy()
    virtual PowerSample read() override;
    virtual void accumulate() override;
    virtual units::energy::joule_t accumulator() const override;

protected:
    /* Call this in a subclass-constructor to call read once
     * and have non-zero first power sample in continous print mode
     * (might be removed in the future) */
    void initial_read()
    { read(); }

private:
    EnergyDataSourceDetail *m_detail;

};

