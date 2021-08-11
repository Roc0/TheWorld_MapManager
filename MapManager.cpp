#include "pch.h"

#define _USE_MATH_DEFINES // for C++

#include "assert.h"

#include "json/json.h"
#include <iostream>
#include <fstream>

#include "MapManager.h"
#include "DBSQLLite.h"



namespace TheWorld_MapManager
{
	MapManager::MapManager()
	{
		string s = getModuleLoadPath();
		s += "\\TheWorld_MapManager.json";

		Json::Value root;
		std::ifstream jsonFile(s);
		jsonFile >> root;
		m_dataPath = root["DataPath"].asString();

		m_SqlInterface = new DBSQLLite(DBType::SQLLite, m_dataPath.c_str());
		m_instrumented = false;
		m_debugMode = false;
	}

	MapManager::~MapManager()
	{
		if (m_SqlInterface)
			m_SqlInterface->finalizeDB();
	}

	bool MapManager::addWD(WorldDefiner& WD)
	{
		debugUtils debugUtil;
		TimerMs clock; // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();

		// we have to find all the vertices affected by AOE according to the fact that the map can grow with square map of point of g_DBGrowingBlockVertexNumber vertices
		float minAOEX = WD.getPosX() - WD.getAOE();
		if (minAOEX < 0 && -minAOEX < g_DBGrowingBlockVertexNumber)
			minAOEX = -g_DBGrowingBlockVertexNumber;

		float maxAOEX = WD.getPosX() + WD.getAOE();
		if (maxAOEX > 0 && maxAOEX < g_DBGrowingBlockVertexNumber)
			maxAOEX = g_DBGrowingBlockVertexNumber;

		float minAOEZ = WD.getPosZ() - WD.getAOE();
		if (minAOEZ < 0 && -minAOEZ < g_DBGrowingBlockVertexNumber)
			minAOEZ = -g_DBGrowingBlockVertexNumber;

		float maxAOEZ = WD.getPosZ() + WD.getAOE();
		if (maxAOEZ > 0 && maxAOEZ < g_DBGrowingBlockVertexNumber)
			maxAOEZ = g_DBGrowingBlockVertexNumber;

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
		vector<SQLInterface::mapVertex> v;
		
		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Computing affected vertices by WorldDefiner: ");

		int numVertices = 0;
		for (int x = minVertexX; x <= maxVertexX; x++)
		{
			for (int z = minVertexZ; z <= maxVertexZ; z++)
			{
				// guard
				{
					numVertices++;
					assert(numVertices <= mapVertexSize);
				}

				SQLInterface::mapVertex mapv;
				mapv.posX = (float)x * g_distanceFromVerticesInWU;
				mapv.posZ = (float)z * g_distanceFromVerticesInWU;
				mapv.posY = 0.0;
				mapv.rowid = -1;
				mapv.radius = sqrtf(powf(mapv.posX, 2.0) + powf(mapv.posZ, 2.0));
				if ((mapv.posX == 0 && mapv.posZ == 0) || mapv.radius == 0)
					mapv.azimuth = 0;
				else
				{
					//angle of radius with x-axus (complementar of 2PI if Z < 0)
					mapv.azimuth = acosf(mapv.posX / mapv.radius);
					if (mapv.posZ < 0)
						mapv.azimuth = (float)(M_PI * 2.0) - mapv.azimuth;
				}
				
				mapv.level = WD.getLevel();
				mapv.affected = false;
				if (getDistance(WD.getPosX(), WD.getPosZ(), mapv.posX, mapv.posZ) <= WD.getAOE())
					mapv.affected = true;

				v.push_back(mapv);

				if (debugMode() && fmod(numVertices, 1024 * 1000) == 0) debugUtil.printVariablePartOfLine(numVertices);
			}
			//if (debugMode()) debugUtil.printVariablePartOfLine(numVertices);
		}

		if (debugMode()) debugUtil.printVariablePartOfLine(numVertices);

		// Adding / updating WD to DB : this action will add / update all affected point
		bool bret_addWD = m_SqlInterface->addWD(WD, v);

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (debugMode()) debugUtil.printNewLine();

		return bret_addWD;
	}

	float MapManager::getDistance(float x1, float y1, float x2, float y2)
	{
		return sqrtf((powf((x2 - x1), 2.0) + powf((y2 - y1), 2.0)));
	}

	/*float MapManager::getDistance(Vector3f v1, Vector3f v2)
	{
		return (v2 - v1).norm();
	}*/

	void MapManager::UpdateValues(void)
	{
		debugUtils debugUtil;
		TimerMs clock; // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();

		/*
		* Open Transaction
		*/
		m_SqlInterface->beginTransaction();

		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Updating vertices marked for update: ");
		int updated = 0;
		int idx = 0;
		SQLInterface::mapVertex mapVertex;
		vector<WorldDefiner> wdMap;
		bool bFound = m_SqlInterface->getFirstModfiedVertex(mapVertex, wdMap);
		while (bFound)
		{
			idx++;

			m_SqlInterface->updateAltitudeOfVertex(mapVertex.rowid, 1.0);

			updated++;
			if (debugMode() && fmod(idx, 1000) == 0)
			{
				string s = "Vertices marked for update: ";	s += to_string(idx);	s += " - Vertices Updated: ";	s += to_string(updated);
				debugUtil.printVariablePartOfLine(s.c_str());
			}

			bFound = m_SqlInterface->getNextModfiedVertex(mapVertex, wdMap);
		}
		if (debugMode())
		{
			string s = "Vertices marked for update: ";	s += to_string(idx);	s += " - Vertices Updated: ";	s += to_string(updated);
			debugUtil.printVariablePartOfLine(s.c_str());
		}

		m_SqlInterface->eraseModifiedVertices();
			
			
		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (debugMode()) debugUtil.printNewLine();

		/*
		* Close Transaction
		*/
		m_SqlInterface->endTransaction();
	}
}
