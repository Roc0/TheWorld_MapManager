#pragma once

#include <string>
#include "DBInterface.h"

namespace TheWorld_MapManager
{
	class DBFactory
	{
	public:
		static DBInterface* getDB(DBInterface::DBType dbt, std::string dataPath);
		static void setConnectionType(enum class DBInterface::ConnectionType type, DBInterface::DBType dbt);
		//{
		//	DBSQLLiteOps::setConnectionType(type);
		//}

	};
}
