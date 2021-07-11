#pragma once

#include <vector>

#include "WorldDefiner.h"

namespace TheWorld_MapManager
{
	enum class DBType
	{
		SQLLite = 0
	};
	
	class SQLInterface
	{
	public:
		SQLInterface(DBType dbt);
		~SQLInterface();

		struct addWD_mapVertex
		{
			float posX;
			float posZ;
			float radius;
			float azimuth;
			bool affected;
		};

		virtual void addWD(WorldDefiner& WD, std::vector<addWD_mapVertex>& mapVertex) = 0;

	private:
		DBType m_dbt;
	};
}
