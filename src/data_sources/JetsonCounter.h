#pragma once

#include "PowerDataSource.h"

#include <stdexcept>
#include <cstdio>

class JetsonCounter: public PowerDataSource
{
public:
	static std::string sourceName()
	{
		return "jetson";
	}

	static std::vector<std::string> detectAvailableCounters();
	static PowerDataSourcePtr openCounter(const std::string & counterName);
	static void registerPossibleAliases();

	virtual ~JetsonCounter()
	{
		fclose(m_fp);
	}

	virtual int read_mW_string(char *buf, size_t buflen)
	{
		size_t pos;
		rewind(m_fp);
		pos = fread(buf, sizeof(char), buflen, m_fp);
		if (pos > 0)
			buf[pos-1] = '\0';
		return pos;
	}
	
	virtual units::power::watt_t read()
	{
		char buf[255];
		read_mW_string(buf, sizeof(buf));
		return units::power::milliwatt_t(atoi(buf));
	}

private:
	std::string m_filename;
	FILE *m_fp;

	JetsonCounter(const std::string & filename) :
		PowerDataSource(),
		m_filename(filename)
	{
		m_fp = fopen(m_filename.c_str(), "r");
		if (m_fp == NULL)
			throw std::runtime_error("Cannot open " + m_filename);
	}
};
