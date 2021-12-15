#pragma once

#define _USE_MATH_DEFINES // for C++

#include "WorldDefiner.h"
#include "SQLInterface.h"
#include "MapManagerException.h"
#include "Utils.h"

//#include "Eigen\Dense"
//using namespace Eigen;

namespace TheWorld_MapManager
{
	// ************************************************************************************
	// I M P O R T A N T: the metrics in this class is always expressed in World Units (WU)
	// ************************************************************************************

	// ************************************************************************************************************************************************
	// size of the square grid of vertices used to expand the map (for example on new WD), this size is expressed in number of vertices so it is an int
	// ************************************************************************************************************************************************
	const string GrowingBlockVertexNumberShiftParamName = "GrowingBlockVertexNumberShift";
	//int g_DBGrowingBlockVertexNumberShift = 10;	// 10 ==> g_DBGrowingBlockVertexNumber = 1024;
	//int g_DBGrowingBlockVertexNumberShift = 8;	// 8 ==> g_DBGrowingBlockVertexNumber = 256;
	int g_DBGrowingBlockVertexNumberShift = 0;
	int g_DBGrowingBlockVertexNumber = 1 << g_DBGrowingBlockVertexNumberShift;
	// ************************************************************************************************************************************************

	const string GridStepInWUParamName = "GridStepInWU";
	float g_gridStepInWU = 0.0;		// distance in world unit between a vertice of the grid and the next

	class MapManager
	{
	public:
		_declspec(dllexport) MapManager();
		_declspec(dllexport) ~MapManager();
		virtual const char* classname() { return "MapManager"; }

		void instrument(bool b) { m_instrumented = b; };
		void debugMode(bool b)
		{
			m_debugMode = b;
			if (m_SqlInterface)
				m_SqlInterface->debugMode(b);
		};
		bool debugMode(void) { return m_debugMode; }
		bool instrumented(void) { return m_instrumented; };

		// Grid / GridPoint are in WUs
		struct gridPoint
		{
			// needed to use an istance of gridPoint as a key in a map (to keep the map sorted by z and by x for equal z)
			// first row, second row, ... etc
			bool operator<(const gridPoint& p) const
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

		_declspec(dllexport) __int64 addWD(WorldDefiner& WD);
		_declspec(dllexport) bool eraseWD(WorldDefiner& WD);
		_declspec(dllexport) bool eraseWD(float posX, float posZ, int level, WDType type);
		_declspec(dllexport) bool eraseWD(__int64 wdRowid);
		_declspec(dllexport) int getNumVertexMarkedForUpdate(void);
		_declspec(dllexport) void LoadGISMap(const char* fileInput, bool writeReport, float metersInWU = 1.0, int level = 0);
		_declspec(dllexport) void DumpDB(void);

		enum class anchorType
		{
			center = 0,
			upperleftcorner = 1
		} ;
		_declspec(dllexport) void getMesh(float anchorX, float anchorZ, anchorType type, float size, vector<SQLInterface::MapVertex>& mesh);

		_declspec(dllexport) void UpdateValues(void);

		_declspec(dllexport) void finalizeDB(void) { if (m_SqlInterface) m_SqlInterface->finalizeDB(); }

	private:
		float computeAltitude(SQLInterface::MapVertex& mapVertex, std::vector<WorldDefiner>& wdMap);
		float computeAltitudeElevator(SQLInterface::MapVertex& mapVertex, WorldDefiner& wd, float distanceFromWD = -1);
		void calcSquareGridMinMax(float minAOEX, float maxAOEX, float minAOEZ, float maxAOEZ, int& minGridPosX, int& maxGridPosX, int& minGridPosZ, int& maxGridPosZ, float& gridStepInWU);
		void getSquareGrid(float minAOEX, float maxAOEX, float minAOEZ, float maxAOEZ, vector<gridPoint>& grid, int& numPointX, int& numPointZ, float& gridStepInWU);
		inline float calcPreviousCoordOnTheGrid(float coord);
		inline float calcNextCoordOnTheGrid(float coord);
		float getDistance(float x1, float y1, float x2, float y2);
		bool TransformProjectedCoordEPSG3857ToGeoCoordEPSG4326(double X, double Y, double& lon, double& lat, int& lonDegrees, int& lonMinutes, double& lonSeconds, int& latDegrees, int& latMinutes, double& latSeconds);
		void DecimalDegreesToDegreesMinutesSeconds(double decimalDegrees, int& degrees, int& minutes, double& seconds);
		//float getDistance(Vector3f v1, Vector3f v2);

	private:
		SQLInterface* m_SqlInterface;
		bool m_instrumented;
		bool m_debugMode;
		std::string m_dataPath;
	};
}


