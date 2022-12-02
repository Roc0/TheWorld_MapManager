#pragma once

#define _USE_MATH_DEFINES // for C++

#include "WorldDefiner.h"
#include "SQLInterface.h"
#include "MapManagerException.h"
#include "Utils.h"
#include <plog/Log.h>

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
		// Grid / GridPoint are in WUs
		class FlatGridPoint
		{
		public:
			FlatGridPoint(void) : x(0.0f), z(0.0f) {}
			FlatGridPoint(float _x, float _z) { this->x = _x;	this->z = _z; }
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

		//// Grid / GridVertex are in WUs
		//class GridVertex
		//{
		//public:
		//	GridVertex(void) : x(0.0f), y(0.0f), z(0.0f), level(0) {}
		//	GridVertex(float _x, float _y, float _z, int _level) { this->x = _x;	this->y = _y;	this->z = _z;	this->level = _level;}
		//	//GridVertex(std::string serializedBuffer)
		//	//{
		//	//	sscanf_s(serializedBuffer.c_str(), "%f-%f-%f-%d", &x, &y, &z, &level);
		//	//}
		//	//GridVertex(const char* serializedBuffer)
		//	//{
		//	//	sscanf_s(serializedBuffer, "%f-%f-%f-%d", &x, &y, &z, &level);
		//	//}
		//	GridVertex(BYTE* stream, size_t& size)
		//	{
		//		size_t _size;
		//		size = 0;
		//		x = deserializeFromByteStream<float>(stream + size, _size);
		//		size += _size;
		//		y = deserializeFromByteStream<float>(stream + size, _size);
		//		size += _size;
		//		z = deserializeFromByteStream<float>(stream + size, _size);
		//		size += _size;
		//		level = deserializeFromByteStream<int>(stream + size, _size);
		//		size += _size;
		//	}

		//	~GridVertex()
		//	{
		//	}

		//	// needed to use an istance of gridPoint as a key in a map (to keep the map sorted by z and by x for equal z)
		//	// first row, second row, ... etc
		//	bool operator<(const GridVertex& p) const
		//	{
		//		if (z < p.z)
		//			return true;
		//		if (z > p.z)
		//			return false;
		//		else
		//			return x < p.x;
		//	}
		//	
		//	bool operator==(const GridVertex& p) const
		//	{
		//		if (x == p.x && y == p.y && z == p.z && level == p.level)
		//			return true;
		//		else
		//			return false;
		//	}

		//	bool equalsApartFromAltitude(const GridVertex& p) const
		//	{
		//		if (x == p.x && z == p.z && level == p.level)
		//			return true;
		//		else
		//			return false;
		//	}
		//	
		//	//std::string serialize(void)
		//	//{
		//	//	char buffer[256];
		//	//	sprintf_s(buffer, "%f-%f-%f-%d", x, y, z, level);
		//	//	return buffer;
		//	//}
		//	
		//	void serialize(BYTE* stream, size_t& size)
		//	{
		//		size_t sz;
		//		serializeToByteStream<float>(x, stream, sz);
		//		size = sz;
		//		serializeToByteStream<float>(y, stream + size, sz);
		//		size += sz;
		//		serializeToByteStream<float>(z, stream + size, sz);
		//		size += sz;
		//		serializeToByteStream<int>(level, stream + size, sz);
		//		size += sz;
		//	}

		//	std::string toString()
		//	{
		//		return "Level=" + std::to_string(level) + "-X=" + std::to_string(x) + "-Z=" + std::to_string(z) + "-Altitude=" + std::to_string(y);
		//	}

		//	float altitude(void) { return y; }
		//	float posX(void) { return x; }
		//	float posZ(void) { return z; }
		//	int lvl(void) { return level; }
		//	void setAltitude(float a) { y = a; }

		//private:
		//	float x;
		//	float y;
		//	float z;
		//	int level;
		//};

		//class QuadrantId
		//{
		//public:
		//	enum class DirectionSlot
		//	{
		//		Center = -1,

		//		//
		//		// - o
		//		//
		//		XMinus = 0,

		//		//
		//		//   o -
		//		//
		//		XPlus = 1,

		//		//   -
		//		//   o
		//		//
		//		ZMinus = 2,

		//		//
		//		//   o
		//		//   -
		//		ZPlus = 3,

		//	};

		//	QuadrantId()
		//	{
		//		m_lowerXGridVertex = m_lowerZGridVertex = m_gridStepInWU = 0;
		//		m_numVerticesPerSize = m_level = 0;
		//		m_sizeInWU = 0;
		//		m_initialized = false;
		//	}

		//	QuadrantId(const QuadrantId& quadrantId)
		//	{
		//		m_lowerXGridVertex = quadrantId.m_lowerXGridVertex;
		//		m_lowerZGridVertex = quadrantId.m_lowerZGridVertex;
		//		m_numVerticesPerSize = quadrantId.m_numVerticesPerSize;
		//		m_level = quadrantId.m_level;
		//		m_gridStepInWU = quadrantId.m_gridStepInWU;
		//		m_sizeInWU = quadrantId.m_sizeInWU;
		//		m_tag = quadrantId.m_tag;
		//		m_initialized = true;
		//	}

		//	QuadrantId(float x, float z, int level, int numVerticesPerSize, float gridStepInWU)
		//	{
		//						float gridSizeInWU = numVerticesPerSize * gridStepInWU;
		//		m_lowerXGridVertex = floor(x / gridSizeInWU) * gridSizeInWU;
		//		m_lowerZGridVertex = floor(z / gridSizeInWU) * gridSizeInWU;
		//		m_numVerticesPerSize = numVerticesPerSize;
		//		m_level = level;
		//		m_gridStepInWU = gridStepInWU;
		//		m_sizeInWU = (m_numVerticesPerSize - 1) * m_gridStepInWU;
		//		m_initialized = true;
		//	}

		//	bool operator<(const QuadrantId& quadrantId) const
		//	{
		//		assert(m_level == quadrantId.m_level);
		//		if (m_level < quadrantId.m_level)
		//			return true;
		//		if (m_level > quadrantId.m_level)
		//			return false;
		//		// m_level == quadrantId.m_level

		//		assert(m_sizeInWU == quadrantId.m_sizeInWU);
		//		if (m_sizeInWU < quadrantId.m_sizeInWU)
		//			return true;
		//		if (m_sizeInWU > quadrantId.m_sizeInWU)
		//			return false;
		//		// m_sizeInWU == quadrantId.m_sizeInWU

		//		if (m_lowerZGridVertex < quadrantId.m_lowerZGridVertex)
		//			return true;
		//		if (m_lowerZGridVertex > quadrantId.m_lowerZGridVertex)
		//			return false;
		//		// m_lowerZGridVertex == quadrantId.m_lowerZGridVertex

		//		return m_lowerXGridVertex < quadrantId.m_lowerXGridVertex;
		//	}

		//	bool operator==(const QuadrantId& quadrantId) const
		//	{
		//		if (m_lowerXGridVertex == quadrantId.m_lowerXGridVertex
		//			&& m_lowerZGridVertex == quadrantId.m_lowerZGridVertex
		//			&& m_numVerticesPerSize == quadrantId.m_numVerticesPerSize
		//			&& m_level == quadrantId.m_level
		//			&& m_gridStepInWU == quadrantId.m_gridStepInWU)
		//			return true;
		//		else
		//			return false;
		//	}
		//	
		//	QuadrantId operator=(const QuadrantId& quadrantId)
		//	{
		//		m_lowerXGridVertex = quadrantId.m_lowerXGridVertex;
		//		m_lowerZGridVertex = quadrantId.m_lowerZGridVertex;
		//		m_numVerticesPerSize = quadrantId.m_numVerticesPerSize;
		//		m_level = quadrantId.m_level;
		//		m_gridStepInWU = quadrantId.m_gridStepInWU;
		//		m_sizeInWU = quadrantId.m_sizeInWU;
		//		m_tag = quadrantId.m_tag;
		//		m_initialized = true;
		//		return *this;
		//	}

		//	std::string getId(void)
		//	{
		//		return "ST" + to_string(m_gridStepInWU) + "_SZ" + to_string(m_numVerticesPerSize) + "_L" + to_string(m_level) + "_X" + to_string(m_lowerXGridVertex) + "_Z" + to_string(m_lowerZGridVertex);
		//	}

		//	float getLowerXGridVertex() { return m_lowerXGridVertex; };
		//	float getLowerZGridVertex() { return m_lowerZGridVertex; };
		//	int getNumVerticesPerSize() { return m_numVerticesPerSize; };
		//	int getLevel() { return m_level; };
		//	float getGridStepInWU() { return m_gridStepInWU; };
		//	float getSizeInWU() { return m_sizeInWU; };
		//	_declspec(dllexport) QuadrantId getQuadrantId(enum class DirectionSlot dir, int numSlot = 1);
		//	void setTag(std::string tag) { m_tag = tag; }
		//	std::string getTag(void) { return m_tag; }
		//	_declspec(dllexport) size_t distanceInPerimeter(QuadrantId& q);
		//	bool isInitialized(void) { return m_initialized; }

		//private:
		//	// ID
		//	float m_lowerXGridVertex;
		//	float m_lowerZGridVertex;
		//	int m_numVerticesPerSize;
		//	int m_level;
		//	float m_gridStepInWU;
		//	// ID

		//	bool m_initialized;

		//	float m_sizeInWU;
		//	std::string m_tag;
		//};
		//	
		//class Quadrant
		//{
		//public:
		//	//Quadrant(MapManager* mapManager)
		//	//{
		//	//	m_mapManager = mapManager;
		//	//}
		//	
		//	Quadrant(QuadrantId& quadrantId /*, MapManager* mapManager*/)
		//	{
		//		m_quadrantId = quadrantId;
		//		//m_mapManager = mapManager;
		//		m_populated = false;
		//	}

		//	~Quadrant()
		//	{
		//		m_vectGridVertices.clear();
		//	}

		//	void implementId(QuadrantId& quadrantId)
		//	{
		//		m_quadrantId = quadrantId;
		//	}

		//	_declspec(dllexport) void populateGridVertices(float& initialViewerPosX, float& initialViewerPosZ);

		//	std::vector<GridVertex>& getGridVertices(void)
		//	{
		//		return m_vectGridVertices;
		//	}

		//	QuadrantId getId(void) { return m_quadrantId; }

		//	bool isPopulated(void) { return m_populated; }

		//private:
		//	QuadrantId m_quadrantId;
		//	std::vector<GridVertex> m_vectGridVertices;
		//	bool m_populated;
		//	//MapManager* m_mapManager;
		//};
		
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
			float getAltitude(void) { return m_altitude; };
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

		_declspec(dllexport) MapManager(const char *logPath = NULL, plog::Severity sev = plog::Severity::none, plog::IAppender* appender = NULL, char * configFileName = NULL);
		_declspec(dllexport) ~MapManager();
		virtual const char* classname() { return "MapManager"; }

		void instrument(bool b) { m_instrumented = b; };
		void setLogMaxSeverity(plog::Severity sev)
		{
			plog::get()->setMaxSeverity(sev);
		}
		void consoleDebugMode(bool b)
		{
			m_consoleDebugMode = b;
			if (m_SqlInterface)
				m_SqlInterface->consoleDebugMode(b);
		};
		bool consoleDebugMode(void) { return m_consoleDebugMode; }
		bool instrumented(void) { return m_instrumented; };

		_declspec(dllexport) __int64 addWD(WorldDefiner& WD);
		_declspec(dllexport) bool eraseWD(WorldDefiner& WD);
		_declspec(dllexport) bool eraseWD(float posX, float posZ, int level, WDType type);
		_declspec(dllexport) bool eraseWD(__int64 wdRowid);
		_declspec(dllexport) int getNumVertexMarkedForUpdate(void);
		_declspec(dllexport) void LoadGISMap(const char* fileInput, bool writeReport, float metersInWU = 1.0, int level = 0);
		_declspec(dllexport) void DumpDB(void);
		_declspec(dllexport) void UpdateValues(void);
		_declspec(dllexport) void finalizeDB(void) { if (m_SqlInterface) m_SqlInterface->finalizeDB(); }
		_declspec(dllexport) float gridStepInWU(void);
		//_declspec(dllexport) MapManager::Quadrant* getQuadrant(float& viewerPosX, float& viewerPosZ, int level, int numVerticesPerSize);
		//_declspec(dllexport) MapManager::Quadrant* getQuadrant(QuadrantId q, enum class QuadrantId::DirectionSlot dir);

		enum class anchorType
		{
			center = 0,
			upperleftcorner = 1
		};
		_declspec(dllexport) void getVertices(float anchorXInWUs, float anchorZInWUs, anchorType type, float size, vector<SQLInterface::GridVertex>& mesh, int& numPointX, int& numPointZ, float& gridStepInWU, int level = 0);
		_declspec(dllexport) void getVertices(float& anchorXInWUs, float& anchorZInWUs, anchorType type, int numVerticesX, int numVerticesZ, vector<SQLInterface::GridVertex>& mesh, float& gridStepInWU, int level = 0);
		_declspec(dllexport) void getPatches(float anchorX, float anchorZ, anchorType type, float size, vector<GridPatch>& patches, int& numPatchX, int& numPatchZ, float& gridStepInWU, int level = 0);
		_declspec(dllexport) inline float calcPreviousCoordOnTheGridInWUs(float coordInWUs);
		_declspec(dllexport) inline float calcNextCoordOnTheGridInWUs(float coordInWUs);

	private:
		float computeAltitude(SQLInterface::GridVertex& gridVertex, std::vector<WorldDefiner>& wdMap);
		float computeAltitudeElevator(SQLInterface::GridVertex& gridVertex, WorldDefiner& wd, float distanceFromWD = -1);
		void calcSquareFlatGridMinMaxToExpand(float minXInWUs, float maxXInWUs, float minZInWUs, float maxZInWUs, int& minGridPosX, int& maxGridPosX, int& minGridPosZ, int& maxGridPosZ, float& gridStepInWU);
		void getSquareFlatGridToExpand(float minXInWUs, float maxXInWUs, float minZInWUs, float maxZInWUs, vector<FlatGridPoint>& grid, int& numPointX, int& numPointZ, float& gridStepInWU);
		void getFlatGrid(float minXInWUs, float maxXInWUs, float minZInWUs, float maxZInWUs, vector<FlatGridPoint>& grid, int& numPointX, int& numPointZ, float& gridStepInWU);
		void getFlatGrid(float minXInWUs, float minZInWUs, int numPointX, int numPointZ, vector<FlatGridPoint>& grid, float& gridStepInWU);
		void internalGetVertices(float min_X_OnTheGridInWUs, float max_X_OnTheGridInWUs, float min_Z_OnTheGridInWUs, float max_Z_OnTheGridInWUs, vector<SQLInterface::GridVertex>& mesh, int& numPointX, int& numPointZ, float& gridStepInWU, int& numFoundInDB, int level = 0);
		void internalGetVertices(float min_X_OnTheGridInWUs, float min_Z_OnTheGridInWUs, int numVerticesX, int numVerticesZ, vector<SQLInterface::GridVertex>& mesh, float& gridStepInWU, int& numFoundInDB, int level);
		void getEmptyVertexGrid(vector<FlatGridPoint>& grid, vector<SQLInterface::GridVertex>& mesh, int level = 0);
		float getDistance(float x1, float y1, float x2, float y2);
		bool TransformProjectedCoordEPSG3857ToGeoCoordEPSG4326(double X, double Y, double& lon, double& lat, int& lonDegrees, int& lonMinutes, double& lonSeconds, int& latDegrees, int& latMinutes, double& latSeconds);
		void DecimalDegreesToDegreesMinutesSeconds(double decimalDegrees, int& degrees, int& minutes, double& seconds);
		//float getDistance(Vector3f v1, Vector3f v2);
		static float interpolateAltitude(vector<SQLInterface::GridVertex>& vectGridVertex, FlatGridPoint& pos);
		std::string getDataPath(void) { return m_dataPath; }

	private:
		SQLInterface* m_SqlInterface;
		bool m_instrumented;
		bool m_consoleDebugMode;
		std::string m_dataPath;
		utils m_utils;
	};
}


