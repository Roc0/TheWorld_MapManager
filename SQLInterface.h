#pragma once

#include <vector>
#include <string>

#include "WorldDefiner.h"

namespace TheWorld_MapManager
{
#define SQL_OK		0
#define SQL_DUPKEY	1
#define SQL_KO		999

	enum class DBType
	{
		SQLLite = 0
	};
	
	class SQLInterface
	{
	public:
		SQLInterface(DBType dbt, const char* dataPath, bool debugMode = false)
		{
			m_dbt = dbt;
			m_dataPath = dataPath;
			m_debugMode = debugMode;
		}
		~SQLInterface()
		{
		}
		virtual const char* classname() { return "SQLInterface"; }

		std::string dataPath(void) { return m_dataPath; }
		void debugMode(bool b) { m_debugMode = b; };
		bool debugMode(void) { return m_debugMode; }

		struct addWD_mapVertex
		{
			float posX;
			float posZ;
			float radius;
			float azimuth;
			int level;
			bool affected;
		};

		// Pure	virtual functions
		virtual void addWD(WorldDefiner& WD, std::vector<addWD_mapVertex>& mapVertex) = 0;
		virtual void finalizeDB(void) = 0;

	private:
		DBType m_dbt;
		std::string m_dataPath;
		bool m_debugMode;
	};
}
