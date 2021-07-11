#include "pch.h"
#include "assert.h"

#include "MapManager.h"
#include "DBSQLLite.h"

namespace TheWorld_MapManager
{
	MapManager::MapManager()
	{
		m_SqlInterface = new DBSQLLite(DBType::SQLLite);
	}

	MapManager::~MapManager()
	{
	}

	void MapManager::addWD(WorldDefiner& WD)
	{
		float azimuthDeviation = getAzimuthAOEDeviation(WD.getRadius(), WD.getAzimuth(), WD.getAOE());

		// we have to find all the vertices affected by AOE according to the fact that the map can grow with square map of point of g_DBGrowingBlockVertexNumber vertices
		float minAOEX = WD.getPosX() - WD.getAOE();
		float maxAOEX = WD.getPosX() + WD.getAOE();
		float minAOEZ = WD.getPosZ() - WD.getAOE();
		float maxAOEZ = WD.getPosZ() + WD.getAOE();

		int minVertexX = int(minAOEX / g_DBGrowingBlockVertexNumber) * g_DBGrowingBlockVertexNumber;
		if (minVertexX < 0 && minVertexX != minAOEX)
			minVertexX -= g_DBGrowingBlockVertexNumber;
		
		int maxVertexX = int(maxAOEX / g_DBGrowingBlockVertexNumber) * g_DBGrowingBlockVertexNumber;
		if (maxVertexX > 0 && maxVertexX != maxAOEX)
			maxVertexX += g_DBGrowingBlockVertexNumber;

		int minVertexZ = int(minAOEZ / g_DBGrowingBlockVertexNumber) * g_DBGrowingBlockVertexNumber;
		if (minVertexZ < 0 && minVertexZ != minAOEZ)
			minVertexZ -= g_DBGrowingBlockVertexNumber;

		int maxVertexZ = int(maxAOEZ / g_DBGrowingBlockVertexNumber) * g_DBGrowingBlockVertexNumber;
		if (maxVertexZ > 0 && maxVertexZ != maxAOEZ)
			maxVertexZ += g_DBGrowingBlockVertexNumber;

		int mapVertexSize = (maxVertexX - minVertexX + 1) * (maxVertexZ - minVertexZ + 1);
		vector<SQLInterface::addWD_mapVertex> v;
		
		int i = 0;
		for (int x = minVertexX; x <= maxVertexX; x++)
		{
			for (int z = minVertexZ; z <= maxVertexZ; z++)
			{
				// guard
				{
					i++;
					assert(i <= mapVertexSize);
				}

				SQLInterface::addWD_mapVertex mapv;
				mapv.posX = (float)x;
				mapv.posZ = (float)z;
				mapv.radius = sqrtf(powf(mapv.posX, 2.0) + powf(mapv.posZ, 2.0));
				if (mapv.radius == 0)
					mapv.azimuth = 0;
				else
				{
					//angle of radius with x-axus (complementar of 2PI if Z < 0)
					mapv.azimuth = acosf(mapv.posX / mapv.radius);
					if (mapv.posZ < 0)
						mapv.azimuth = (float)(M_PI * 2.0) - mapv.azimuth;
				}
				
				mapv.affected = false;
				float minRadius = WD.getRadius() - WD.getAOE();
				float maxRadius = WD.getRadius() + WD.getAOE();
				float minAzimuth = WD.getAzimuth() - azimuthDeviation;
				float maxAzimuth = WD.getAzimuth() + azimuthDeviation;

				if (mapv.radius >= minRadius && mapv.radius <= maxRadius && mapv.azimuth >= minAzimuth && mapv.azimuth <= maxAzimuth)
					mapv.affected = true;

				// guard
				{
					float distance = getDistance(mapv.posX, mapv.posZ, WD.getPosX(), WD.getPosZ());
					if (mapv.affected)
						if (distance > WD.getAOE())
							assert(distance <= WD.getAOE());
					if (distance <= WD.getAOE())
						if (!mapv.affected)
							assert(mapv.affected);
				}

				v.push_back(mapv);
			}
		}

		// Adding / updating WD to DB : this action will add / update all affected point
		m_SqlInterface->addWD(WD, v);
	}

	float MapManager::getAzimuthAOEDeviation(float radius, float azimuth, float AOE)
	{
		float tangentToAOECircle = sqrtf(powf(radius, 2.0) + powf(AOE, 2.0));
		float azimuthDeviation = acosf(radius / tangentToAOECircle);
		return azimuthDeviation;
		
		//return acosf(radius / sqrtf(powf(radius, 2.0) + powf(AOE, 2.0)));;
	}

	float MapManager::getDistance(float x1, float y1, float x2, float y2)
	{
		return sqrtf((powf((x2 - x1), 2.0) + powf((y2 - y1), 2.0)));
	}
}
