#pragma once

#include <stdio.h>
#define _USE_MATH_DEFINES // for C++

#include "WorldDefiner.h"
#include "SQLInterface.h"
#include "MapManagerException.h"

namespace TheWorld_MapManager
{
	// size of the square of vertices used to expand the map (for example on new WD) 
	const int g_DBGrowingBlockVertexNumberShift = 10;	// 10 ==> g_DBGrowingBlockVertexNumber = 1024;
	const int g_DBGrowingBlockVertexNumber = 1 << g_DBGrowingBlockVertexNumberShift;

	class MapManager
	{
	public:
		_declspec(dllexport) MapManager();
		_declspec(dllexport) ~MapManager();

		_declspec(dllexport) void addWD(WorldDefiner& WD);
	
	private:
		float getAzimuthAOEDeviation(float radius, float azimuth, float AOE);
		float getDistance(float x1, float y1, float x2, float y2);

	private:
		SQLInterface* m_SqlInterface;
	};
}


