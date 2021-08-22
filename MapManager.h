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
	// size of the grid of vertices used to expand the map (for example on new WD), this size is expressed in number of vertices so it is an int
	//const int g_DBGrowingBlockVertexNumberShift = 10;	// 10 ==> g_DBGrowingBlockVertexNumber = 1024;
	const int g_DBGrowingBlockVertexNumberShift = 8;	// 10 ==> g_DBGrowingBlockVertexNumber = 256;
	const int g_DBGrowingBlockVertexNumber = 1 << g_DBGrowingBlockVertexNumberShift;
	const float g_distanceFromVerticesInWU = 1.0;		// distance in world unit between a vertice of the grid and the next

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
		
		_declspec(dllexport) void finalizeDB(void) { if (m_SqlInterface) m_SqlInterface->finalizeDB(); }

		_declspec(dllexport) void Test(void);

	private:
		float getDistance(float x1, float y1, float x2, float y2);
		//float getDistance(Vector3f v1, Vector3f v2);

	private:
		SQLInterface* m_SqlInterface;
		bool m_instrumented;
		bool m_debugMode;
		std::string m_dataPath;
	};
}


