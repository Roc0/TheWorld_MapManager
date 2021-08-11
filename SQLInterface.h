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

		struct mapVertex
		{
			float posX;
			float posZ;
			float radius;
			float azimuth;
			int level;
			float posY;
			bool affected;
			__int64 rowid;
		};

		// Pure	virtual functions
		virtual void beginTransaction(void) = 0;
		virtual void endTransaction(bool commit = true) = 0;
		virtual bool addWD(WorldDefiner& WD, std::vector<mapVertex>& mapVertices) = 0;
		virtual void updateAltitudeOfVertex(__int64 vertexRowid, float posY) = 0;
		virtual void eraseModifiedVertices(void) = 0;
		virtual void getVertex(__int64 vertexRowid, mapVertex& mapVertex) = 0;
		virtual void getWD(__int64 wdRowid, WorldDefiner& WD) = 0;
		virtual void getWDRowIdForVertex(__int64 vertexRowid, std::vector<__int64>& MapWDRowId) = 0;
		virtual bool getFirstModfiedVertex(mapVertex& mapVertex, std::vector<WorldDefiner>& wdMap) = 0;
		virtual bool getNextModfiedVertex(mapVertex& mapVertex, std::vector<WorldDefiner>& wdMap) = 0;
		virtual void finalizeDB(void) = 0;

	private:
		DBType m_dbt;
		std::string m_dataPath;
		bool m_debugMode;
	};
}
