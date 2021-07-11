#include "pch.h"

#include "DBSQLLite.h"

namespace TheWorld_MapManager
{
	DBSQLLite::DBSQLLite(DBType dbt) : SQLInterface(dbt)
	{
	}

	DBSQLLite::~DBSQLLite()
	{
	}

	void DBSQLLite::addWD(WorldDefiner& WD, std::vector<addWD_mapVertex>& mapVertex)
	{}
}