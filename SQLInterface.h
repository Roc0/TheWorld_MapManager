#pragma once

#define _USE_MATH_DEFINES // for C++

#include <vector>
#include <string>
#include <math.h>

#include "WorldDefiner.h"
#include "Utils.h"

namespace TheWorld_MapManager
{
#define SQL_OK		0
#define SQL_DUPKEY	1
#define SQL_KO		999

	class SQLInterface
	{
	public:
		enum class DBType
		{
			SQLLite = 0
		};

		enum class QuadrantStatus
		{
			NotSet = -1,
			Empty = 0,
			Loading = 1,
			Complete = 2
		};

		SQLInterface(DBType dbt, const char* dataPath, bool consoleDebugMode = false)
		{
			m_dbt = dbt;
			m_dataPath = dataPath;
			m_consoleDebugMode = consoleDebugMode;
		}
		~SQLInterface()
		{
		}
		virtual const char* classname()
		{
			return "SQLInterface"; 
		}

		std::string dataPath(void) 
		{
			return m_dataPath; 
		}
		void consoleDebugMode(bool b) 
		{
			m_consoleDebugMode = b; 
		};
		bool consoleDebugMode(void)
		{
			return m_consoleDebugMode; 
		}

		class GridVertex
		{
		public:
			enum class latlong_type
			{
				degrees = 1,
				minutes = 2,
				seconds = 3
			};

			GridVertex()
			{
				m_posX = 0.0;
				m_altitude = 0.0;
				m_posZ = 0.0;
				m_radius = 0.0;
				m_azimuth = 0.0;
				m_level = 0;
				m_initialAltitude = 0.0;
				m_rowid = -1;
				m_normX = 0;
				m_normY = 0;
				m_normZ = 0;
				m_colorR = -1;
				m_colorG = -1;
				m_colorB = -1;
				m_colorA = -1;
				m_lowElevationTexAmount = -1;
				m_highElevationTexAmount = -1;
				m_dirtTexAmount = -1;
				m_rocksTexAmount = -1;
				m_globalMapR = -1;
				m_globalMapG = -1;
				m_globalMapB = -1;
			}
			GridVertex(float posX, float posZ, int level = 0)
			{
				initGridVertex(posX, posZ, 0.0, level);
			}

			GridVertex(float posX, float posZ, float initialAltitude, int level = 0)
			{
				initGridVertex(posX, posZ, initialAltitude, level);
			}

			~GridVertex() {}

			void setAltitude(float altitude) 
			{
				m_altitude = altitude; 
			};

			void initGridVertex(float posX, float posZ, float initialAltitude, int level = 0)
			{
				m_posX = posX;
				m_altitude = 0.0;		// Altitude will be calculated later
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
				m_normX = 0;
				m_normY = 0;
				m_normZ = 0;
				m_colorR = -1;
				m_colorG = -1;
				m_colorB = -1;
				m_colorA = -1;
				m_lowElevationTexAmount = -1;
				m_highElevationTexAmount = -1;
				m_dirtTexAmount = -1;
				m_rocksTexAmount = -1;
				m_globalMapR = -1;
				m_globalMapG = -1;
				m_globalMapB = -1;
			}
			
			// needed to use an istance of GridVertex as a key in a map (to keep the map sorted by m_posZ and by m_posX for equal m_posZ)
			// first row, second row, ... etc
			bool operator<(const GridVertex& p) const
			{
				if (m_posZ < p.m_posZ)
					return true;
				if (m_posZ > p.m_posZ)
					return false;
				else
					return m_posX < p.m_posX;
			}

			bool operator==(const GridVertex& p) const
			{
				if (m_posZ == p.m_posZ && m_posX == p.m_posX && m_level == p.m_level)
					return true;
				else
					return false;
			}

			void setInternalValues(float posX, float altitude, float posZ, float radius, float azimuth, int level, float initialAltitude, __int64 rowid,
				float normX = 0, float normY = 0, float normZ = 0, int colorR = -1, int colorG = -1, int colorB = -1, int colorA = -1,
				int lowElevationTexAmount = -1, int highElevationTexAmount = -1, int dirtTexAmount = -1, int rocksTexAmount = -1,
				int globalMapR = -1, int globalMapG = -1, int globalMapB = -1)
			{
				m_posX = posX;
				m_altitude = altitude;
				m_posZ = posZ;
				m_radius = radius;
				m_azimuth = azimuth;
				m_level = level;
				m_initialAltitude = initialAltitude;
				m_rowid = rowid;
				m_normX = normX;
				m_normY = normY;
				m_normZ = normZ;
				m_colorR = colorR;
				m_colorG = colorG;
				m_colorB = colorB;
				m_colorA = colorA;
				m_lowElevationTexAmount = lowElevationTexAmount;
				m_highElevationTexAmount = highElevationTexAmount;
				m_dirtTexAmount = dirtTexAmount;
				m_rocksTexAmount = rocksTexAmount;
				m_globalMapR = globalMapR;
				m_globalMapG = globalMapG;
				m_globalMapB = globalMapB;
			}

			float posX(void) const
			{
				return m_posX; 
			}
			float altitude(void) const
			{
				return m_altitude; 
			}
			float posZ(void) const
			{
				return m_posZ; 
			}
			float radius(void) const
			{
				return m_radius; 
			}
			float azimuth(void) const
			{
				return m_azimuth; 
			}
			int level(void) const
			{
				return m_level; 
			}
			float initialAltitude(void) const
			{
				return m_initialAltitude; 
			}
			__int64 rowid(void) const 
			{
				return m_rowid; 
			}
			float normX(void) const
			{
				return m_normX;
			}
			float normY(void) const
			{
				return m_normY;
			}
			float normZ(void) const
			{
				return m_normZ;
			}
			int colorR(void) const
			{
				return m_colorR;
			}
			int colorG(void) const
			{
				return m_colorG;
			}
			int colorB(void) const
			{
				return m_colorB;
			}
			int colorA(void) const
			{
				return m_colorA;
			}
			int lowElevationTexAmount(void) const
			{
				return m_lowElevationTexAmount;
			}
			int highElevationTexAmount(void) const
			{
				return m_highElevationTexAmount;
			}
			int dirtTexAmount(void) const
			{
				return m_dirtTexAmount;
			}
			int rocksTexAmount(void) const
			{
				return m_rocksTexAmount;
			}
			int globalMapR(void) const
			{
				return m_globalMapR;
			}
			int globalMapG(void) const
			{
				return m_globalMapG;
			}
			int globalMapB(void) const
			{
				return m_globalMapB;
			}

		private:
			float m_posX;
			float m_altitude;
			float m_posZ;
			float m_radius;
			float m_azimuth;
			int m_level;
			float m_initialAltitude;
			__int64 m_rowid;
			float m_normX;
			float m_normY;
			float m_normZ;
			int m_colorR;					// 0/255
			int m_colorG;					// 0/255
			int m_colorB;					// 0/255
			int m_colorA;					// 0/255
			int m_lowElevationTexAmount;	// 0/255
			int m_highElevationTexAmount;	// 0/255
			int m_dirtTexAmount;			// 0/255
			int m_rocksTexAmount;			// 0/255
			int m_globalMapR;				// 0/255
			int m_globalMapG;				// 0/255
			int m_globalMapB;				// 0/255
		};

		// Pure	virtual functions
		virtual std::string readParam(std::string paranName) = 0;
		virtual void beginTransaction(void) = 0;
		virtual void endTransaction(bool commit = true) = 0;
		virtual __int64 addWDAndVertices(WorldDefiner* pWD, std::vector<GridVertex>& vectGridVertices) = 0;
		virtual bool eraseWD(__int64 wdRowid) = 0;
		virtual void updateAltitudeOfVertex(__int64 vertexRowid, float altitude) = 0;
		virtual void clearVerticesMarkedForUpdate(void) = 0;
		virtual void getVertex(__int64 vertexRowid, GridVertex& gridVertex, int level) = 0;
		virtual void getVertex(GridVertex& gridVertex) = 0;
		virtual void getVertices(float minX, float maxX, float minZ, float maxZ, std::vector<GridVertex>& vectGridVertices, int level) = 0;
		virtual size_t eraseVertices(float minX, float maxX, float minZ, float maxZ, int level) = 0;
		virtual bool getWD(float posX, float posZ, int level, WDType type, WorldDefiner& WD) = 0;
		virtual bool getWD(__int64 wdRowid, WorldDefiner& WD) = 0;
		virtual void getWDRowIdForVertex(__int64 vertexRowid, std::vector<__int64>& vectWDRowId) = 0;
		virtual bool getFirstModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD) = 0;
		virtual bool getNextModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD) = 0;
		virtual std::string getQuadrantHash(float gridStep, size_t vertxePerSize, size_t level, float posX, float posZ, enum class SQLInterface::QuadrantStatus& status) = 0;
		virtual bool writeQuadrantToDB(TheWorld_Utils::MeshCacheBuffer& cache, TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData& cacheQuadrantData, bool& stop) = 0;
		virtual void readQuadrantFromDB(TheWorld_Utils::MeshCacheBuffer& cache, std::string& meshId, enum class SQLInterface::QuadrantStatus& status, TheWorld_Utils::TerrainEdit& terrainEdit) = 0;
		virtual void finalizeDB(void) = 0;

	private:
		DBType m_dbt;
		std::string m_dataPath;
		bool m_consoleDebugMode;
	};
}
