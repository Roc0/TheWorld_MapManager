#include "pch.h"

#include "SQLInterface.h"

namespace TheWorld_MapManager
{
	SQLInterface::SQLInterface(DBType dbt)
	{
		m_dbt = dbt;
	}

	SQLInterface::~SQLInterface()
	{
	}
}