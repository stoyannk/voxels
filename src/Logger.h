// Copyright (c) 2013-2016, Stoyan Nikolov
// All rights reserved.
// Voxels Library, please see LICENSE for licensing details.
#pragma once

#include "../include/Library.h"

#define VOXLOG(SEVERITY, MSG) Voxels::Logger::Get()->Log(Voxels::SEVERITY, MSG)

namespace Voxels
{

class Logger
{
public:
	static void Create(LogMessage log)
	{
		s_Logger = new Logger(log);
	}

	static void Destroy()
	{
		delete s_Logger;
		s_Logger = nullptr;
	}

	static Logger* Get()
	{
		return s_Logger;
	}

	void Log(LogSeverity severity, const char* msg)
	{
		if (m_ExtLogger)
		{
			m_ExtLogger(severity, msg);
		}
	}

private:
	Logger(LogMessage log)
		: m_ExtLogger(log)
	{}

	static Logger* s_Logger;

	LogMessage m_ExtLogger;
};

}