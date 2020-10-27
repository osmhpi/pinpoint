#pragma once

struct ExperimentDetail;

class Experiment
{
public:
	Experiment();
	virtual ~Experiment();

	void run();
	void printResult();

private:
	ExperimentDetail *m_detail;

	void run_single();
};
