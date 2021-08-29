#pragma once

#define _USE_MATH_DEFINES // for C++

#include <vector>
#include <string>
#include <math.h>

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

		class MapVertex
		{
		public:
			MapVertex()
			{
				m_posX = 0.0;
				m_posY = 0.0;
				m_posZ = 0.0;
				m_radius = 0.0;
				m_azimuth = 0.0;
				m_level = 0;
				m_initialAltitude = 0.0;
				m_rowid = -1;
			}
			MapVertex(float posX, float posZ, int level = 0)
			{
				m_posX = posX;
				m_posY = 0.0;
				m_posZ = posZ;
				m_level = level;
				m_initialAltitude = 0.0;
				m_radius = sqrtf(powf(m_posX, 2.0) + powf(m_posZ, 2.0));
				if ((m_posX == 0 && m_posZ == 0) || m_radius == 0)
					m_azimuth = 0;
				else
				{
					//angle of radius with x-axus (complementar of 2PI if Z < 0)
					m_azimuth = acosf(m_posX / m_radius);
					if (m_posZ < 0)
						m_azimuth = (float)(M_PI * 2.0) - m_azimuth;
				}
				m_rowid = -1;
			}

			MapVertex(float posX, float posZ, float initialAltitude, int level = 0)
			{
				m_posX = posX;
				m_posY = 0.0;
				m_posZ = posZ;
				m_level = level;
				m_initialAltitude = initialAltitude;
				m_radius = sqrtf(powf(m_posX, 2.0) + powf(m_posZ, 2.0));
				if ((m_posX == 0 && m_posZ == 0) || m_radius == 0)
					m_azimuth = 0;
				else
				{
					//angle of radius with x-axus (complementar of 2PI if Z < 0)
					m_azimuth = acosf(m_posX / m_radius);
					if (m_posZ < 0)
						m_azimuth = (float)(M_PI * 2.0) - m_azimuth;
				}
				m_rowid = -1;
			}

			~MapVertex() {}

			void setInternalValues(float posX, float posY, float posZ, float radius, float azimuth, int level, float initialAltitude, __int64 rowid)
			{
				m_posX = posX;
				m_posY = posY;
				m_posZ = posZ;
				m_radius = radius;
				m_azimuth = azimuth;
				m_level = level;
				m_initialAltitude = initialAltitude;
				m_rowid = rowid;
			}
			
			float posX(void) { return m_posX; };
			float posY(void) { return m_posY; };
			float posZ(void) { return m_posZ; };
			float radius(void) { return m_radius; };
			float azimuth(void) { return m_azimuth; };
			int level(void) { return m_level; };
			float initialAltitude(void) { return m_initialAltitude; };
			__int64 rowid(void) { return m_rowid; };

		private:
			float m_posX;
			float m_posY;
			float m_posZ;
			float m_radius;
			float m_azimuth;
			int m_level;
			float m_initialAltitude;
			__int64 m_rowid;
		};

		// Pure	virtual functions
		virtual void beginTransaction(void) = 0;
		virtual void endTransaction(bool commit = true) = 0;
		virtual __int64 addWDAndVertices(WorldDefiner* pWD, std::vector<MapVertex>& mapVertices) = 0;
		virtual bool eraseWD(__int64 wdRowid) = 0;
		virtual void updateAltitudeOfVertex(__int64 vertexRowid, float posY) = 0;
		virtual void clearVerticesMarkedForUpdate(void) = 0;
		virtual void getVertex(__int64 vertexRowid, MapVertex& mapVertex) = 0;
		virtual bool getWD(float posX, float posZ, int level, WDType type, WorldDefiner& WD) = 0;
		virtual bool getWD(__int64 wdRowid, WorldDefiner& WD) = 0;
		virtual void getWDRowIdForVertex(__int64 vertexRowid, std::vector<__int64>& MapWDRowId) = 0;
		virtual bool getFirstModfiedVertex(MapVertex& mapVertex, std::vector<WorldDefiner>& wdMap) = 0;
		virtual bool getNextModfiedVertex(MapVertex& mapVertex, std::vector<WorldDefiner>& wdMap) = 0;
		virtual void finalizeDB(void) = 0;

	private:
		DBType m_dbt;
		std::string m_dataPath;
		bool m_debugMode;
	};
}
