#pragma once

#include "SQLInterface.h"

namespace TheWorld_MapManager
{
	class DBSQLLite : public SQLInterface
	{
	public:
		_declspec(dllexport) DBSQLLite(DBType dbt = DBType::SQLLite);
		_declspec(dllexport) ~DBSQLLite();

		_declspec(dllexport) void addWD(WorldDefiner& WD, std::vector<addWD_mapVertex>& mapVertex);

	private:

	};
}
