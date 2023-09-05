#pragma once

#define _USE_MATH_DEFINES // for C++

#include "WorldDefiner.h"
#include "SQLInterface.h"
#include "MapManagerException.h"
#include "MapManager_Utils.h"
#include <plog/Log.h>
#include <..\TheWorld_GDN_Viewer\TheWorld_GDN_Viewer\Viewer_Utils.h>
#include "Utils.h"
#include <map>
#include <queue>

//#include "Eigen\Dense"
//using namespace Eigen;

namespace TheWorld_MapManager
{
	extern float g_gridStepInWU;

	// ************************************************************************************
	// I M P O R T A N T: the metrics in this class is always expressed in World Units (WU)
	// ************************************************************************************

	class MapManager
	{
	public:
		class QuadrantPos
		{
		public:
			QuadrantPos()
			{
				m_lowerXGridVertex = 0.0f;
				m_lowerZGridVertex = 0.0f;
				m_numVerticesPerSize = 0;
				m_level = 0;
				m_gridStepInWU = 0.0f;
				m_initialized = false;
			}

			QuadrantPos(float lowerXGridVertex, float lowerZGridVertex, size_t numVerticesPerSize, int level, float gridStepInWU)
			{
				m_lowerXGridVertex = lowerXGridVertex;
				m_lowerZGridVertex = lowerZGridVertex;
				m_numVerticesPerSize = numVerticesPerSize;
				m_level = level;
				m_gridStepInWU = gridStepInWU;
				m_initialized = true;
			}
			// needed to use an istance of QuadrantPos as a key in a map (to keep the map sorted by QuadrantPos)
			bool operator<(const QuadrantPos& pos) const
			{
				if (m_gridStepInWU == pos.m_gridStepInWU)
				{
					if (m_numVerticesPerSize == pos.m_numVerticesPerSize)
					{
						if (m_level == pos.m_level)
						{
							if (m_lowerZGridVertex < pos.m_lowerZGridVertex)
								return true;
							if (m_lowerZGridVertex > pos.m_lowerZGridVertex)
								return false;
							else
								return m_lowerXGridVertex < pos.m_lowerXGridVertex;
						}
						else
							if (m_level < pos.m_level)
								return true;
							else
								return false;
					}
					else
						if (m_numVerticesPerSize < pos.m_numVerticesPerSize)
							return true;
						else
							return false;
				}
				else
					if (m_gridStepInWU < pos.m_gridStepInWU)
						return true;
					else
						return false;
			}
			bool operator==(const QuadrantPos& p) const
			{
				if (m_initialized == p.m_initialized && m_lowerXGridVertex == p.m_lowerXGridVertex && m_lowerZGridVertex == p.m_lowerZGridVertex && m_numVerticesPerSize == p.m_numVerticesPerSize && m_level == p.m_level && m_gridStepInWU == p.m_gridStepInWU)
					return true;
				else
					return false;
			}
			QuadrantPos operator=(const QuadrantPos& pos)
			{
				m_lowerXGridVertex = pos.m_lowerXGridVertex;
				m_lowerZGridVertex = pos.m_lowerZGridVertex;
				m_numVerticesPerSize = pos.m_numVerticesPerSize;
				m_level = pos.m_level;
				m_gridStepInWU = pos.m_gridStepInWU;
				m_initialized = pos.m_initialized;
				return *this;
			}

			bool initialized(void)
			{
				return m_initialized;
			}

		public:
			float m_lowerXGridVertex = 0.0f;
			float m_lowerZGridVertex = 0.0f;
			size_t m_numVerticesPerSize = 0;
			int m_level = 0;
			float m_gridStepInWU = 0.0f;
		
		private:
			bool m_initialized;
		};
		
		// Grid / GridPoint are in WUs
		class FlatGridPoint
		{
		public:
			FlatGridPoint(void) : x(0.0f), z(0.0f) {}
			FlatGridPoint(float _x, float _z)
			{
				this->x = _x;	this->z = _z; 
			}
			// needed to use an istance of gridPoint as a key in a map (to keep the map sorted by z and by x for equal z)
			// first row, second row, ... etc
			bool operator<(const FlatGridPoint& p) const
			{
				if (z < p.z)
					return true;
				if (z > p.z)
					return false;
				else
					return x < p.x;
			}
			float x;
			float z;
		};

		class GridPatch
		{
		public:
			GridPatch(SQLInterface::GridVertex& upperLeftGridVertex, SQLInterface::GridVertex& upperRightGridVertex, SQLInterface::GridVertex& lowerLeftGridVertex, SQLInterface::GridVertex& lowerRightGridVertex)
			{
				if (g_gridStepInWU == 0.0f)
					throw(MapManagerException(__FUNCTION__, string("Initialize DB!").c_str()));

				gridVertex.push_back(upperLeftGridVertex);
				gridVertex.push_back(upperRightGridVertex);
				gridVertex.push_back(lowerLeftGridVertex);
				gridVertex.push_back(lowerRightGridVertex);

				FlatGridPoint p((upperLeftGridVertex.posX() + upperRightGridVertex.posX()) / 2, (upperLeftGridVertex.posZ() + lowerLeftGridVertex.posZ()) / 2);
				m_altitude = MapManager::interpolateAltitude(gridVertex, p);

				// 0 based prog of the patch on the X axis: 0 is the coord of the first patch on the X axis along the positive direction, -1 is the coord of the first patch on the X axis along the negative direction
				m_posX = int(upperLeftGridVertex.posX() / g_gridStepInWU);
				// 0 based prog of the patch on the Z axis: 0 is the coord of the first patch on the Z axis along the positive direction, -1 is the coord of the first patch on the Z axis along the negative direction
				m_posZ = int(upperLeftGridVertex.posZ() / g_gridStepInWU);
			}

			// needed to use an istance of gridPoint as a key in a map (to keep the map sorted by z and by x for equal z)
			// first row, second row, ... etc
			bool operator<(const GridPatch& p) const
			{
				if (m_posZ < p.m_posZ)
					return true;
				if (m_posZ > p.m_posZ)
					return false;
				else
					return m_posX < p.m_posX;
			}
			bool operator==(const GridPatch& p) const
			{
				if (m_posZ == p.m_posZ && m_posX == p.m_posX)
					return true;
				else
					return false;
			}
			float getAltitude(void)
			{
				return m_altitude; 
			};
			enum class vertexPos
			{
				upper_left = 0,		// P1	idx 0
				upper_right = 1,	// P2	idx 1
				lower_left = 2,		// P3	idx 2
				lower_right = 3		// P4	idx 3
			};
			SQLInterface::GridVertex getVertex(vertexPos pos)
			{
				switch (pos)
				{
				case vertexPos::upper_left:
					return gridVertex[0];
				case vertexPos::upper_right:
					return gridVertex[1];
				case vertexPos::lower_left:
					return gridVertex[2];
				case vertexPos::lower_right:
					return gridVertex[3];
				default:
					throw(MapManagerException(__FUNCTION__, string("Unknown vertxPos").c_str()));
				}
			}

		private:
			vector<SQLInterface::GridVertex> gridVertex;
			float m_altitude;
			int m_posX;
			int m_posZ;
		};

		class InitMap
		{
		public:
			std::thread m_alignCacheAndDbThread;
			bool m_alignCacheAndDbThreadRequiredExit = true;

		};

		typedef std::map<int, InitMap*> MapInitMapPerLevel;
		typedef std::map<size_t, MapInitMapPerLevel> MapInitMap;

		_declspec(dllexport) MapManager(/*const char* logPath = NULL, plog::Severity sev = plog::Severity::none, plog::IAppender* appender = nullptr,*/ char* configFileName = nullptr, bool multiThreadEnvironment = false);
		_declspec(dllexport) ~MapManager();
		_declspec(dllexport) static void staticInit(const char* logPath, plog::Severity sev, plog::IAppender* appender = nullptr, bool multiThreadEnvironment = true/* every thread is provided with its own DB connection associated to the thread::id*/);
		_declspec(dllexport) static void staticDeinit(void);
		virtual const char* classname() 
		{
			return "MapManager"; 
		}

		void instrument(bool b)
		{
			m_instrumented = b; 
		};
		_declspec(dllexport) static void setLogMaxSeverity(plog::Severity sev);
		void consoleDebugMode(bool b)
		{
			m_consoleDebugMode = b;
			if (m_SqlInterface)
				m_SqlInterface->consoleDebugMode(b);
		};
		bool consoleDebugMode(void)
		{
			return m_consoleDebugMode; 
		}
		bool instrumented(void) 
		{
			return m_instrumented; 
		};

		_declspec(dllexport) __int64 addWD(WorldDefiner& WD);
		_declspec(dllexport) bool eraseWD(WorldDefiner& WD);
		_declspec(dllexport) bool eraseWD(float posX, float posZ, int level, WDType type);
		_declspec(dllexport) bool eraseWD(__int64 wdRowid);
		_declspec(dllexport) int getNumVertexMarkedForUpdate(void);
		_declspec(dllexport) void LoadGISMap(const char* fileInput, bool writeReport, float metersInWU = 1.0, int level = 0);
		_declspec(dllexport) void DumpDB(void);
		_declspec(dllexport) void UpdateValues(void);
		_declspec(dllexport) void finalizeDB(void)
		{
			if (m_SqlInterface) m_SqlInterface->finalizeDB(); 
		}
		_declspec(dllexport) std::string getMapName(void);
		_declspec(dllexport) float gridStepInWU(void);

		enum class anchorType
		{
			center = 0,
			upperleftcorner = 1
		};
		_declspec(dllexport) void getVertices(float anchorXInWUs, float anchorZInWUs, anchorType type, float size, vector<SQLInterface::GridVertex>& mesh, int& numPointX, int& numPointZ, float& gridStepInWU, int level = 0);
		_declspec(dllexport) void getVertices(float& anchorXInWUs, float& anchorZInWUs, anchorType type, int numVerticesX, int numVerticesZ, vector<SQLInterface::GridVertex>& mesh, float& gridStepInWU, int level = 0);
		_declspec(dllexport) void getQuadrantVertices(float lowerXGridVertex, float lowerZGridVertex, int numVerticesPerSize, float& gridStepInWU, int level, std::string& meshId, std::string& meshBuffer);
		_declspec(dllexport) int alignDiskCacheAndDB(TheWorld_Utils::MeshCacheBuffer& cache, bool& stop, bool writeCompactVerticesToDB);
		_declspec(dllexport) void alignDiskCacheAndDB(bool isInEditor, size_t numVerticesPerSize, int level);
		_declspec(dllexport) void alignDiskCacheAndDBTask(size_t numVerticesPerSize, int level);
		_declspec(dllexport) void stopAlignDiskCacheAndDBTasks(bool isInEditor);
		_declspec(dllexport) void sync(bool isInEditor, size_t numVerticesPerSize, float gridStepinWU, bool& foundUpdatedQuadrant, int& level, float& lowerXGridVertex, float& lowerZGridVertex);
		_declspec(dllexport) void stopAlignDiskCacheAndDBTask(size_t numVerticesPerSize, int level);
		_declspec(dllexport) bool writeDiskCacheToDB(TheWorld_Utils::MeshCacheBuffer& cache, bool& stop, bool writeCompactVerticesToDB);
		_declspec(dllexport) bool writeDiskCacheFromDB(TheWorld_Utils::MeshCacheBuffer& cache, bool& stop);
		_declspec(dllexport) void uploadCacheBuffer(float lowerXGridVertex, float lowerZGridVertex, int numVerticesPerSize, float& gridStepInWU, int level, std::string& meshBuffer);
		_declspec(dllexport) void getPatches(float anchorX, float anchorZ, anchorType type, float size, vector<GridPatch>& patches, int& numPatchX, int& numPatchZ, float& gridStepInWU, int level = 0);
		_declspec(dllexport) inline float calcPreviousCoordOnTheGridInWUs(float coordInWUs);
		_declspec(dllexport) inline float calcNextCoordOnTheGridInWUs(float coordInWUs);

	private:
		float computeAltitude(SQLInterface::GridVertex& gridVertex, std::vector<WorldDefiner>& wdMap);
		float computeAltitudeModifier(SQLInterface::GridVertex& gridVertex, const WorldDefiner& wd, float distanceFromWD = -1, float wdAltitude = -1);
		void calcSquareFlatGridMinMaxToExpand(float minXInWUs, float maxXInWUs, float minZInWUs, float maxZInWUs, int& minGridPosX, int& maxGridPosX, int& minGridPosZ, int& maxGridPosZ, float& gridStepInWU);
		void getSquareFlatGridToExpand(float minXInWUs, float maxXInWUs, float minZInWUs, float maxZInWUs, vector<FlatGridPoint>& grid, int& numPointX, int& numPointZ, float& gridStepInWU);
		void getFlatGrid(float minXInWUs, float maxXInWUs, float minZInWUs, float maxZInWUs, vector<FlatGridPoint>& grid, int& numPointX, int& numPointZ, float& gridStepInWU);
		void getFlatGrid(float minXInWUs, float minZInWUs, int numPointX, int numPointZ, vector<FlatGridPoint>& grid, float& gridStepInWU);
		void internalGetVertices(float min_X_OnTheGridInWUs, float max_X_OnTheGridInWUs, float min_Z_OnTheGridInWUs, float max_Z_OnTheGridInWUs, vector<SQLInterface::GridVertex>& mesh, int& numPointX, int& numPointZ, float& gridStepInWU, int& numFoundInDB, int level = 0);
		void internalGetVertices(float min_X_OnTheGridInWUs, float min_Z_OnTheGridInWUs, int numVerticesX, int numVerticesZ, vector<SQLInterface::GridVertex>& mesh, float& gridStepInWU, int& numFoundInDB, int level);
		void internalGetVertices(float min_X_OnTheGridInWUs, float max_X_OnTheGridInWUs, float min_Z_OnTheGridInWUs, float max_Z_OnTheGridInWUs, int numVerticesX, int numVerticesZ, vector<SQLInterface::GridVertex>& mesh, float& gridStepInWU, int& numFoundInDB, int level);
		void getEmptyVertexGrid(vector<FlatGridPoint>& grid, vector<SQLInterface::GridVertex>& mesh, int level = 0);
		float getDistance(float x1, float y1, float x2, float y2);
		bool TransformProjectedCoordEPSG3857ToGeoCoordEPSG4326(double X, double Y, double& lon, double& lat, int& lonDegrees, int& lonMinutes, double& lonSeconds, int& latDegrees, int& latMinutes, double& latSeconds);
		void DecimalDegreesToDegreesMinutesSeconds(double decimalDegrees, int& degrees, int& minutes, double& seconds);
		//float getDistance(Vector3f v1, Vector3f v2);
		static float interpolateAltitude(vector<SQLInterface::GridVertex>& vectGridVertex, FlatGridPoint& pos);
		std::string getDataPath(void) 
		{
			return m_dataPath; 
		}

	private:
		static bool staticMapManagerInitializationDone;
		static std::recursive_mutex s_staticMapManagerInitializationMtx;
		//static enum class DBType s_dbType;

		SQLInterface* m_SqlInterface;
		bool m_instrumented;
		bool m_consoleDebugMode;
		std::string m_dataPath;
		MapManagerUtils m_utils;
		//static std::recursive_mutex s_mtxInternalData;

		static std::recursive_mutex s_cacheMtx;

		static std::recursive_mutex s_initMapMtx;
		static MapInitMap s_mapInitMap;

		static std::map<QuadrantPos, size_t> s_mapQuadrantToUpdate;
		static std::recursive_mutex s_mapQuadrantToUpdateMtx;

		static std::recursive_mutex s_dbExclusiveAccesMtx;
	};
}


