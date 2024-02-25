#include "pch.h"
#include "DBFactory.h"

#include "MapManager_Utils.h"
#include "DBJson.h"
#include "DBSQLLite.h"

namespace TheWorld_MapManager
{
	DBInterface* DBFactory::getDB(DBInterface::DBType dbt, std::string dataPath)
	{
		switch (dbt)
		{
			case DBInterface::DBType::Json:
			{
				return new DBJson(dbt, dataPath.c_str());
			}
			break;
			case DBInterface::DBType::SQLLite:
			{
				return new DBSQLLite(dbt, dataPath.c_str());
			}
			break;
			default:
				return nullptr;
		}
	}

	void DBFactory::setConnectionType(enum class DBInterface::ConnectionType type, DBInterface::DBType dbt)
	{
		switch (dbt)
		{
			case DBInterface::DBType::Json:
			{
			}
			break;
			case DBInterface::DBType::SQLLite:
			{
				DBSQLLiteOps::setConnectionType(type);
			}
			break;
		}
	}


}