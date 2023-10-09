//#include "pch.h"

#define _USE_MATH_DEFINES // for C++

#include "assert.h"

#include "json/json.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <thread>
//#include <filesystem>

#include <Rpc.h>

//#include "GDN_TheWorld_Viewer.h"
#include "MapManager.h"
#include "DBSQLLite.h"
#include "Profiler.h"

#include "shapefil.h"
#include <proj.h>

//namespace fs = std::filesystem;

namespace TheWorld_MapManager
{
	//std::recursive_mutex MapManager::s_mtxInternalData;
	bool MapManager::staticMapManagerInitializationDone = false;
	std::recursive_mutex MapManager::s_staticMapManagerInitializationMtx;
	std::recursive_mutex MapManager::s_initMapMtx;
	std::recursive_mutex MapManager::s_cacheMtx;
	MapManager::MapInitMap MapManager::s_mapInitMap;
	std::map<MapManager::QuadrantPos, size_t> MapManager::s_mapQuadrantToUpdate;
	std::recursive_mutex MapManager::s_mapQuadrantToUpdateMtx;
	//enum class DBType MapManager::s_dbType = DBType::SQLLite;

	const std::string MapNameParamName = "MapName";
	std::string g_mapName = "";

	const bool writeCompactVerticesToDB = true;

	// ************************************************************************************************************************************************
	// size of the square grid of vertices used to expand the map (for example on new WD), this size is expressed in number of vertices so it is an int
	// ************************************************************************************************************************************************
	const std::string GrowingBlockVertexNumberShiftParamName = "GrowingBlockVertexNumberShift";
	//int g_DBGrowingBlockVertexNumberShift = 10;	// 10 ==> g_DBGrowingBlockVertexNumber = 1024;
	//int g_DBGrowingBlockVertexNumberShift = 8;	// 8 ==> g_DBGrowingBlockVertexNumber = 256;
	int g_DBGrowingBlockVertexNumberShift = 0;
	int g_DBGrowingBlockVertexNumber = 1 << g_DBGrowingBlockVertexNumberShift;
	// ************************************************************************************************************************************************

	const std::string GridStepInWUParamName = "GridStepInWU";
	float g_gridStepInWU = 0.0f;		// distance in world unit between a vertex of the grid and the next

	MapManager::MapManager(/*const char* logPath, plog::Severity sev, plog::IAppender* appender,*/ char* configFileName, bool multiThreadEnvironment /* every thread is provided with its own DB connection associated to the thread::id*/)
	{
		s_staticMapManagerInitializationMtx.lock();
		if (!staticMapManagerInitializationDone)
		{
			// actually empty
			staticMapManagerInitializationDone = true;
		}
		s_staticMapManagerInitializationMtx.unlock();

		string sModulePath = getModuleLoadPath();

		string configFilePath = sModulePath;
		if (configFileName == nullptr)
			configFilePath += "\\TheWorld_MapManager.json";
		else
			configFilePath += string("\\") + configFileName;

		//if (appender == nullptr)
		//{
		//	string _logPath;
		//	if (logPath == nullptr)
		//		_logPath = sModulePath += "\\TheWorld_MapManager_Log.txt";
		//	else
		//		_logPath = logPath;
		//	m_utils.staticInit(_logPath.c_str(), sev);
		//}
		//else
		//{
		//	m_utils.staticInit(nullptr, sev, appender);
		//}

		Json::Value root;
		std::ifstream jsonFile(configFilePath);
		jsonFile >> root;
		m_dataPath = root["DataPath"].asString();

		m_SqlInterface = new DBSQLLite(SQLInterface::DBType::SQLLite, m_dataPath.c_str());
		m_instrumented = false;
		m_consoleDebugMode = false;

		std::recursive_mutex& exclusiveDBAccessMutex = m_SqlInterface->getExclusiveDBAccessMutex();

		std::lock_guard<std::recursive_mutex> lock(exclusiveDBAccessMutex);

		std::string s = m_SqlInterface->readParam(GrowingBlockVertexNumberShiftParamName);
		if (s.empty())
			throw(MapManagerException(__FUNCTION__, string("Param <" + GrowingBlockVertexNumberShiftParamName + "> not read from DB").c_str()));
		g_DBGrowingBlockVertexNumberShift = stoi(s);
		g_DBGrowingBlockVertexNumber = 1 << g_DBGrowingBlockVertexNumberShift;
		
		s = m_SqlInterface->readParam(GridStepInWUParamName);
		if (s.empty())
			throw(MapManagerException(__FUNCTION__, string("Param <" + GridStepInWUParamName + "> not read from DB").c_str()));
		g_gridStepInWU = stof(s);

		g_mapName = m_SqlInterface->readParam(MapNameParamName);
		if (g_mapName.empty())
			throw(MapManagerException(__FUNCTION__, string("Param <" + MapNameParamName + "> not read from DB").c_str()));
	}

	MapManager::~MapManager()
	{
		if (m_SqlInterface)
			m_SqlInterface->finalizeDB();
	}

	void MapManager::staticInit(const char* logPath, plog::Severity sev, plog::IAppender* appender, bool multiThreadEnvironment /* every thread is provided with its own DB connection associated to the thread::id*/)
	{
		if (multiThreadEnvironment)
			DBSQLLite::setConnectionType(DBSQLLiteOps::ConnectionType::MultiConn);
		else
			DBSQLLite::setConnectionType(DBSQLLiteOps::ConnectionType::SingleConn);

		string sModulePath = getModuleLoadPath();

		if (appender == nullptr)
		{
			string _logPath;
			if (logPath == nullptr)
				_logPath = sModulePath += "\\TheWorld_MapManager_Log.txt";
			else
				_logPath = logPath;
			MapManagerUtils::staticInit(_logPath.c_str(), sev);
		}
		else
		{
			MapManagerUtils::staticInit(nullptr, sev, appender);
		}
	}

	void MapManager::staticDeinit(void)
	{
		MapManagerUtils::staticDeinit();
	}

	void MapManager::setLogMaxSeverity(plog::Severity sev)
	{
		plog::get()->setMaxSeverity(sev);
		PLOG(plog::get()->getMaxSeverity()) << "Log severity changed to: " << std::to_string(sev);
	}

	std::string MapManager::getMapName(void)
	{
		if (g_mapName.size() == 0)
			throw(MapManagerException(__FUNCTION__, string("MapManager not initialized!").c_str()));
		return g_mapName;
	}

	float MapManager::gridStepInWU(void)
	{
		if (g_gridStepInWU == 0.0f)
			throw(MapManagerException(__FUNCTION__, string("MapManager not initialized!").c_str()));
		return g_gridStepInWU;
	}

	__int64 MapManager::addWD(WorldDefiner& WD)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode());
		if (instrumented()) clock.tick();

		// we have to find all the vertices affected by AOE according to the fact that the map can grow with square map of point of g_DBGrowingBlockVertexNumber vertices
		float minAOEX = WD.getPosX() - WD.getAOE();
		float maxAOEX = WD.getPosX() + WD.getAOE();
		float minAOEZ = WD.getPosZ() - WD.getAOE();
		float maxAOEZ = WD.getPosZ() + WD.getAOE();

		int minGridPosX = 0;
		int maxGridPosX = 0;
		int minGridPosZ = 0;
		int maxGridPosZ = 0;

		float gridStepInWU = 0.0;

		// we need to calculate the grid so that it is expressed of square patches with a number of vertices for every size equal to g_DBGrowingBlockVertexNumber
		// they are spaced by a number of WU equal to gridStepInWU (MapManager::gridStepInWU())
		calcSquareFlatGridMinMaxToExpand(minAOEX, maxAOEX, minAOEZ, maxAOEZ, minGridPosX, maxGridPosX, minGridPosZ, maxGridPosZ, gridStepInWU);

		size_t gridSize = (maxGridPosX - minGridPosX + 1) * (maxGridPosZ - minGridPosZ + 1);
		vector<SQLInterface::GridVertex> v;
		v.resize(gridSize);
		
		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Computing affected vertices by WorldDefiner: ");

		int numVertices = 0, idx = 0;
		for (int z = minGridPosZ; z <= maxGridPosZ; z++)
		{
			for (int x = minGridPosX; x <= maxGridPosX; x++)
			{
				// guard
				{
					numVertices++;
					assert(numVertices <= gridSize);
				}

				SQLInterface::GridVertex gridVertex(float(x) * gridStepInWU, float(z) * gridStepInWU, WD.getLevel());
				//v.push_back(gridVertex);
				v[idx] = gridVertex;
				idx++;

				if (consoleDebugMode() && fmod(numVertices, 1024 * 1000) == 0) _consoleDebugUtil.printVariablePartOfLine(numVertices);
			}
		}

		if (consoleDebugMode()) _consoleDebugUtil.printVariablePartOfLine(numVertices);

		// Adding / updating WD to DB : this action will add / update all affected point
		__int64 rowid = m_SqlInterface->addWDAndVertices(&WD, v);

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (consoleDebugMode()) _consoleDebugUtil.printNewLine();

		return rowid;
	}

	// Return a square grid with a number of vertices for every size multiple of g_DBGrowingBlockVertexNumber, they are spaced by a number of WU equal to MapManager::gridStepInWU()
	// every point of the grid is defined by its X and Z coord expressed in WU in whole numbers 
	void MapManager::calcSquareFlatGridMinMaxToExpand(float minXInWUs, float maxXInWUs, float minZInWUs, float maxZInWUs, int& minGridPosX, int& maxGridPosX, int& minGridPosZ, int& maxGridPosZ, float& gridStepInWU)
	{
		gridStepInWU = MapManager::gridStepInWU();

		float GrowingBlockInWU = g_DBGrowingBlockVertexNumber * gridStepInWU;
		
		//minAOEX = -10.0;
		//maxAOEX = 10.0;
		//minAOEZ = -10.0;
		//maxAOEZ = 10.0;

		if (minXInWUs > 0)
		{
			if (minXInWUs < GrowingBlockInWU)
				minXInWUs = 0;
		}
		else if (minXInWUs < 0)
		{
			if (abs(minXInWUs) < GrowingBlockInWU)
				minXInWUs = -GrowingBlockInWU;
		}


		if (maxXInWUs > 0)
		{
			if (maxXInWUs < GrowingBlockInWU)
				maxXInWUs = GrowingBlockInWU;
		}
		else if (maxXInWUs < 0)
		{
			if (abs(maxXInWUs) < GrowingBlockInWU)
				maxXInWUs = 0;
		}

		if (minZInWUs > 0)
		{
			if (minZInWUs < GrowingBlockInWU)
				minZInWUs = 0;
		}
		else if (minZInWUs < 0)
		{
			if (abs(minZInWUs) < GrowingBlockInWU)
				minZInWUs = -GrowingBlockInWU;
		}

		if (maxZInWUs > 0)
		{
			if (maxZInWUs < GrowingBlockInWU)
				maxZInWUs = GrowingBlockInWU;
		}
		else if (maxZInWUs < 0)
		{
			if (abs(maxZInWUs) < GrowingBlockInWU)
				maxZInWUs = 0;
		}

		float minPosX = floorf(minXInWUs / GrowingBlockInWU) * GrowingBlockInWU;
		float maxPosX = floorf(maxXInWUs / GrowingBlockInWU) * GrowingBlockInWU;
		if (maxPosX < maxXInWUs)
			maxPosX += GrowingBlockInWU;
		float minPosZ = floorf(minZInWUs / GrowingBlockInWU) * GrowingBlockInWU;
		float maxPosZ = floorf(maxZInWUs / GrowingBlockInWU) * GrowingBlockInWU;
		if (maxPosZ < maxZInWUs)
			maxPosZ += GrowingBlockInWU;

		minGridPosX = int(minPosX / gridStepInWU);
		maxGridPosX = int(maxPosX / gridStepInWU);
		minGridPosZ = int(minPosZ / gridStepInWU);
		maxGridPosZ = int(maxPosZ / gridStepInWU);
	}
	
	// return a square grid as a vector of GridPoint stepped by gridStepInWU (MapManager::gridStepInWU()) which is in size numPointX x numPointZ and placed as a sequence of rows (a row incrementng z)
	void MapManager::getSquareFlatGridToExpand(float minXInWUs, float maxXInWUs, float minZInWUs, float maxZInWUs, vector<FlatGridPoint>& grid, int& numPointX, int& numPointZ, float& gridStepInWU)
	{
		int minGridPosX = 0;
		int maxGridPosX = 0;
		int minGridPosZ = 0;
		int maxGridPosZ = 0;

		calcSquareFlatGridMinMaxToExpand(minXInWUs, maxXInWUs, minZInWUs, maxZInWUs, minGridPosX, maxGridPosX, minGridPosZ, maxGridPosZ, gridStepInWU);

		numPointX = maxGridPosX - minGridPosX + 1;
		numPointZ = maxGridPosZ - minGridPosZ + 1;

		grid.clear();
		grid.reserve(size_t(numPointX) * size_t(numPointZ));

		for (int z = minGridPosZ; z <= maxGridPosZ; z++)
		{
			for (int x = minGridPosX; x <= maxGridPosX; x++)
			{
				FlatGridPoint p;
				p.x = float(x) * gridStepInWU;
				p.z = float(z) * gridStepInWU;
				grid.push_back(p);
			}
		}
		assert(grid.size() == size_t(numPointX) * size_t(numPointZ));
	}

	void MapManager::getFlatGrid(float minXInWUs, float maxXInWUs, float minZInWUs, float maxZInWUs, vector<FlatGridPoint>& grid, int& numPointX, int& numPointZ, float& gridStepInWU)
	{
		gridStepInWU = MapManager::gridStepInWU();

		float f = calcPreviousCoordOnTheGridInWUs(minXInWUs);
		float minGridPosX = f / gridStepInWU;
		f = calcNextCoordOnTheGridInWUs(maxXInWUs);
		float maxGridPosX = f / gridStepInWU;
		float minGridPosZ = calcPreviousCoordOnTheGridInWUs(minZInWUs) / gridStepInWU;
		float maxGridPosZ = calcNextCoordOnTheGridInWUs(maxZInWUs) / gridStepInWU;

		numPointX = int(maxGridPosX - minGridPosX + 1);
		numPointZ = int(maxGridPosZ - minGridPosZ + 1);

		getFlatGrid(minGridPosX, minGridPosZ, numPointX, numPointZ, grid, gridStepInWU);
	}

	void MapManager::getFlatGrid(float minXInWUs, float minZInWUs, int numPointX, int numPointZ, vector<FlatGridPoint>& grid, float& gridStepInWU)
	{
		gridStepInWU = MapManager::gridStepInWU();

		//float maxX = minXInWUs + numPointX - 1;
		//float maxZ = minZInWUs + numPointZ - 1;

		grid.clear();
		//grid.reserve(size_t(numPointX) * size_t(numPointZ));
		grid.resize(size_t(numPointX) * size_t(numPointZ));

		size_t idx = 0;
		for (int z = 0; z < numPointZ; z++)
		{
			float incZ = minZInWUs + z * gridStepInWU;
			for (int x = 0; x < numPointX; x++)
			{
				FlatGridPoint p;
				p.x = minXInWUs + x * gridStepInWU;
				p.z = incZ;
				//grid.push_back(p);
				grid[idx] = p;
				idx++;
			}
		}
		assert(grid.size() == size_t(numPointX) * size_t(numPointZ));
	}

	void MapManager::getEmptyVertexGrid(vector<FlatGridPoint>& grid, vector<SQLInterface::GridVertex>& emptyGridVertex, int level)
	{
		emptyGridVertex.clear();
		emptyGridVertex.resize(grid.size());
		vector<FlatGridPoint>::iterator it;
		size_t idx = 0;
		for (it = grid.begin(); it != grid.end(); it++)
		{
			SQLInterface::GridVertex gridVertex(it->x, it->z, level);
			//emptyGridVertex.push_back(gridVertex);
			emptyGridVertex[idx] = gridVertex;
			idx++;
		}
	}

	bool MapManager::eraseWD(__int64 wdRowid)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode());
		if (instrumented()) clock.tick();

		bool bDeleted = m_SqlInterface->eraseWD(wdRowid);

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (consoleDebugMode()) _consoleDebugUtil.printNewLine();

		return bDeleted;
	}

	bool MapManager::eraseWD(float posX, float posZ, int level, WDType type)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode());
		if (instrumented()) clock.tick();
		bool bDeleted = false;

		WorldDefiner wd;
		bool bFound = m_SqlInterface->getWD(posX, posZ, level, type, wd);
		if (bFound)
			bDeleted = m_SqlInterface->eraseWD(wd.getRowid());

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (consoleDebugMode()) _consoleDebugUtil.printNewLine();

		return bDeleted;
	}

	bool MapManager::eraseWD(WorldDefiner& WD)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode());
		if (instrumented()) clock.tick();

		bool bDeleted = eraseWD(WD.getPosX(), WD.getPosZ(), WD.getLevel(), WD.getType());

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (consoleDebugMode()) _consoleDebugUtil.printNewLine();

		return bDeleted;
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
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode());
		if (instrumented()) clock.tick();

		/*
		* Open Transaction
		*/
		m_SqlInterface->beginTransaction();

		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Updating vertices marked for update: ");
		int updated = 0;
		int idx = 0;
		SQLInterface::GridVertex gridVertex;
		std::vector<WorldDefiner> vectWD;
		std::map<WorldDefiner, std::vector<SQLInterface::GridVertex>> mapOfVerticesPerWD;
		bool bFound = m_SqlInterface->getFirstModfiedVertex(gridVertex, vectWD);
		while (bFound)
		{
			if (gridVertex.posX() == 24 && gridVertex.posZ() == 24)
				DebugBreak();

			idx++;

			float altitude = computeAltitude(gridVertex, vectWD);
			gridVertex.setAltitude(altitude);
			m_SqlInterface->updateAltitudeOfVertex(gridVertex.rowid(), altitude);

			updated++;
			if (consoleDebugMode() && fmod(idx, 1000) == 0)
			{
				string s = "Vertices marked for update: ";	s += to_string(idx);	s += " - Vertices Updated: ";	s += to_string(updated);
				_consoleDebugUtil.printVariablePartOfLine(s.c_str());
			}

			// saving vertices ordered by wd
			for (auto& wd : vectWD)
				if (wd.getType() == WDType::flattener)
					mapOfVerticesPerWD[wd].push_back(gridVertex);
			
			vectWD.clear();
			bFound = m_SqlInterface->getNextModfiedVertex(gridVertex, vectWD);
		}

		std::map<SQLInterface::GridVertex, float> grideVerticesToUpdateForFlattening;
		for (auto& mapItem : mapOfVerticesPerWD)
		{
			if (mapItem.first.getType() == WDType::flattener)
			{
				// approximated WD pos on the grid of vertices
				float wdX = calcPreviousCoordOnTheGridInWUs(mapItem.first.getPosX());
				float wdZ = calcPreviousCoordOnTheGridInWUs(mapItem.first.getPosZ());

				// current approximated altitude of wd
				float wdAltitude = 0.0;

				// looking for a vertex coincident with approximated wd pos in the array of vertices
				SQLInterface::GridVertex v(wdX, wdZ, mapItem.first.getLevel());
				std::vector<SQLInterface::GridVertex>::iterator it = std::find(mapItem.second.begin(), mapItem.second.end(), v);
				if (it != mapItem.second.end())
					wdAltitude = it->altitude();
				else
				{
					m_SqlInterface->getVertex(v);
					wdAltitude = v.altitude();
				}

				for (auto& vertex : mapItem.second)
				{
					if (vertex.posX() == 24 && vertex.posZ() == 24)
						DebugBreak();

					float altitude = wdAltitude + computeAltitudeModifier(vertex, mapItem.first, -1, wdAltitude);
					vertex.setAltitude(altitude);	// actually useless
					grideVerticesToUpdateForFlattening[vertex] = altitude;
				}
			}
		}

		for (auto& v : grideVerticesToUpdateForFlattening)
		{
			m_SqlInterface->updateAltitudeOfVertex(v.first.rowid(), v.second);
		}

		if (consoleDebugMode())
		{
			string s = "Vertices marked for update: ";	s += to_string(idx);	s += " - Vertices Updated: ";	s += to_string(updated);
			_consoleDebugUtil.printVariablePartOfLine(s.c_str());
		}

		m_SqlInterface->clearVerticesMarkedForUpdate();
			
		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (consoleDebugMode()) _consoleDebugUtil.printNewLine();

		/*
		* Close Transaction
		*/
		m_SqlInterface->endTransaction();
	}

	float MapManager::computeAltitude(SQLInterface::GridVertex& gridVertex, std::vector<WorldDefiner>& wdMap)
	{
		float altitude = gridVertex.initialAltitude();
		
		// Scanning all WDs
		int numWDs = (int)wdMap.size();
		float distanceFromWD = -1;
		for (int idx = 0; idx < numWDs; idx++)
		{
			float AOE = wdMap[idx].getAOE();
			distanceFromWD = getDistance(gridVertex.posX(), gridVertex.posZ(), wdMap[idx].getPosX(), wdMap[idx].getPosZ());
			if (distanceFromWD <= AOE)	// Vertex is influenced by current WD
			{
				switch (wdMap[idx].getType())
				{
				case WDType::elevator:
					altitude += computeAltitudeModifier(gridVertex, wdMap[idx], distanceFromWD);
					break;
				case WDType::depressor:
					altitude -= computeAltitudeModifier(gridVertex, wdMap[idx], distanceFromWD);
					break;
				default:
					break;
				}
			}
		}

		return altitude;
	}

	float MapManager::computeAltitudeModifier(SQLInterface::GridVertex& gridVertex, const WorldDefiner& wd, float distanceFromWD, float wdAltitude)
	{
		float altitudeModifier = 0.0;

		float AOE = wd.getAOE();

		if (distanceFromWD == -1)
			distanceFromWD = getDistance(gridVertex.posX(), gridVertex.posZ(), wd.getPosX(), wd.getPosZ());
		
		if (distanceFromWD > AOE)
			return altitudeModifier;
		
		if (wd.getType() == WDType::elevator || wd.getType() == WDType::depressor)
		{
			switch (wd.getFunctionType())
			{
				case WDFunctionType::MaxEffectOnWD:
				{
					float d = distanceFromWD / AOE;			// from 0 (on WD) to 1 (on border of AOE)
					float argument = d * (float)M_PI_2;		// from 0 (on WD) to M_PI_2 (on border of AOE)
					altitudeModifier = cosf(argument);		// from 1 (on WD) to 0 (on border of AOE)
					altitudeModifier *= wd.getStrength();	// from wd.getStrength() (on WD) to 0 (on border of AOE)
					//altitude = cosf( (distanceFromWD / wd.getAOE()) * (float)M_PI_2 ) * wd.getStrength();
				}
				break;
				case WDFunctionType::MinEffectOnWD:
				{
					float d = distanceFromWD / AOE;			// from 0 (on WD) to 1 (on border of AOE)
					float argument = d * (float)M_PI_2;		// from 0 (on WD) to M_PI_2 (on border of AOE)
					altitudeModifier = sinf(argument);				// from 0 (on WD) to 1 (on border of AOE)
					altitudeModifier *= wd.getStrength();			// from wd.getStrength() (on WD) to 0 (on border of AOE)
					//altitude = cosf( (distanceFromWD / wd.getAOE()) * (float)M_PI_2 ) * wd.getStrength();
				}
				break;
			default:
				break;
			}
		}
		else if (wd.getType() == WDType::flattener)
		{
			float unaffectedAltitude = gridVertex.altitude() - wdAltitude;
			float coeffForDistance = distanceFromWD / AOE;					// from 0 (on wd) to 1 (on border of AOE)
			altitudeModifier = unaffectedAltitude * coeffForDistance;	// from 0 (on wd) to unaffectedAltitude (on border of AOE = unaffecting altitude summing to wd altitude)
			
			float strengthModifier = 1 - wd.getStrength();					// force 0 modifier from wd altitude when strength is max (=1), leave altitudeModifier unaffected (multiplying by 1) when strength is 0
			altitudeModifier *= strengthModifier;
		}

		return altitudeModifier;
	}

	int MapManager::getNumVertexMarkedForUpdate(void)
	{
		SQLInterface::GridVertex gridVertex;
		vector<WorldDefiner> vectWD;
		bool bFound = m_SqlInterface->getFirstModfiedVertex(gridVertex, vectWD);
		int idx = 0;
		while (bFound)
		{
			idx++;
			bFound = m_SqlInterface->getNextModfiedVertex(gridVertex, vectWD);
		}
		return idx;
	}
	
	void MapManager::DumpDB(void)
	{
		// RMTODO
	}

	float MapManager::calcPreviousCoordOnTheGridInWUs(float coordInWUs)
	{
		float f = floorf(coordInWUs / MapManager::gridStepInWU()) * MapManager::gridStepInWU();
		return f;
	}

	float MapManager::calcNextCoordOnTheGridInWUs(float coordInWUs)
	{
		float nextCoord = floorf(coordInWUs / MapManager::gridStepInWU()) * MapManager::gridStepInWU();
		if (nextCoord < coordInWUs)
			return nextCoord + MapManager::gridStepInWU();
		else
			return coordInWUs;
	}

	void MapManager::getVertices(float anchorXInWUs, float anchorZInWUs, anchorType type, float size, vector<SQLInterface::GridVertex>& mesh, int& numPointX, int& numPointZ, float& gridStepInWU, int level)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode());
		if (instrumented()) clock.tick();

		//limiter l(2);
		//std::lock_guard<std::recursive_mutex> lock(s_mtxInternalData);

		float min_X_OnTheGridInWUs, max_X_OnTheGridInWUs, min_Z_OnTheGridInWUs, max_Z_OnTheGridInWUs;

		if (type == anchorType::center)
		{
			min_X_OnTheGridInWUs = calcPreviousCoordOnTheGridInWUs(anchorXInWUs - size / 2);
			max_X_OnTheGridInWUs = calcNextCoordOnTheGridInWUs(anchorXInWUs + size / 2);
			min_Z_OnTheGridInWUs = calcPreviousCoordOnTheGridInWUs(anchorZInWUs - size / 2);
			max_Z_OnTheGridInWUs = calcNextCoordOnTheGridInWUs(anchorZInWUs + size / 2);
		}
		else if (type == anchorType::upperleftcorner)
		{
			min_X_OnTheGridInWUs = calcPreviousCoordOnTheGridInWUs(anchorXInWUs);
			max_X_OnTheGridInWUs = calcNextCoordOnTheGridInWUs(anchorXInWUs + size);
			min_Z_OnTheGridInWUs = calcPreviousCoordOnTheGridInWUs(anchorZInWUs);
			max_Z_OnTheGridInWUs = calcNextCoordOnTheGridInWUs(anchorZInWUs + size);
		}
		else
			throw(MapManagerException(__FUNCTION__, string("Unkwon anchor type").c_str()));

		anchorXInWUs = calcNextCoordOnTheGridInWUs(anchorXInWUs);
		anchorZInWUs = calcNextCoordOnTheGridInWUs(anchorZInWUs);

		int numFoundInDB = 0;
		
		internalGetVertices(min_X_OnTheGridInWUs, max_X_OnTheGridInWUs, min_Z_OnTheGridInWUs, max_Z_OnTheGridInWUs, mesh, numPointX, numPointZ, gridStepInWU, numFoundInDB, level);

		if (instrumented()) clock.printDuration((string(__FUNCTION__) + " - Num Vertices: " + to_string(mesh.size()) + " - (X x Z): " + to_string(numPointX) + " x " + to_string(numPointZ) + " - Found in DB: " + to_string(numFoundInDB)).c_str());
	}
	
	void MapManager::internalGetVertices(float min_X_OnTheGridInWUs, float max_X_OnTheGridInWUs, float min_Z_OnTheGridInWUs, float max_Z_OnTheGridInWUs, vector<SQLInterface::GridVertex>& mesh, int& numPointX, int& numPointZ, float& gridStepInWU, int& numFoundInDB, int level)
	{
		vector<FlatGridPoint> grid;
		getFlatGrid(min_X_OnTheGridInWUs, max_X_OnTheGridInWUs, min_Z_OnTheGridInWUs, max_Z_OnTheGridInWUs, grid, numPointX, numPointZ, gridStepInWU);
		getEmptyVertexGrid(grid, mesh, level);

		vector<SQLInterface::GridVertex> vectGridVerticesFromDB;
		m_SqlInterface->getVertices(min_X_OnTheGridInWUs, max_X_OnTheGridInWUs, min_Z_OnTheGridInWUs, max_Z_OnTheGridInWUs, vectGridVerticesFromDB, level);

		std::sort(vectGridVerticesFromDB.begin(), vectGridVerticesFromDB.end());
		std::sort(mesh.begin(), mesh.end());

		vector<SQLInterface::GridVertex>::iterator itGridFromDB = vectGridVerticesFromDB.begin();
		vector<SQLInterface::GridVertex>::iterator itMesh = mesh.begin();
		for (;;)
		{
			if (itMesh == mesh.end())
				break;

			if (itGridFromDB == vectGridVerticesFromDB.end())
				break;

			if (*itMesh == *itGridFromDB)
			{
				*itMesh = *itGridFromDB;
				itGridFromDB++;
				itMesh++;
				numFoundInDB++;
			}
			else if (*itMesh < *itGridFromDB)
			{
				itMesh++;
				continue;
			}
			else
			{
				while (itGridFromDB != vectGridVerticesFromDB.end() && !(*itMesh < *itGridFromDB))
					itGridFromDB++;

				if (itGridFromDB == vectGridVerticesFromDB.end())
					break;

				if (*itMesh == *itGridFromDB)
				{
					*itMesh = *itGridFromDB;
					itGridFromDB++;
					itMesh++;
					numFoundInDB++;
				}
			}
		}
	}

	void MapManager::uploadCacheBuffer(float lowerXGridVertex, float lowerZGridVertex, int numVerticesPerSize, float& gridStepInWU, int level, std::string& meshBuffer)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("UploadBuffer 1b ") + __FUNCTION__, "uploadCacheBuffer");

		std::lock_guard<std::recursive_mutex> lock(s_cacheMtx);
		
		TheWorld_Utils::MeshCacheBuffer cache;
		std::string cacheDir = m_SqlInterface->dataPath();
		std::string mapName = getMapName();
		cache = TheWorld_Utils::MeshCacheBuffer(cacheDir, mapName, gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);
		cache.writeBufferToDiskCache(meshBuffer, true);
		bool stop = false;
		//writeDiskCacheToDB(cache, stop, writeCompactVerticesToDB);
	}

	bool MapManager::writeDiskCacheToDB(TheWorld_Utils::MeshCacheBuffer& cache, bool& stop, bool writeCompactVerticesToDB)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("writeDiskCacheToDB ") + __FUNCTION__, "ALL");

		PLOG_DEBUG << "MapManager::writeDiskCacheToDB - Compact ==> " << writeCompactVerticesToDB;

		bool emptyBuffer = cache.isEmptyBuffer(cache.getMeshIdFromDisk());
		
		TheWorld_Utils::MemoryBuffer memoryBuffer;
		cache.readBufferFromDisk(cache.getMeshIdFromDisk(), memoryBuffer);
		std::string strBuffer;
		
		TheWorld_Utils::MemoryBuffer terrainEditValues;
		TheWorld_Utils::MemoryBuffer heights16Buffer;
		TheWorld_Utils::MemoryBuffer heights32Buffer;
		TheWorld_Utils::MemoryBuffer normalsBuffer;
		TheWorld_Utils::MemoryBuffer splatmapBuffer;
		TheWorld_Utils::MemoryBuffer colormapBuffer;
		TheWorld_Utils::MemoryBuffer globalmapBuffer;
		TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData cacheQuadrantData;
		cacheQuadrantData.meshId = cache.getMeshIdFromDisk();
		cacheQuadrantData.terrainEditValues = &terrainEditValues;
		cacheQuadrantData.heights16Buffer = &heights16Buffer;
		cacheQuadrantData.heights32Buffer = &heights32Buffer;
		cacheQuadrantData.normalsBuffer = &normalsBuffer;
		cacheQuadrantData.splatmapBuffer = &splatmapBuffer;
		cacheQuadrantData.colormapBuffer = &colormapBuffer;
		cacheQuadrantData.globalmapBuffer = &globalmapBuffer;
		
		bool completed;

		std::recursive_mutex& exclusiveDBAccessMutex = m_SqlInterface->getExclusiveDBAccessMutex();

		std::lock_guard<std::recursive_mutex> lock(exclusiveDBAccessMutex);

		if (emptyBuffer)
		{
			SQLInterface::QuadrantVertexStoreType vertexStoreType = writeCompactVerticesToDB ? SQLInterface::QuadrantVertexStoreType::Compact : SQLInterface::QuadrantVertexStoreType::eXtended;
			completed = m_SqlInterface->writeQuadrantToDB(cache, cacheQuadrantData, strBuffer, vertexStoreType, stop);
		}
		else
		{
			if (writeCompactVerticesToDB)
			{
				//size_t size = memoryBuffer.size();
				//strBuffer.clear();
				//strBuffer.reserve(size);
				//strBuffer.append((char*)memoryBuffer.ptr(), size);
				memoryBuffer.toString(strBuffer);
			}

			cache.refreshCacheQuadrantDataFromBuffer(memoryBuffer, cacheQuadrantData, false);

			SQLInterface::QuadrantVertexStoreType vertexStoreType = writeCompactVerticesToDB ? SQLInterface::QuadrantVertexStoreType::Compact : SQLInterface::QuadrantVertexStoreType::eXtended;
			completed = m_SqlInterface->writeQuadrantToDB(cache, cacheQuadrantData, strBuffer, vertexStoreType, stop);
		}
		
		return completed;
	}

	bool MapManager::writeDiskCacheFromDB(TheWorld_Utils::MeshCacheBuffer& cache, bool& stop)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("writeDiskCacheFromDB ") + __FUNCTION__, "ALL");

		std::string meshId;
		TheWorld_Utils::TerrainEdit terrainEdit;
		enum class SQLInterface::QuadrantStatus status;
		enum class SQLInterface::QuadrantVertexStoreType vertexStoreType;
		std::string strBuffer;
		m_SqlInterface->readQuadrantFromDB(cache, meshId, strBuffer, status, vertexStoreType, terrainEdit);

		if (status == SQLInterface::QuadrantStatus::Loading)
			return false;

		if (meshId.size() == 0)
			return false;
		
		if (vertexStoreType == SQLInterface::QuadrantVertexStoreType::Compact)
		{
			cache.writeBufferToDiskCache(strBuffer);
		}
		else
		{
			TheWorld_Utils::MemoryBuffer heights16Buffer;
			TheWorld_Utils::MemoryBuffer heights32Buffer;
			TheWorld_Utils::MemoryBuffer normalsBuffer;
			TheWorld_Utils::MemoryBuffer splatmapBuffer;
			TheWorld_Utils::MemoryBuffer colormapBuffer;
			TheWorld_Utils::MemoryBuffer globalmapBuffer;

			float gridStep = cache.getGridStepInWU();
			size_t vertexPerSize = cache.getNumVerticesPerSize();
			int level = cache.getLevel();
			float quadPosX = cache.getLowerXGridVertex();
			float quadPosZ = cache.getLowerZGridVertex();
			float quadEndPosX = quadPosX + (vertexPerSize - 1) * gridStep;
			float quadEndPosZ = quadPosZ + (vertexPerSize - 1) * gridStep;

			int numFoundInDB = 0;
			std::vector<SQLInterface::GridVertex> vectGridVertex;
			internalGetVertices(quadPosX, quadEndPosX, quadPosZ, quadEndPosZ, (int)vertexPerSize, (int)vertexPerSize, vectGridVertex, gridStep, numFoundInDB, level);
			size_t numVertices = vectGridVertex.size();
			if (numVertices != 0 && numVertices != vertexPerSize * vertexPerSize)
				throw(MapManagerException(__FUNCTION__, (std::string("internalGetVertices returned unexpected size: ") + std::to_string(numVertices) + "(expected" + std::to_string(vertexPerSize * vertexPerSize) + ")").c_str()));

			TheWorld_Utils::MemoryBuffer buffer;

			float minHeight = FLT_MAX, maxHeight = -FLT_MAX;

			if (numFoundInDB > 0 && numVertices > 0)
			{
				heights16Buffer.reserve(numVertices * sizeof(uint16_t));
				uint16_t* _tempHeights16BufferPtr = (uint16_t*)heights16Buffer.ptr();
				heights32Buffer.reserve(numVertices * sizeof(float));
				float* _tempHeights32BufferPtr = (float*)heights32Buffer.ptr();
				normalsBuffer.reserve(numVertices * sizeof(TheWorld_Utils::_RGB));
				TheWorld_Utils::_RGB* _tempNormalsBufferPtr = (TheWorld_Utils::_RGB*)normalsBuffer.ptr();
				splatmapBuffer.reserve(numVertices * sizeof(TheWorld_Utils::_RGBA));
				TheWorld_Utils::_RGBA* _tempSplatmapBufferPtr = (TheWorld_Utils::_RGBA*)splatmapBuffer.ptr();
				colormapBuffer.reserve(numVertices * sizeof(TheWorld_Utils::_RGBA));
				TheWorld_Utils::_RGBA* _tempColormapBufferPtr = (TheWorld_Utils::_RGBA*)colormapBuffer.ptr();
				globalmapBuffer.reserve(numVertices * sizeof(TheWorld_Utils::_RGB));
				TheWorld_Utils::_RGB* _tempGlobalmapBufferPtr = (TheWorld_Utils::_RGB*)globalmapBuffer.ptr();

				bool normalMapEmpty = false, splatMapEmpty = false, colorMapEmpty = false, globalMapEmpty = false;

				size_t x = 0, z = 0;
				for (auto& gridVertex : vectGridVertex)
				{
					if (stop)
						return false;

					TheWorld_Utils::FLOAT_32 f;
					f.f32 = gridVertex.altitude();

					if (f.f32 < minHeight)
						minHeight = f.f32;

					if (f.f32 > maxHeight)
						maxHeight = f.f32;

					*_tempHeights16BufferPtr = TheWorld_Utils::MeshCacheBuffer::halfFromFloat(f.u32);
					_tempHeights16BufferPtr++;

					*_tempHeights32BufferPtr = f.f32;
					_tempHeights32BufferPtr++;

					if (!normalMapEmpty)
					{
						if (gridVertex.normX() == 0 && gridVertex.normY() == 0 && gridVertex.normZ() == 0)
							normalMapEmpty = true;
						else
						{
							Eigen::Vector3d normal(gridVertex.normX(), gridVertex.normY(), gridVertex.normZ());
							Eigen::Vector3d packedNormal = TheWorld_Utils::packNormal(normal);
							(*_tempNormalsBufferPtr).r = (BYTE)(packedNormal.x() * 255);	// normals coords range from 0 to 1 but if expressed as color in a normalmap range from 0 to 255
							(*_tempNormalsBufferPtr).g = (BYTE)(packedNormal.y() * 255);
							(*_tempNormalsBufferPtr).b = (BYTE)(packedNormal.z() * 255);
							_tempNormalsBufferPtr++;
						}
					}

					if (!splatMapEmpty)
					{
						if (gridVertex.lowElevationTexAmount() == -1 && gridVertex.highElevationTexAmount() == -1 && gridVertex.dirtTexAmount() == -1 && gridVertex.rocksTexAmount() == -1)
							splatMapEmpty = true;
						else
						{
							(*_tempSplatmapBufferPtr).r = BYTE(gridVertex.lowElevationTexAmount());
							(*_tempSplatmapBufferPtr).g = BYTE(gridVertex.highElevationTexAmount());
							(*_tempSplatmapBufferPtr).b = BYTE(gridVertex.dirtTexAmount());
							(*_tempSplatmapBufferPtr).a = BYTE(gridVertex.rocksTexAmount());
							_tempSplatmapBufferPtr++;
						}
					}

					if (!colorMapEmpty)
					{
						if (gridVertex.colorR() == -1 && gridVertex.colorG() == -1 && gridVertex.colorB() == -1 && gridVertex.colorA() == -1)
							colorMapEmpty = true;
						else
						{
							(*_tempColormapBufferPtr).r = BYTE(gridVertex.colorR());
							(*_tempColormapBufferPtr).g = BYTE(gridVertex.colorG());
							(*_tempColormapBufferPtr).b = BYTE(gridVertex.colorB());
							(*_tempColormapBufferPtr).a = BYTE(gridVertex.colorA());
							_tempColormapBufferPtr++;
						}
					}

					if (!globalMapEmpty)
					{
						if (gridVertex.globalMapR() == -1 && gridVertex.globalMapG() == -1 && gridVertex.globalMapB() == -1)
							globalMapEmpty = true;
						else
						{
							(*_tempGlobalmapBufferPtr).r = BYTE(gridVertex.globalMapR());
							(*_tempGlobalmapBufferPtr).g = BYTE(gridVertex.globalMapG());
							(*_tempGlobalmapBufferPtr).b = BYTE(gridVertex.globalMapB());
							_tempGlobalmapBufferPtr++;
						}
					}

					x++;
					if (x == vertexPerSize)
					{
						x = 0;
						z++;
					}
				}

				if (x != 0 && z != vertexPerSize)
					throw(MapManagerException(__FUNCTION__, (std::string("something wrong iterating grid vertices: x=") + std::to_string(x) + " z=" + std::to_string(z)).c_str()));

				heights16Buffer.adjustSize(numVertices * sizeof(uint16_t));
				heights32Buffer.adjustSize(numVertices * sizeof(float));
				if (normalMapEmpty)
					normalsBuffer.clear();
				else
					normalsBuffer.adjustSize(numVertices * sizeof(TheWorld_Utils::_RGB));
				if (splatMapEmpty)
					splatmapBuffer.clear();
				else
					splatmapBuffer.adjustSize(numVertices * sizeof(TheWorld_Utils::_RGBA));
				if (colorMapEmpty)
					colormapBuffer.clear();
				else
					colormapBuffer.adjustSize(numVertices * sizeof(TheWorld_Utils::_RGBA));
				if (globalMapEmpty)
					globalmapBuffer.clear();
				else
					globalmapBuffer.adjustSize(numVertices * sizeof(TheWorld_Utils::_RGB));

				TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData cacheQuadrantData;
				cacheQuadrantData.meshId = meshId;

				if (heights16Buffer.size() > 0)
					cacheQuadrantData.heights16Buffer = &heights16Buffer;

				if (heights32Buffer.size() > 0)
					cacheQuadrantData.heights32Buffer = &heights32Buffer;

				if (normalsBuffer.size() > 0)
					cacheQuadrantData.normalsBuffer = &normalsBuffer;
				else
					terrainEdit.normalsNeedRegen = true;

				if (splatmapBuffer.size() > 0)
					cacheQuadrantData.splatmapBuffer = &splatmapBuffer;
				else
					terrainEdit.extraValues.splatmapNeedRegen = true;

				if (colormapBuffer.size() > 0)
					cacheQuadrantData.colormapBuffer = &colormapBuffer;
				else
					terrainEdit.extraValues.emptyColormap = true;

				if (globalmapBuffer.size() > 0)
					cacheQuadrantData.globalmapBuffer = &globalmapBuffer;
				else
					terrainEdit.extraValues.emptyGlobalmap = true;

				TheWorld_Utils::MemoryBuffer terrainEditValues;
				if (terrainEdit.size > 0)
				{
					terrainEdit.serialize(terrainEditValues);
					cacheQuadrantData.terrainEditValues = &terrainEditValues;
					cacheQuadrantData.minHeight = terrainEdit.minHeight;
					cacheQuadrantData.maxHeight = terrainEdit.maxHeight;
				}
				if (minHeight != FLT_MAX)
				{
					cacheQuadrantData.minHeight = minHeight;
					//terrainEdit.minHeight = cacheQuadrantData.minHeight;
				}
				if (maxHeight != -FLT_MAX)
				{
					cacheQuadrantData.maxHeight = maxHeight;
					//terrainEdit.maxHeight = cacheQuadrantData.maxHeight;
				}

				cache.setBufferFromCacheQuadrantData(cache.getNumVerticesPerSize(), cacheQuadrantData, buffer);
			}
			else
			{
				//if no vertices of the quadrant in DB we force empty quadrant meshid (with empty quadrant meshid only the hash is updated and cache and db are aligned)
				std::string emptyBufferMeshId;
				cache.setEmptyBuffer(cache.getNumVerticesPerSize(), emptyBufferMeshId, buffer);
				//writeDiskCacheToDB(cache, stop);
			}

			cache.writeBufferToDiskCache(buffer);
		}
		
		return true;

	}

#define MAPMANAGER_STOPPED				-1
#define MAPMANAGER_NOTHING_TO_DO		0
#define MAPMANAGER_UPDATED_CACHE		1
#define MAPMANAGER_UPDATED_DB			2
//#define MAPMANAGER_UPDATED_CACHE_AND_DB 3

	void MapManager::alignDiskCacheAndDB(bool isInEditor, size_t numVerticesPerSize, int level)
	{
		if (isInEditor)
			return;
		
		std::lock_guard<std::recursive_mutex> lock(s_initMapMtx);

		if (!s_mapInitMap.contains(numVerticesPerSize))
		{
			MapInitMapPerLevel m;
			s_mapInitMap[numVerticesPerSize] = m;
		}
		if (!s_mapInitMap[numVerticesPerSize].contains(level) || s_mapInitMap[numVerticesPerSize][level] == nullptr)
			s_mapInitMap[numVerticesPerSize][level] = new InitMap;

		s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThreadRequiredExit = false;
		s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThread = std::thread(&MapManager::alignDiskCacheAndDBTask, this, numVerticesPerSize, level);
	}

	void MapManager::stopAlignDiskCacheAndDBTasks(bool isInEditor)
	{
		if (isInEditor)
			return;

		std::lock_guard<std::recursive_mutex> lock(s_initMapMtx);

		for (auto& m : s_mapInitMap)
		{
			for (auto m1 : m.second)
			{
				stopAlignDiskCacheAndDBTask(m.first, m1.first);
			}
		}

		s_mapInitMap.clear();
	}

	void MapManager::sync(bool isInEditor, size_t numVerticesPerSize, float gridStepinWU, bool& foundUpdatedQuadrant, int& level, float& lowerXGridVertex, float& lowerZGridVertex)
	{
		foundUpdatedQuadrant = false;
		
		std::lock_guard<std::recursive_mutex> lock(s_mapQuadrantToUpdateMtx);

		QuadrantPos pos;

		for (auto& q : s_mapQuadrantToUpdate)
		{
			if (q.first.m_numVerticesPerSize == numVerticesPerSize && q.first.m_gridStepInWU == gridStepinWU)
			{
				pos = q.first;
				break;
			}
		}

		if (pos.initialized())
		{
			foundUpdatedQuadrant = true;
			level = pos.m_level;
			lowerXGridVertex = pos.m_lowerXGridVertex;
			lowerZGridVertex = pos.m_lowerZGridVertex;
			s_mapQuadrantToUpdate.erase(pos);
		}
	}

	void MapManager::stopAlignDiskCacheAndDBTask(size_t numVerticesPerSize, int level)
	{
		std::lock_guard<std::recursive_mutex> lock(s_initMapMtx);

		if (!s_mapInitMap.contains(numVerticesPerSize))
			return;

		if (!s_mapInitMap[numVerticesPerSize].contains(level))
			return;

		if (s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThread.joinable())
		{
			s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThreadRequiredExit = true;
			s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThread.join();
			delete s_mapInitMap[numVerticesPerSize][level];
			s_mapInitMap[numVerticesPerSize][level] = nullptr;
		}
	}

#define WAIT_TIME_BEFORE_NEW_SCAN	10000

	void MapManager::alignDiskCacheAndDBTask(size_t numVerticesPerSize, int level)
	{
		PLOG_DEBUG << "Align CACHE <==> DB - Start alignement thread";
		//return;

		while (!s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThreadRequiredExit)
		{
			float gridStepInWU = MapManager::gridStepInWU();
			std::string cacheDir = m_SqlInterface->dataPath();
			std::string mapName = getMapName();
			std::vector<TheWorld_Utils::MeshCacheBuffer> vectDiskCache;
			TheWorld_Utils::MeshCacheBuffer::getAllDiskCache(cacheDir, mapName, gridStepInWU, numVerticesPerSize, level, vectDiskCache);

			size_t progr = 0;
			if (vectDiskCache.size() > 0)
			{
				for (auto& cache : vectDiskCache)
				{
					progr++;

					if (s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThreadRequiredExit)
						break;
					
					TheWorld_Utils::MsTimePoint start = std::chrono::time_point_cast<TheWorld_Utils::MsTimePoint::duration>(std::chrono::system_clock::now());
					
					int ret = alignDiskCacheAndDB(cache, s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThreadRequiredExit, writeCompactVerticesToDB);
					if (ret != MAPMANAGER_NOTHING_TO_DO && ret != MAPMANAGER_STOPPED)
					{
						TheWorld_Utils::MsTimePoint now = std::chrono::time_point_cast<TheWorld_Utils::MsTimePoint::duration>(std::chrono::system_clock::now());
						PLOG_DEBUG << "Align CACHE <==> DB - Aligned cache element " << progr << "/" << vectDiskCache.size() << " - Elapsed " << (now - start).count() << " ms";
						
						if (ret == MAPMANAGER_UPDATED_CACHE)
						{
							std::lock_guard<std::recursive_mutex> lock(s_mapQuadrantToUpdateMtx);
							QuadrantPos pos(cache.getLowerXGridVertex(), cache.getLowerZGridVertex(), cache.getNumVerticesPerSize(), cache.getLevel(), cache.getGridStepInWU());
							if (s_mapQuadrantToUpdate.contains(pos))
								s_mapQuadrantToUpdate[pos]++;
							else
								s_mapQuadrantToUpdate[pos] = 1;
						}
						
						//s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThreadRequiredExit = true;
						//break;
					}
				}
			}

			TheWorld_Viewer_Utils::MsTimePoint start = std::chrono::time_point_cast<TheWorld_Viewer_Utils::MsTimePoint::duration>(std::chrono::system_clock::now());
			while (true)
			{
				if (s_mapInitMap[numVerticesPerSize][level]->m_alignCacheAndDbThreadRequiredExit)
					break;

				TheWorld_Viewer_Utils::MsTimePoint now = std::chrono::time_point_cast<TheWorld_Viewer_Utils::MsTimePoint::duration>(std::chrono::system_clock::now());
				long long elapsedWaiting = (now - start).count();
				if (elapsedWaiting > WAIT_TIME_BEFORE_NEW_SCAN)
					break;

				Sleep(50);
			}
		}

		PLOG_DEBUG << "Align CACHE <==> DB - Stop alignement thread";
	}

	int MapManager::alignDiskCacheAndDB(TheWorld_Utils::MeshCacheBuffer& cache, bool& stop, bool writeCompactVerticesToDB)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("alignCacheAndDB ") + __FUNCTION__, "Single CACHE");

		std::lock_guard<std::recursive_mutex> lock(s_cacheMtx);

		int ret = MAPMANAGER_NOTHING_TO_DO;

		std::string diskCacheMeshId = cache.getMeshIdFromDisk();
		enum class SQLInterface::QuadrantStatus status;
		enum class SQLInterface::QuadrantVertexStoreType vertexStoreType;
		std::string dbMeshId = m_SqlInterface->getQuadrantHash(cache.getGridStepInWU(), cache.getNumVerticesPerSize(), cache.getLevel(), cache.getLowerXGridVertex(), cache.getLowerZGridVertex(), status, vertexStoreType);
		if (status == SQLInterface::QuadrantStatus::Loading)
			dbMeshId = "";

		//size_t size = diskCacheMeshId.size();
		//const char* p = diskCacheMeshId.c_str();
		//size_t size1 = dbMeshId.size();
		//const char* p1 = dbMeshId.c_str();
		//size = memcmp(diskCacheMeshId.c_str(), dbMeshId.c_str(), size);

		if (diskCacheMeshId.size() == 0 && dbMeshId.size() == 0)
		{
			// no info of the terrain in cache nor in the db
			
			ret = MAPMANAGER_NOTHING_TO_DO;

			//TheWorld_Utils::MemoryBuffer tempBuffer;
			//
			//{
			//	TheWorld_Utils::GuardProfiler profiler(std::string("alignCacheAndDB 1.1 ") + __FUNCTION__, "Generate empty buffer");
			//	cache.setEmptyBuffer(cache.getNumVerticesPerSize(), diskCacheMeshId, tempBuffer);
			//}

			//{
			//	TheWorld_Utils::GuardProfiler profiler(std::string("alignCacheAndDB 1.2 ") + __FUNCTION__, "Write buffer to cache");
			//	cache.writeBufferToDiskCache(tempBuffer);
			//}
			//writeDiskCacheToDB(cache, stop);

			//ret = MAPMANAGER_UPDATED_CACHE_AND_DB;
		}
		else if (diskCacheMeshId.size() == 0)
		{
			// info of the terrain is in DB
			PLOG_DEBUG << "Align CACHE <==> DB - Start aligning " << cache.getCacheIdStr() << " DB ==> Cache";
			bool completed = writeDiskCacheFromDB(cache, stop);
			if (completed)
			{
				PLOG_DEBUG << "Align CACHE <==> DB - Alignement " << cache.getCacheIdStr() << " DB ==> Cache - Completed";
				ret = MAPMANAGER_UPDATED_CACHE;
			}
			else
				ret = MAPMANAGER_STOPPED;
		}
		else if (dbMeshId.size() == 0)
		{
			//bool emptyBuffer = cache.isEmptyBuffer(diskCacheMeshId);
			//
			//if (!emptyBuffer)
			//{
				// info of the terrain is in cache
				PLOG_DEBUG << "Align CACHE <==> DB - Start aligning " << cache.getCacheIdStr() << " Cache ==> DB";
				bool completed = writeDiskCacheToDB(cache, stop, writeCompactVerticesToDB);
				if (completed)
				{
					PLOG_DEBUG << "Align CACHE <==> DB - Alignement " << cache.getCacheIdStr() << " Cache ==> DB - Completed";
					ret = MAPMANAGER_UPDATED_DB;
				}
				else
					ret = MAPMANAGER_STOPPED;
			//}
			//else
			//	ret = MAPMANAGER_NOTHING_TO_DO;
		}
		else
		{
			//if (cache.isEmptyBuffer(diskCacheMeshId))
			//{
			//	size_t s1 = diskCacheMeshId.size();
			//	size_t s2 = dbMeshId.size();
			//	bool b = cache.firstMeshIdMoreRecent(diskCacheMeshId, dbMeshId);
			//	b = true;
			//}
			
			// info of the terrain is in cache and in db
			if (diskCacheMeshId.compare(dbMeshId) == 0)
			{
				ret = MAPMANAGER_NOTHING_TO_DO;
			}
			else if (cache.firstMeshIdMoreRecent(diskCacheMeshId, dbMeshId))
			{
				PLOG_DEBUG << "Align CACHE <==> DB - Start aligning " << cache.getCacheIdStr() << " Cache ==> DB";
				bool completed = writeDiskCacheToDB(cache, stop, writeCompactVerticesToDB);
				if (completed)
				{
					PLOG_DEBUG << "Align CACHE <==> DB - Alignement " << cache.getCacheIdStr() << " Cache ==> DB - Completed";
					ret = MAPMANAGER_UPDATED_DB;
				}
				else
					ret = MAPMANAGER_STOPPED;
			}
			else
			{
				PLOG_DEBUG << "Align CACHE <==> DB - Start aligning " << cache.getCacheIdStr() << " DB ==> Cache";
				bool completed = writeDiskCacheFromDB(cache, stop);
				if (completed)
				{
					PLOG_DEBUG << "Align CACHE <==> DB - Alignement " << cache.getCacheIdStr() << " DB ==> Cache - Completed";
					ret = MAPMANAGER_UPDATED_CACHE;
				}
				else
					ret = MAPMANAGER_STOPPED;
			}
		}

		return ret;
	}
	
	void MapManager::getQuadrantVertices(float lowerXGridVertex, float lowerZGridVertex, int numVerticesPerSize, float& gridStepInWU, int level, std::string& meshId, std::string& meshBuffer)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("WorldDeploy 1b ") + __FUNCTION__, "getQuadrantVertices");
		//limiter l(2);

		//std::string s = TheWorld_Utils::MeshCacheBuffer::generateNewMeshIdForEmptyBuffer();
		//if (lowerXGridVertex == 0.0f && lowerZGridVertex == 0.0f)
		//{
		//	PLOG_DEBUG << "debug code";
		//}
		
		std::string diskCacheMeshId;
		TheWorld_Utils::MeshCacheBuffer cache;
		
		{
			TheWorld_Utils::GuardProfiler profiler(std::string("WorldDeploy 1b.1 ") + __FUNCTION__, "Get MeshId from cache");
			gridStepInWU = MapManager::gridStepInWU();
			std::string cacheDir = m_SqlInterface->dataPath();
			std::string mapName = getMapName();
			cache = TheWorld_Utils::MeshCacheBuffer(cacheDir, mapName, gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);

			diskCacheMeshId = cache.getMeshIdFromDisk();

			//if (diskCacheMeshId.size() == 0)
			//{
			//	bool stop = false;
			//	int ret = alignDiskCacheAndDB(cache, stop, writeCompactVerticesToDB);
			//	diskCacheMeshId = cache.getMeshIdFromDisk();
			//}
		}

		bool clientCacheValid = false;
		if (meshId.size() > 0)
		{
			if (meshId == diskCacheMeshId)
				clientCacheValid = true;
			else
				if (diskCacheMeshId.size() == 0 || cache.firstMeshIdMoreRecent(meshId, diskCacheMeshId))
					clientCacheValid = true;
		}
		
		if (clientCacheValid)
		{
			// client cache is valid: more recent than server's one (if server's cache is empty it MUST be less recent by design) - We answer an empty buffer so that client can use its own buffer

			TheWorld_Utils::GuardProfiler profiler(std::string("WorldDeploy 1b.2 ") + __FUNCTION__, "Set header in buffer (use client cache)");
			std::vector<float> vectGridHeights;
			float minAltitude = 0, maxAltitude = 0;
			TheWorld_Utils::MemoryBuffer terrainEditValuesBuffer;
			cache.setBufferFromHeights(meshId, numVerticesPerSize, gridStepInWU, terrainEditValuesBuffer, vectGridHeights, meshBuffer, minAltitude, maxAltitude, false);
		}
		else
		{
			// client cache is not valid we answer server's buffer (it can be empty)

			TheWorld_Utils::GuardProfiler profiler(std::string("WorldDeploy 1b.3 ") + __FUNCTION__, "Set buffer from server cache");
			meshId = diskCacheMeshId;
			cache.readBufferFromDisk(diskCacheMeshId, meshBuffer);

			if (diskCacheMeshId.size() == 0 || meshBuffer.size() == 0)
			{
				// client and server cache are not valid
				TheWorld_Utils::MemoryBuffer emptyBuffer;
				std::string emptyBufferMeshId;
				cache.setEmptyBuffer(numVerticesPerSize, emptyBufferMeshId, emptyBuffer);
				cache.writeBufferToDiskCache(emptyBuffer);
				meshId = emptyBufferMeshId;
			}
		}
	}
		
	void MapManager::getVertices(float& anchorXInWUs, float& anchorZInWUs, anchorType type, int numVerticesX, int numVerticesZ, vector<SQLInterface::GridVertex>& mesh, float& gridStepInWU, int level)
	{
		//std::lock_guard<std::recursive_mutex> lock(s_mtxInternalData);

		//limiter l(2);

		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode());
		if (instrumented()) clock.tick();

		float min_X_OnTheGridInWUs, min_Z_OnTheGridInWUs;
		//float max_X_OnTheGridInWUs, max_Z_OnTheGridInWUs;

		if (type == anchorType::center)
		{
			min_X_OnTheGridInWUs = calcPreviousCoordOnTheGridInWUs(anchorXInWUs - (numVerticesX * MapManager::gridStepInWU()) / 2);
			//max_X_OnTheGridInWUs = calcNextCoordOnTheGridInWUs(anchorXInWUs + (numVerticesX * MapManager::gridStepInWU()) / 2);
			min_Z_OnTheGridInWUs = calcPreviousCoordOnTheGridInWUs(anchorZInWUs - (numVerticesZ * MapManager::gridStepInWU()) / 2);
			//max_Z_OnTheGridInWUs = calcNextCoordOnTheGridInWUs(anchorZInWUs + (numVerticesZ * MapManager::gridStepInWU()) / 2);
		}
		else if (type == anchorType::upperleftcorner)
		{
			min_X_OnTheGridInWUs = calcPreviousCoordOnTheGridInWUs(anchorXInWUs);
			//max_X_OnTheGridInWUs = calcNextCoordOnTheGridInWUs(anchorXInWUs + (numVerticesX * MapManager::gridStepInWU()));
			min_Z_OnTheGridInWUs = calcPreviousCoordOnTheGridInWUs(anchorZInWUs);
			//max_Z_OnTheGridInWUs = calcNextCoordOnTheGridInWUs(anchorZInWUs + (numVerticesZ * MapManager::gridStepInWU()));
		}
		else
			throw(MapManagerException(__FUNCTION__, string("Unkwon anchor type").c_str()));

		anchorXInWUs = calcNextCoordOnTheGridInWUs(anchorXInWUs);
		anchorZInWUs = calcNextCoordOnTheGridInWUs(anchorZInWUs);

		int numFoundInDB = 0;

		internalGetVertices(min_X_OnTheGridInWUs, min_Z_OnTheGridInWUs, numVerticesX, numVerticesZ, mesh, gridStepInWU, numFoundInDB, level);

		if (instrumented()) clock.printDuration((string(__FUNCTION__) + " - Num Vertices: " + to_string(mesh.size()) + " - (X x Z): " + to_string(numVerticesX) + " x " + to_string(numVerticesZ) + " - Found in DB: " + to_string(numFoundInDB)).c_str());
	}

	void MapManager::internalGetVertices(float min_X_OnTheGridInWUs, float min_Z_OnTheGridInWUs, int numVerticesX, int numVerticesZ, vector<SQLInterface::GridVertex>& mesh, float& gridStepInWU, int& numFoundInDB, int level)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("internalGetVertices1 ") + __FUNCTION__, "ALL");

		float max_X_OnTheGridInWUs = min_X_OnTheGridInWUs + numVerticesX * gridStepInWU;
		float max_Z_OnTheGridInWUs = min_Z_OnTheGridInWUs + numVerticesZ * gridStepInWU;
		internalGetVertices(min_X_OnTheGridInWUs, max_X_OnTheGridInWUs, min_Z_OnTheGridInWUs, max_Z_OnTheGridInWUs, numVerticesX, numVerticesZ, mesh, gridStepInWU, numFoundInDB, level);
	}
		
	void MapManager::internalGetVertices(float min_X_OnTheGridInWUs, float max_X_OnTheGridInWUs, float min_Z_OnTheGridInWUs, float max_Z_OnTheGridInWUs, int numVerticesX, int numVerticesZ, vector<SQLInterface::GridVertex>& mesh, float& gridStepInWU, int& numFoundInDB, int level)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("internalGetVertices2 ") + __FUNCTION__, "ALL");
		
		vector<FlatGridPoint> grid;
		getFlatGrid(min_X_OnTheGridInWUs, min_Z_OnTheGridInWUs, numVerticesX, numVerticesZ, grid, gridStepInWU);
		getEmptyVertexGrid(grid, mesh, level);

		vector<SQLInterface::GridVertex> vectGridVerticesFromDB;
		m_SqlInterface->getVertices(min_X_OnTheGridInWUs, max_X_OnTheGridInWUs, min_Z_OnTheGridInWUs, max_Z_OnTheGridInWUs, vectGridVerticesFromDB, level);

		std::sort(vectGridVerticesFromDB.begin(), vectGridVerticesFromDB.end());
		std::sort(mesh.begin(), mesh.end());

		vector<SQLInterface::GridVertex>::iterator itGridFromDB = vectGridVerticesFromDB.begin();
		vector<SQLInterface::GridVertex>::iterator itMesh = mesh.begin();
		for (;;)
		{
			//Sleep(0);
			if (itMesh == mesh.end())
				break;

			if (itGridFromDB == vectGridVerticesFromDB.end())
				break;

			if (*itMesh == *itGridFromDB)
			{
				*itMesh = *itGridFromDB;
				itGridFromDB++;
				itMesh++;
				numFoundInDB++;
			}
			else if (*itMesh < *itGridFromDB)
			{
				itMesh++;
				continue;
			}
			else
			{
				while (itGridFromDB != vectGridVerticesFromDB.end() && !(*itMesh < *itGridFromDB))
					itGridFromDB++;

				if (itGridFromDB == vectGridVerticesFromDB.end())
					break;

				if (*itMesh == *itGridFromDB)
				{
					*itMesh = *itGridFromDB;
					itGridFromDB++;
					itMesh++;
					numFoundInDB++;
				}
			}
		}
	}

	void MapManager::getPatches(float anchorX, float anchorZ, anchorType type, float size, vector<GridPatch>& patches, int& numPatchX, int& numPatchZ, float& gridStepInWU, int level)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode());
		if (instrumented()) clock.tick();

		patches.clear();
		
		int numPointX = 0, numPointZ = 0;
		vector<SQLInterface::GridVertex> mesh;
		getVertices(anchorX, anchorZ, type, size, mesh, numPointX, numPointZ, gridStepInWU, level);

		if (mesh.size() <= 1)
			return;

		numPatchX = numPointX - 1;
		numPatchZ = numPointZ - 1;
		
		patches.reserve(size_t(numPatchX) * size_t(numPatchZ));
		
		// Collecting patches: the patch is the one wich has the current grid vertex as upper left corner
		for (size_t idx = 0; idx < mesh.size(); idx++)
		{
			SQLInterface::GridVertex upperLeftGridVertex = mesh[idx];
			assert(idx + 1 < mesh.size());
			SQLInterface::GridVertex upperRightGridVertex = mesh[idx + 1];
			if (upperLeftGridVertex.posZ() != upperRightGridVertex.posZ())
				continue;	// the current grid vertex (p1) is the last of the row so it doesn't point to a patch
			if (idx + numPointX >= mesh.size())
				break;		// the current grid vertex (p1) is on the last row of the grid so it doesn't point to a patch nor any of the following: we can exit loop
			SQLInterface::GridVertex lowerLeftGridVertex = mesh[idx + numPointX];
			SQLInterface::GridVertex lowerRightGridVertex = mesh[idx + numPointX + 1];
			GridPatch patch(upperLeftGridVertex, upperRightGridVertex, lowerLeftGridVertex, lowerRightGridVertex);
			patches.push_back(patch);
		}
		assert((int)patches.size() == numPatchX * numPatchZ);

		if (instrumented()) clock.printDuration((string(__FUNCTION__) + " - Num patches: " + to_string(patches.size()) + " - (X x Z): " + to_string(numPatchX) + " x " + to_string(numPatchZ)).c_str());
	}

	struct GISPoint
	{
		// needed to use an istance of GISPoint as a key in a map (to keep the map sorted by y and by x for equal y)
		// first row, second row, ... etc
		bool operator<(const GISPoint& p) const
		{
			if (y < p.y)
				return true;
			if (y > p.y)
				return false;
			else
				return x < p.x;
		}
		double x;
		double y;
	};
	typedef map<int, vector<GISPoint>> _GISPointsAffectingGridPoints;

	//typedef map<point, vector<double>, std::less<point>> pointMap;
	typedef map<GISPoint, vector<double>> _GISPointMap;

	void MapManager::LoadGISMap(const char* fileInput, bool writeReport, float metersInWU, int level)
	{
		// *****************************************************************************************************************
		// W A R N I N G : assuming coordinates in input file are in projected coordinates EPSG 3857 where the unit is metre
		// *****************************************************************************************************************

		TimerMs clock(true, consoleDebugMode());
		if (instrumented()) clock.tick();

		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		consoleDebugUtil _consoleDebugUtil1(consoleDebugMode());

		// ***************************************************************************
		// W A R N I N G : Z axis  is UP (Blender uses right handed coordinate system: X left to right, Y front to back, Z top to bottom)
		// ***************************************************************************

		//string filePath = "D:\\TheWorld\\Client\\Italy_shapefile\\it_10km.shp";
		// https://www.youtube.com/watch?v=lP52QKda3mw
		string filePath = fileInput;

		SHPHandle handle = SHPOpen(filePath.c_str(), "rb");
		if (handle != 0)
		{
			throw(MapManagerException(__FUNCTION__, string("File " + filePath + " not found").c_str()));
		}
		
		string fileName, outfilePath;
		ofstream outFile;
		if (writeReport)
		{
			fileName = filePath.substr(filePath.find_last_of("\\") + 1, (filePath.find_last_of(".") - filePath.find_last_of("\\") - 1));
			outfilePath = filePath.substr(0, filePath.find_last_of("\\") + 1) + "LoadGISMap_" + fileName + ".txt";
			outFile.open(outfilePath);
		}

		int shapeType, nEntities;
		double adfMinBound[4], adfMaxBound[4];
		SHPGetInfo(handle, &nEntities, &shapeType, adfMinBound, adfMaxBound);

		if (writeReport)
		{
			outFile << "Shape Type: " << to_string(shapeType) << "\n";
			outFile << "Min Bound X: " << to_string(adfMinBound[0]) << " Min Bound Y: " << to_string(adfMinBound[1]) << " Min Bound Z: " << to_string(adfMinBound[2]) << " Min Bound M: " << to_string(adfMinBound[3]) << "\n";
			outFile << "Max Bound X: " << to_string(adfMaxBound[0]) << " Max Bound Y: " << to_string(adfMaxBound[1]) << " Max Bound Z: " << to_string(adfMaxBound[2]) << " Max Bound M: " << to_string(adfMaxBound[3]) << "\n";
			outFile << "Size X: " << to_string(adfMaxBound[0] - adfMinBound[0]) << " Size Y: " << to_string(adfMaxBound[1] - adfMinBound[1]) << " Size Z: " << to_string(adfMaxBound[2] - adfMinBound[2]) << " Size M: " << to_string(adfMaxBound[3] - adfMinBound[3]) << "\n";
		}

		double minAltitude = adfMinBound[2];
		
		// B O U N D I N G   B O X   I N   W U s
		// transfom: meters ==> WUs, we need to express di bounding box in WUs
		float minAOE_X_WU = (float)adfMinBound[0] / metersInWU;
		float maxAOE_X_WU = (float)adfMaxBound[0] / metersInWU;
		float minAOE_Z_WU = (float)adfMinBound[1] / metersInWU;
		float maxAOE_Z_WU = (float)adfMaxBound[1] / metersInWU;

		vector<FlatGridPoint> grid;
		int numPointX;
		int numPointZ;
		float gridStepInWU;

		// we need to calculate the grid so that the map grows of multiples of square patches with a number of vertices for every size equal to g_DBGrowingBlockVertexNumber
		// so the grid has a number of vertices equal to a multiple of g_DBGrowingBlockVertexNumber, they are spaced by a number of WU equal to gridStepInWU (MapManager::gridStepInWU())
		getSquareFlatGridToExpand(minAOE_X_WU, maxAOE_X_WU, minAOE_Z_WU, maxAOE_Z_WU, grid, numPointX, numPointZ, gridStepInWU);

		_GISPointMap GISPointAltiduesMap;
		_GISPointMap::iterator itGISPointAltiduesMap;

		_GISPointsAffectingGridPoints GISPointsAffectingGridPoints;
		_GISPointsAffectingGridPoints::iterator itGISPointsAffectingGridPoints;

		// ****************************************
		// Read input file and create the point map
		// ****************************************
		string s = "FIRST LOOP - Looping into entities of: " + filePath + " - Entities(" + to_string(nEntities) + "): ";
		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, s.c_str());
		if (writeReport) outFile << endl << "************************* INIZIO SEZIONE *************************" << endl << s.c_str() << endl;
		for (int i = 0; i < nEntities; i++)
		{
			SHPObject * psShape = SHPReadObject(handle, i);

			// Read only polygons, and only those without holes
			if (
					((psShape->nSHPType == SHPT_MULTIPOINT || psShape->nSHPType == SHPT_MULTIPOINTZ || psShape->nSHPType == SHPT_MULTIPOINTM) && psShape->nParts == 0)
					//|| ((psShape->nSHPType == SHPT_POLYGON || psShape->nSHPType == SHPT_POLYGONZ || psShape->nSHPType == SHPT_POLYGONM) && psShape->nParts == 1)
				)
			{
				if (writeReport) outFile << "Entity: " << to_string(i) << " - Num Vertices: " << to_string(psShape->nVertices) << "\n";

				double* x = psShape->padfX;
				double* y = psShape->padfY;
				double* z = psShape->padfZ;
				double* m = psShape->padfM;

				if (consoleDebugMode())
				{
					string s = "   Dumping vertices(" + to_string(psShape->nVertices) + "): ";
					_consoleDebugUtil1.printFixedPartOfLine(classname(), __FUNCTION__, s.c_str(), &_consoleDebugUtil);
					_consoleDebugUtil1.printNewLine();
				}
				for (int v = 0; v < psShape->nVertices; v++)
				{
					if (writeReport) outFile << "Vertex X: " << to_string(x[v]) << " - Vertex Y: " << to_string(y[v]) << " - Vertex Z: " << to_string(z[v]) << " - Vertex M: " << to_string(m[v]) << "\n";

					// From now we consider only WUs
					GISPoint p = { x[v] / metersInWU, y[v] / metersInWU };

					// For every point read from input file we colect all its alitudes (detecting if it can have more than one)
					{
						/*if (x[v] == 1195425.1762949340 && z[v] == 869.21911621093750)
						{
							outFile << "eccolo" << endl;
						}*/
						itGISPointAltiduesMap = GISPointAltiduesMap.find(p);
						if (itGISPointAltiduesMap == GISPointAltiduesMap.end())
						{
							vector<double> altitudes;
							altitudes.push_back(z[v] / metersInWU);
							GISPointAltiduesMap[p] = altitudes;
						}
						else
						{
							GISPointAltiduesMap[p].push_back(z[v] / metersInWU);
							assert(GISPointAltiduesMap[p].size() == 1);
						}
					}

					// for every point of input file we assign it as an altitude modifier of the grid map point vertices of the grid map square in which it is contained (it will determine its altitude interpolating every affecting GIS points)
					// assuming points are inside af the square and not placed on the vertices of the grid map then for every grid map point we have to interpolate its altidue considering all the point modifiying it
					if (z[v] != minAltitude)
					{
						double offsetX = (p.x - grid[0].x);
						int idxMinAffectedGridPointX = (int)floor(offsetX / gridStepInWU);		// 0 to numPointX - 1
						double offsetZ = (p.y - grid[0].z);
						int idxMinAffectedGridPointZ = (int)floor(offsetZ / gridStepInWU);		// 0 to numPointZ - 1
						
						//	.	.	.	.	.	.	.
						//	.	.	.	.	.	.	.
						//	.	.	P1	P2	.	.	.
						//	.	.	P3	P4	.	.	.
						//	.	.	.	.	.	.	.
						//	.	.	.	.	.	.	.
						//	.	.	.	.	.	.	.
						int idxGridP1 = idxMinAffectedGridPointZ * numPointX + idxMinAffectedGridPointX;			// + 1 to position on the first point of the line after bypassing the points of the upper lines
						int idxGridP2 = idxGridP1 + 1;
						//int idxGridP3 = (idxMinAffectedGridPointZ + 1) * numPointX + idxMinAffectedGridPointX;
						int idxGridP3 = idxGridP1 + numPointX;
						int idxGridP4 = idxGridP3 + 1;

						void LoadGISMap_pushPointsAffectingPointMap(TheWorld_MapManager::_GISPointsAffectingGridPoints& map, TheWorld_MapManager::GISPoint& p, int idxPoint);
						LoadGISMap_pushPointsAffectingPointMap(GISPointsAffectingGridPoints, p, idxGridP1);
						LoadGISMap_pushPointsAffectingPointMap(GISPointsAffectingGridPoints, p, idxGridP2);
						LoadGISMap_pushPointsAffectingPointMap(GISPointsAffectingGridPoints, p, idxGridP3);
						LoadGISMap_pushPointsAffectingPointMap(GISPointsAffectingGridPoints, p, idxGridP4);
					}

					if (consoleDebugMode() && fmod(v + 1, 1000) == 0) _consoleDebugUtil1.printVariablePartOfLine(v + 1);
				}
				if (consoleDebugMode()) _consoleDebugUtil1.printVariablePartOfLine(psShape->nVertices);
			}

			SHPDestroyObject(psShape);

			if (consoleDebugMode() && fmod(i + 1, 1000) == 0) _consoleDebugUtil.printVariablePartOfLine(i + 1);
		}
		if (writeReport) outFile << "************************* FINE SEZIONE ***************************" << endl;
		if (consoleDebugMode()) _consoleDebugUtil.printVariablePartOfLine(nEntities);
		SHPClose(handle);

		// We need to know for every point of the point map read from the input file in which square of the grid is placed (the grid is spaced by MapManager::gridStepInWU() WUs and the input file express points in meters)
		// TODO
		/*if (writeReport) 
		{
			if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Dumping Row / Column for every point of the plane: ", &_consoleDebugUtil1);

			int idxPoint = 0;
			outFile << endl << "************************* INIZIO SEZIONE *************************" << endl;
			outFile << "Inizio test" << endl;
			int row = 0, col = 0, maxCols = 0;
			double lastX = 0.0;
			for (itAltiduesMap = altiduesMap.begin(); itAltiduesMap != altiduesMap.end(); itAltiduesMap++)
			{
				if (itAltiduesMap->second.size() != 1)
					throw(MapManagerException(__FUNCTION__, "Found point with a number of altitudes not equal 1"));

				idxPoint++;

				if (itAltiduesMap->first.x != lastX)
				{
					lastX = itAltiduesMap->first.x;
					row++;
					if (col > maxCols)
						maxCols = col;
					col = 0;
					outFile << "Row: " << to_string(row) << " - Vertex X: " << to_string(itAltiduesMap->first.x) << endl;
				}
				col++;
				outFile << "   Col: " << to_string(col) << " - Vertex Y: " << to_string(itAltiduesMap->first.y) << " - Altitude: " << itAltiduesMap->second[0] << endl;

				if (consoleDebugMode() && fmod(idxPoint, 1000) == 0) _consoleDebugUtil.printVariablePartOfLine(idxPoint);
			}
			outFile << endl << "Fine test - Rows: " << to_string(row) << " - MaxCols: " << to_string(maxCols) << endl;
			outFile << "************************* FINE SEZIONE ***************************" << endl;

			if (consoleDebugMode()) _consoleDebugUtil.printVariablePartOfLine(idxPoint);
		}*/

		vector<SQLInterface::GridVertex> vectGridVertices;

		int maxNumGISPointsAffectingGridPoints = 0;

		s = "GrowingBlockVertexNumber: " + to_string(g_DBGrowingBlockVertexNumber) + " - GridStepInWU : " + to_string(MapManager::gridStepInWU());
		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, s.c_str(), &_consoleDebugUtil1);
		if (writeReport) outFile << endl << s.c_str() << endl;

		s = "SECOND LOOP - Filling map to DB - Num Grid points : " + to_string(numPointX * numPointZ) + " (" + to_string(numPointX) + " x " + to_string(numPointZ) + ") - Grid boxes: " + to_string(numPointX - 1) + " x " + to_string(numPointZ - 1);
		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, (s + " - Point: ").c_str(), &_consoleDebugUtil);
		if (writeReport) outFile << endl << "************************* INIZIO SEZIONE *************************" << endl << s.c_str() << endl;
		int idxGridPoint = 0, row = 0, col = 0;
		for (int z = 0; z < numPointZ; z++)
		{
			row++;
			col = 0;
			for (int x = 0; x < numPointX; x++)
			{

				col++;

				double LoadGISMap_interpolateAltitudeOfGridPOint(vector<FlatGridPoint>& grid, int idxGridPoint, _GISPointsAffectingGridPoints& GISPointsAffectingGridPoints, _GISPointMap& GISPointAltidues);
				double altidue = LoadGISMap_interpolateAltitudeOfGridPOint(grid, idxGridPoint, GISPointsAffectingGridPoints, GISPointAltiduesMap);

				SQLInterface::GridVertex gridVertex(grid[idxGridPoint].x, grid[idxGridPoint].z, (float)altidue, level);
				vectGridVertices.push_back(gridVertex);

				if (writeReport)
				{
					if (x == 0)
						outFile << "Row: " << to_string(row) << " - Z: " << to_string(grid[idxGridPoint].z) << endl;
					if (GISPointsAffectingGridPoints[idxGridPoint].size() > maxNumGISPointsAffectingGridPoints)
						maxNumGISPointsAffectingGridPoints = (int)GISPointsAffectingGridPoints[idxGridPoint].size();
					outFile << "   Grid Map Idx: " << to_string(idxGridPoint) << " - Row: " << to_string(row) << " - Col: " << to_string(col) << " - X: " << to_string(grid[idxGridPoint].x) << " - Z: " << to_string(grid[idxGridPoint].z) << " - Altitude : " << to_string((float)altidue) << " - Num affecting GIS points : " << to_string(GISPointsAffectingGridPoints[idxGridPoint].size()) << endl;
					for (int idx = 0; idx < GISPointsAffectingGridPoints[idxGridPoint].size(); idx++)
					{
						double LoadGISMap_getAltitude(TheWorld_MapManager::GISPoint& GISP, TheWorld_MapManager::_GISPointMap& GISPointAltidues);
						double GISAltitude = LoadGISMap_getAltitude(GISPointsAffectingGridPoints[idxGridPoint][idx], GISPointAltiduesMap);
						outFile << "      GIS X: " << to_string(GISPointsAffectingGridPoints[idxGridPoint][idx].x) << " - GIS Y: " << to_string(GISPointsAffectingGridPoints[idxGridPoint][idx].y) << " - Altitude: " << to_string((float)GISAltitude) << endl;
					}
				}

				idxGridPoint++;
				if (consoleDebugMode() && fmod(idxGridPoint + 1, 1000) == 0) _consoleDebugUtil.printVariablePartOfLine(idxGridPoint);
			}
		}
		if (writeReport)
		{
			outFile << "Max num GIS points affecting Grid points: " << to_string(maxNumGISPointsAffectingGridPoints ) << endl;
			outFile << "************************* FINE SEZIONE ***************************" << endl;
		}
		if (consoleDebugMode()) _consoleDebugUtil.printVariablePartOfLine(idxGridPoint);

		if (writeReport) outFile.close();

		__int64 rowid = m_SqlInterface->addWDAndVertices(NULL, vectGridVertices);

		
		if (instrumented()) clock.printDuration(__FUNCTION__);
	}

	float MapManager::interpolateAltitude(vector<SQLInterface::GridVertex>& vectGridVertex, FlatGridPoint& pos)
	{
		// I N T E R P O L A T I O N : we use the "inverse distance weighted" algorithm
		// rapporto tra la sommatoria per ogni punto da interpolare di altitudine su distanza dal punto target e la sommatoria di 1 su distanza dal punto target

		if (vectGridVertex.size() == 0)
			return 0.0;
		
		float numerator = 0.0, denominator = 0.0;
		for (int idx = 0; idx < vectGridVertex.size(); idx++)
		{
			float altitude = vectGridVertex[idx].altitude();
			float distance = sqrtf(powf((pos.x - vectGridVertex[idx].posX()), 2.0) + powf((pos.z - vectGridVertex[idx].posZ()), 2.0));
			if (distance == 0.0)
			{
				numerator += altitude;
				denominator += 1.0;
			}
			else
			{
				numerator += (altitude / distance);
				denominator += (1.0f / distance);
			}
		}
		return (numerator / denominator);
	}

	bool MapManager::TransformProjectedCoordEPSG3857ToGeoCoordEPSG4326(double X, double Y, double& lonDecimalDegrees, double& latDecimalDegrees, int& lonDegrees, int& lonMinutes, double& lonSeconds, int& latDegrees, int& latMinutes, double& latSeconds)
	{
		// https://proj.org/development/quickstart.html
		// 
		// EPSG:3857 WGS 84 / Pseudo-Mercator ==> EPSG:4326 WGS 84
		// https://epsg.io/transform#s_srs=3857&t_srs=4326&x=1195475.1220960&y=5467999.2554860
		// 
		// https://gis.stackexchange.com/questions/48949/epsg-3857-or-4326-for-googlemaps-openstreetmap-and-leaflet
		// 
		// Google Earth is in a Geographic coordinate system with the wgs84 datum. (EPSG: 4326)
		// Google Maps is in a projected coordinate system that is based on the wgs84 datum. (EPSG 3857)
		// The data in Open Street Map database is stored in a gcs with units decimal degrees& datum of wgs84. (EPSG: 4326)
		// The Open Street Map tilesand the WMS webservice, are in the projected coordinate system that is based on the wgs84 datum. (EPSG 3857)
		// 
		// E:\OSGeo4W>echo 1195475.122096 5467999.255486 | proj +proj=webmerc +datum=WGS84 -I
		// 10d44'20.889"E  44d0'59.477"N
		// 
		// E:\OSGeo4W>echo 1195475.122096 5467999.255486 | proj +proj=webmerc +datum=WGS84 -I -d 6
		// 10.739136       44.016521
		// 
		// E:\OSGeo4W>echo 10.739136 44.016521 | proj +proj=webmerc +datum=WGS84 -d 6
		// 1195475.151080  5467999.202178
		// 
		// E:\OSGeo4W>echo 10d44'20.889"E  44d0'59.477"N | proj +proj=webmerc +datum=WGS84 -d 6
		// 1195475.132526  5467999.262376
		//

		PJ_CONTEXT* C;
		PJ* P;
		PJ* P_for_GIS;
		PJ_COORD a, b;


		/* or you may set C=PJ_DEFAULT_CTX if you are sure you will     */
		/* use PJ objects from only one thread                          */
		C = proj_context_create();
		P = proj_create_crs_to_crs(C,
			//"EPSG:4326",
			//"+proj=utm +zone=32 +datum=WGS84", /* or EPSG:32632 */
			"EPSG:3857",
			//"EPSG:4326 +datum=WGS84", /* or EPSG:32632 */
			"EPSG:4326",
			NULL);

		if (0 == P)
		{
			return false;
		}

		/* This will ensure that the order of coordinates for the input CRS */
		/* will be longitude, latitude, whereas EPSG:4326 mandates latitude, */
		/* longitude */
		P_for_GIS = proj_normalize_for_visualization(C, P);
		if (0 == P_for_GIS)
		{
			fprintf(stderr, "Oops\n");
			return 1;
		}
		proj_destroy(P);
		P = P_for_GIS;

		//a = proj_coord(1195475.122096, 5467999.255486, 0, 0);
		a = proj_coord(X, Y, 0, 0);
		b = proj_trans(P, PJ_FWD, a);

		/* Clean up */
		proj_destroy(P);
		proj_context_destroy(C); /* may be omitted in the single threaded case */

		lonDecimalDegrees = b.lp.lam;
		latDecimalDegrees = b.lp.phi;

		// https://www.calculatorsoup.com/calculators/conversions/convert-decimal-degrees-to-degrees-minutes-seconds.php

		DecimalDegreesToDegreesMinutesSeconds(lonDecimalDegrees, lonDegrees, lonMinutes, lonSeconds);

		DecimalDegreesToDegreesMinutesSeconds(latDecimalDegrees, latDegrees, latMinutes, latSeconds);

		return true;
	}

	void MapManager::DecimalDegreesToDegreesMinutesSeconds(double decimalDegrees, int& degrees, int& minutes, double& seconds)
	{
		//Follow these steps to convert decimal degrees to DMS :
		// For the degrees use the whole number part of the decimal
		// For the minutes multiply the remaining decimal by 60. Use the whole number part of the answer as minutes.
		// For the seconds multiply the new remaining decimal by 60
		double d = floor(decimalDegrees);
		degrees = (int)d;
		decimalDegrees = (decimalDegrees - d) * 60;
		d = floor(decimalDegrees);
		minutes = (int)d;
		seconds = (decimalDegrees - d) * 60;
	}
}

void LoadGISMap_pushPointsAffectingPointMap(TheWorld_MapManager::_GISPointsAffectingGridPoints& map, TheWorld_MapManager::GISPoint& point, int idxPoint)
{
	TheWorld_MapManager::_GISPointsAffectingGridPoints::iterator itPointsAffectingPointMap = map.find(idxPoint);
	if (itPointsAffectingPointMap == map.end())
	{
		vector<TheWorld_MapManager::GISPoint> points;
		points.push_back(point);
		map[idxPoint] = points;
	}
	else
	{
		map[idxPoint].push_back(point);
	}
}

double LoadGISMap_interpolateAltitudeOfGridPOint(vector<TheWorld_MapManager::MapManager::FlatGridPoint>& grid, int idxGridPoint, TheWorld_MapManager::_GISPointsAffectingGridPoints& GISPointsAffectingGridPoints,
	TheWorld_MapManager::_GISPointMap& GISPointAltidues)
{
	// I N T E R P O L A T I O N : we use the "inverse distance weighted" algorithm
	// rapporto tra la sommatoria per ogni punto da interpolare di altitudine su distanza dal punto target e la sommatoria di 1 su distanza dal punto target
	
	// DEBUG
	//TheWorld_MapManager::MapManager::gridPoint gridP = grid[idxGridPoint];
	//vector<TheWorld_MapManager::GISPoint> GISPoints = GISPointsAffectingGridPoints[idxGridPoint];
	// DEBUG

	if (GISPointsAffectingGridPoints[idxGridPoint].size() == 0)
		return 0.0;

	double numerator = 0.0, denominator = 0.0;
	for (int idx = 0; idx < GISPointsAffectingGridPoints[idxGridPoint].size(); idx++)
	{
		
		// DEBUG
		//{
		//	size_t a = 0;
		//	if (GISPointsAffectingGridPoints[idxGridPoint].size() > 15)
		//		a = GISPointsAffectingGridPoints[idxGridPoint].size();
		//}
		// DEBUG

		double LoadGISMap_getAltitude(TheWorld_MapManager::GISPoint & GISP, TheWorld_MapManager::_GISPointMap & GISPointAltidues);
		double altitude = LoadGISMap_getAltitude(GISPointsAffectingGridPoints[idxGridPoint][idx], GISPointAltidues);

		double LoadGISMap_getDistance(TheWorld_MapManager::MapManager::FlatGridPoint & gridP, TheWorld_MapManager::GISPoint& GISP);
		double distance = LoadGISMap_getDistance(grid[idxGridPoint], GISPointsAffectingGridPoints[idxGridPoint][idx]);

		if (distance == 0.0)
		{
			numerator += altitude;
			denominator += 1.0;
		}
		else
		{
			numerator += (altitude / distance);
			denominator += (1.0 / distance);
		}
	}
	
	return (numerator / denominator);
}

double LoadGISMap_getDistance(TheWorld_MapManager::MapManager::FlatGridPoint& gridP, TheWorld_MapManager::GISPoint& GISP)
{
	return sqrt(pow(((double)gridP.x - GISP.x), 2.0) + pow(((double)gridP.z - GISP.y), 2.0));
}

double LoadGISMap_getAltitude(TheWorld_MapManager::GISPoint& GISP, TheWorld_MapManager::_GISPointMap& GISPointAltidues)
{
	TheWorld_MapManager::_GISPointMap::iterator itGISPointAltiduesMap = GISPointAltidues.find(GISP);
	if (itGISPointAltiduesMap == GISPointAltidues.end())
		return 0.0;
	else
		return GISPointAltidues[GISP][0];
}
