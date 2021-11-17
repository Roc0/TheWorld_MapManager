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
	// I M P O R T A N T: the metrics in this class is always expressed in World Units (WU)
	// size of the grid of vertices used to expand the map (for example on new WD), this size is expressed in number of vertices so it is an int
	//const int g_DBGrowingBlockVertexNumberShift = 10;	// 10 ==> g_DBGrowingBlockVertexNumber = 1024;
	const int g_DBGrowingBlockVertexNumberShift = 8;	// 8 ==> g_DBGrowingBlockVertexNumber = 256;
	const int g_DBGrowingBlockVertexNumber = 1 << g_DBGrowingBlockVertexNumberShift;
	const float g_gridStepInWU = 10.0;		// distance in world unit between a vertice of the grid and the next

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

		_declspec(dllexport) __int64 addWD(WorldDefiner& WD);
		_declspec(dllexport) bool eraseWD(WorldDefiner& WD);
		_declspec(dllexport) bool eraseWD(float posX, float posZ, int level, WDType type);
		_declspec(dllexport) bool eraseWD(__int64 wdRowid);
		_declspec(dllexport) float computeAltitude(SQLInterface::MapVertex& mapVertex, std::vector<WorldDefiner>& wdMap);
		_declspec(dllexport) float computeAltitudeElevator(SQLInterface::MapVertex& mapVertex, WorldDefiner& wd, float distanceFromWD = -1);
		_declspec(dllexport) void UpdateValues(void);
		_declspec(dllexport) int getNumVertexMarkedForUpdate(void);
		_declspec(dllexport) void LoadGISMap(const char* fileInput, bool writeReport, float metersInWU = 1.0, int level = 0);
		_declspec(dllexport) void DumpDB(void);

		_declspec(dllexport) void finalizeDB(void) { if (m_SqlInterface) m_SqlInterface->finalizeDB(); }

	private:
		void calcSquareGridMinMax(float minAOEX, float maxAOEX, float minAOEZ, float maxAOEZ, int& minGridPosX, int& maxGridPosX, int& minGridPosZ, int& maxGridPosZ, float& gridStepInWU);
		struct gridPoint
		{
			// use to keep the map sorted by x, y if used as key in a map 
			bool operator()(const gridPoint& p1, const gridPoint& p2) const
			{
				if (p1.x < p2.x)
					return true;
				if (p1.x > p2.x)
					return false;
				else
					return p1.z < p2.z;
			}
			float x;
			float z;
		};
		void getSquareGrid(float minAOEX, float maxAOEX, float minAOEZ, float maxAOEZ, vector<gridPoint>& grid, int& numPointX, int& numPointZ, float& gridStepInWU);
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


