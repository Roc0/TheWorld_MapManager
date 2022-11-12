#include "pch.h"

#define _USE_MATH_DEFINES // for C++

#include "assert.h"

#include "json/json.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>

#include "MapManager.h"
#include "DBSQLLite.h"

#include "shapefil.h"
#include <proj.h>

namespace fs = std::filesystem;

namespace TheWorld_MapManager
{
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
	float g_gridStepInWU = 0.0f;		// distance in world unit between a vertex of the grid and the next

	MapManager::MapManager(const char* logPath, plog::Severity sev, plog::IAppender* appender, char* configFileName)
	{
		string sModulePath = getModuleLoadPath();

		string configFilePath = sModulePath;
		if (configFileName == NULL)
			configFilePath += "\\TheWorld_MapManager.json";
		else
			configFilePath += string ("\\") + configFileName;

		if (appender == NULL)
		{
			string _logPath;
			if (logPath == NULL)
				_logPath = sModulePath += "\\TheWorld_MapManager_Log.txt";
			else
				_logPath = logPath;
			m_utils.init(_logPath.c_str(), sev);
		}
		else
		{
			m_utils.init(NULL, sev, appender);
		}
		
		Json::Value root;
		std::ifstream jsonFile(configFilePath);
		jsonFile >> root;
		m_dataPath = root["DataPath"].asString();

		m_SqlInterface = new DBSQLLite(DBType::SQLLite, m_dataPath.c_str());
		m_instrumented = false;
		m_consoleDebugMode = false;

		string s = m_SqlInterface->readParam(GrowingBlockVertexNumberShiftParamName);
		if (s.empty())
			throw(MapManagerException(__FUNCTION__, string("Param <" + GrowingBlockVertexNumberShiftParamName + "> not read from DB").c_str()));
		g_DBGrowingBlockVertexNumberShift = stoi(s);
		g_DBGrowingBlockVertexNumber = 1 << g_DBGrowingBlockVertexNumberShift;
		s = m_SqlInterface->readParam(GridStepInWUParamName);
		if (s.empty())
			throw(MapManagerException(__FUNCTION__, string("Param <" + GridStepInWUParamName + "> not read from DB").c_str()));
		g_gridStepInWU = stof(s);
	}

	MapManager::~MapManager()
	{
		if (m_SqlInterface)
			m_SqlInterface->finalizeDB();
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
		TimerMs clock(true, consoleDebugMode()); // Timer<milliseconds, steady_clock>
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

		int gridSize = (maxGridPosX - minGridPosX + 1) * (maxGridPosZ - minGridPosZ + 1);
		vector<SQLInterface::GridVertex> v;
		
		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Computing affected vertices by WorldDefiner: ");

		int numVertices = 0;
		for (int x = minGridPosX; x <= maxGridPosX; x++)
		{
			for (int z = minGridPosZ; z <= maxGridPosZ; z++)
			{
				// guard
				{
					numVertices++;
					assert(numVertices <= gridSize);
				}

				SQLInterface::GridVertex mapv(float(x) * gridStepInWU, float(z) * gridStepInWU, WD.getLevel());
				v.push_back(mapv);

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
		grid.reserve(size_t(numPointX) * size_t(numPointZ));

		for (int z = 0; z < numPointZ; z++)
		{
			float incZ = minZInWUs + z * gridStepInWU;
			for (int x = 0; x < numPointX; x++)
			{
				FlatGridPoint p;
				p.x = minXInWUs + x * gridStepInWU;
				p.z = incZ;
				grid.push_back(p);
			}
		}
		assert(grid.size() == size_t(numPointX) * size_t(numPointZ));
	}

	void MapManager::getEmptyVertexGrid(vector<FlatGridPoint>& grid, vector<SQLInterface::GridVertex>& emptyGridVertex, int level)
	{
		emptyGridVertex.clear();
		vector<FlatGridPoint>::iterator it;
		for (it = grid.begin(); it != grid.end(); it++)
		{
			SQLInterface::GridVertex gridVertex(it->x, it->z, level);
			emptyGridVertex.push_back(gridVertex);
		}
	}

	bool MapManager::eraseWD(__int64 wdRowid)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode()); // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();

		bool bDeleted = m_SqlInterface->eraseWD(wdRowid);

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (consoleDebugMode()) _consoleDebugUtil.printNewLine();

		return bDeleted;
	}

	bool MapManager::eraseWD(float posX, float posZ, int level, WDType type)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode()); // Timer<milliseconds, steady_clock>
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
		TimerMs clock(true, consoleDebugMode()); // Timer<milliseconds, steady_clock>
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
		TimerMs clock(true, consoleDebugMode()); // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();

		/*
		* Open Transaction
		*/
		m_SqlInterface->beginTransaction();

		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Updating vertices marked for update: ");
		int updated = 0;
		int idx = 0;
		SQLInterface::GridVertex gridVertex;
		vector<WorldDefiner> vectWD;
		bool bFound = m_SqlInterface->getFirstModfiedVertex(gridVertex, vectWD);
		while (bFound)
		{
			idx++;

			float altitude = computeAltitude(gridVertex, vectWD);
			m_SqlInterface->updateAltitudeOfVertex(gridVertex.rowid(), altitude);

			updated++;
			if (consoleDebugMode() && fmod(idx, 1000) == 0)
			{
				string s = "Vertices marked for update: ";	s += to_string(idx);	s += " - Vertices Updated: ";	s += to_string(updated);
				_consoleDebugUtil.printVariablePartOfLine(s.c_str());
			}

			bFound = m_SqlInterface->getNextModfiedVertex(gridVertex, vectWD);
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
					altitude += computeAltitudeElevator(gridVertex, wdMap[idx], distanceFromWD);
					break;
				default:
					break;
				}
			}
		}

		return altitude;
	}

	float MapManager::computeAltitudeElevator(SQLInterface::GridVertex& gridVertex, WorldDefiner& wd, float distanceFromWD)
	{
		float altitude = 0.0;

		if (distanceFromWD == -1)
			distanceFromWD = getDistance(gridVertex.posX(), gridVertex.posZ(), wd.getPosX(), wd.getPosZ());
		
		switch (wd.getFunctionType())
		{
		case WDFunctionType::cosin:
		{
			float d = distanceFromWD / wd.getAOE();	// from 0 (on WD) to 1 (on border)
			float argument = d * (float)M_PI_2;		// from 0 (on WD) to M_PI_2 (on border)
			altitude = cosf(argument);				// from 1 (on WD) to 0 (on border)
			altitude *= wd.getStrength();			// from wd.getStrength() (on WD) to 0 (on border)
			//altitude = cosf( (distanceFromWD / wd.getAOE()) * (float)M_PI_2 ) * wd.getStrength();
		}
			break;
		default:
			break;
		}

		return altitude;
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
		TimerMs clock(true, consoleDebugMode()); // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();

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

	void MapManager::getVertices(float& anchorXInWUs, float& anchorZInWUs, anchorType type, int numVerticesX, int numVerticesZ, vector<SQLInterface::GridVertex>& mesh, float& gridStepInWU, int level)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode()); // Timer<milliseconds, steady_clock>
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
		vector<FlatGridPoint> grid;
		getFlatGrid(min_X_OnTheGridInWUs, min_Z_OnTheGridInWUs, numVerticesX, numVerticesZ, grid, gridStepInWU);
		getEmptyVertexGrid(grid, mesh, level);

		float max_X_OnTheGridInWUs = min_X_OnTheGridInWUs + numVerticesX * gridStepInWU;
		float max_Z_OnTheGridInWUs = min_Z_OnTheGridInWUs + numVerticesZ * gridStepInWU;
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

	void MapManager::getPatches(float anchorX, float anchorZ, anchorType type, float size, vector<GridPatch>& patches, int& numPatchX, int& numPatchZ, float& gridStepInWU, int level)
	{
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());
		TimerMs clock(true, consoleDebugMode()); // Timer<milliseconds, steady_clock>
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

		TimerMs clock(true, consoleDebugMode()); // Timer<milliseconds, steady_clock>
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

		if (0 == P) {
			return false;
		}

		/* This will ensure that the order of coordinates for the input CRS */
		/* will be longitude, latitude, whereas EPSG:4326 mandates latitude, */
		/* longitude */
		P_for_GIS = proj_normalize_for_visualization(C, P);
		if (0 == P_for_GIS) {
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

	MapManager::QuadrantId MapManager::QuadrantId::getQuadrantId(enum class DirectionSlot dir, int numSlot)
	{
		QuadrantId q = *this;

		switch (dir)
		{
			case DirectionSlot::XMinus:
			{
				q.m_lowerXGridVertex -= (m_sizeInWU * numSlot);
			}
			break;
			case DirectionSlot::XPlus:
			{
				q.m_lowerXGridVertex += (m_sizeInWU * numSlot);
			}
			break;
			case DirectionSlot::ZMinus:
			{
				q.m_lowerZGridVertex -= (m_sizeInWU * numSlot);
			}
			break;
			case DirectionSlot::ZPlus:
			{
				q.m_lowerZGridVertex += (m_sizeInWU * numSlot);
			}
			break;
		}

		return q;
	}

	size_t MapManager::QuadrantId::distanceInPerimeter(MapManager::QuadrantId& q)
	{
		size_t distanceOnX = (size_t)ceil( abs(q.getLowerXGridVertex() - getLowerXGridVertex()) / q.m_sizeInWU);
		size_t distanceOnZ = (size_t)ceil( abs(q.getLowerZGridVertex() - getLowerZGridVertex()) / q.m_sizeInWU);
		if (distanceOnX > distanceOnZ)
			return distanceOnX;
		else
			return distanceOnZ;
	}

	MapManager::Quadrant* MapManager::getQuadrant(float& viewerPosX, float& viewerPosZ, int level, int numVerticesPerSize)
	{
		float _gridStepInWU = gridStepInWU();
		QuadrantId quadrantId(viewerPosX, viewerPosZ, level, numVerticesPerSize, _gridStepInWU);
		MapManager::Quadrant* quadrant = new Quadrant(quadrantId, this);
		quadrant->populateGridVertices(viewerPosX, viewerPosZ);

		return quadrant;
	}

	MapManager::Quadrant* MapManager::getQuadrant(MapManager::QuadrantId q, enum class MapManager::QuadrantId::DirectionSlot dir)
	{
		QuadrantId quadrantId = q.getQuadrantId(dir);
		MapManager::Quadrant* quadrant = new Quadrant(quadrantId, this);
		float viewerPosX = 0, viewerPosZ = 0;
		quadrant->populateGridVertices(viewerPosX, viewerPosZ);

		return quadrant;
	}

	void MapManager::Quadrant::populateGridVertices(float& viewerPosX, float& viewerPosZ)
	{
		BYTE shortBuffer[256 + 1];
		size_t size;

		//{
		//	GridVertex v(0.12F, 0.1212F, 0.1313F, 1);
		//	v.serialize(shortBuffer, size);
		//	GridVertex v1(shortBuffer, size);
		//	assert(v.posX() == v1.posX());
		//	assert(v.altitude() == v1.altitude());
		//	assert(v.posZ() == v1.posZ());
		//	assert(v.lvl() == v1.lvl());
		//}
		
		m_vectGridVertices.clear();

		size_t serializedVertexSize;
		GridVertex v;
		v.serialize(shortBuffer, serializedVertexSize);

		// look for cache in file system
		char level[4];
		snprintf(level, 4, "%03d", m_quadrantId.getLevel());
		string cachePath = m_mapManager->getDataPath() + "\\" + "Cache" + "\\" + "ST-" + to_string(m_quadrantId.getGridStepInWU()) + "_SZ-" + to_string(m_quadrantId.getNumVerticesPerSize()) + "\\L-" + string(level);
		if (!fs::exists(cachePath))
		{
			fs::create_directories(cachePath);
		}
		string cacheFileName = "X-" + to_string(m_quadrantId.getLowerXGridVertex()) + "_Z-" + to_string(m_quadrantId.getLowerZGridVertex());
		string cacheFileNameFull = cachePath + "\\" + cacheFileName;
		if (fs::exists(cacheFileNameFull))
		{
			FILE* inFile = nullptr;
			errno_t err = fopen_s(&inFile, cacheFileNameFull.c_str(), "rb");
			if (err != 0)
				throw(MapManagerException(__FUNCTION__, string("Open " + cacheFileNameFull + " in errore - Err=" + to_string(err)).c_str())); 
			
			if (fread(shortBuffer, 1, 1, inFile) != 1)	// "0"
				throw(MapManagerException(__FUNCTION__, string("Read error 1!").c_str()));
				
			serializeToByteStream<size_t>(m_vectGridVertices.size(), shortBuffer, size);
			if (fread(shortBuffer, size, 1, inFile) != 1)
				throw(MapManagerException(__FUNCTION__, string("Read error 2!").c_str()));
			size_t vectSize = deserializeFromByteStream<size_t>(shortBuffer, size);
				
			size_t streamBufferSize = vectSize * serializedVertexSize;
			BYTE* streamBuffer = (BYTE*)calloc(1, streamBufferSize);
			if (streamBuffer == nullptr)
				throw(MapManagerException(__FUNCTION__, string("Allocation error!").c_str()));

			//size_t num = 0;
			//size_t sizeRead = 0;
			//while (!feof(inFile))
			//{
			//	size_t i = fread(streamBuffer + sizeRead, serializedVertexSize, 1, inFile);
			//	sizeRead += serializedVertexSize;
			//	num++;
			//}
			size_t s = fread(streamBuffer, streamBufferSize, 1, inFile);
			//size_t s = fread(streamBuffer, serializedVertexSize, vectSize, inFile);
			//if (s != 1)
			//{
			//	free(streamBuffer);
			//	int i = feof(inFile);
			//	i = ferror(inFile);
			//	throw(MapManagerException(__FUNCTION__, string("Read error 3! feof " + to_string(feof(inFile)) + " ferror " + to_string(ferror(inFile))).c_str()));
			//}
				
			fclose(inFile);

			BYTE* movingStreamBuffer = streamBuffer;
			BYTE* endOfBuffer = streamBuffer + streamBufferSize;
			while (movingStreamBuffer < endOfBuffer)
			{
				m_vectGridVertices.push_back(GridVertex(movingStreamBuffer, size));	// circa 2 sec
				movingStreamBuffer += size;
			}

			free(streamBuffer);

			if (m_vectGridVertices.size() != vectSize)
				throw(MapManagerException(__FUNCTION__, string("Sequence error 4!").c_str()));

			if (viewerPosX != 0 && viewerPosZ != 0)
			{
				viewerPosX = m_mapManager->calcNextCoordOnTheGridInWUs(viewerPosX);
				viewerPosZ = m_mapManager->calcNextCoordOnTheGridInWUs(viewerPosZ);
			}
			
			m_populated = true;

			return;
		}

		std::vector<SQLInterface::GridVertex> worldVertices;
		float lowerXGridVertex = m_quadrantId.getLowerXGridVertex();
		float lowerZGridVertex = m_quadrantId.getLowerZGridVertex();
		float gridStepinWU = m_quadrantId.getGridStepInWU();
		m_mapManager->getVertices(lowerXGridVertex, lowerZGridVertex, TheWorld_MapManager::MapManager::anchorType::upperleftcorner, m_quadrantId.getNumVerticesPerSize(), m_quadrantId.getNumVerticesPerSize(), worldVertices, gridStepinWU, m_quadrantId.getLevel());

		for (int z = 0; z < m_quadrantId.getNumVerticesPerSize(); z++)			// m_heightMapImage->get_height()
			for (int x = 0; x < m_quadrantId.getNumVerticesPerSize(); x++)		// m_heightMapImage->get_width()
			{
				SQLInterface::GridVertex& v = worldVertices[z * m_quadrantId.getNumVerticesPerSize() + x];
				m_vectGridVertices.push_back(GridVertex(v.posX(), v.altitude(), v.posZ(), m_quadrantId.getLevel()));
			}

		if (viewerPosX != 0 && viewerPosZ != 0)
		{
			viewerPosX = m_mapManager->calcNextCoordOnTheGridInWUs(viewerPosX);
			viewerPosZ = m_mapManager->calcNextCoordOnTheGridInWUs(viewerPosZ);
		}

		size_t vectSize = m_vectGridVertices.size();

		size_t streamBufferSize = vectSize * serializedVertexSize;
		BYTE* streamBuffer = (BYTE*)calloc(1, streamBufferSize);
		if (streamBuffer == nullptr)
			throw(MapManagerException(__FUNCTION__, string("Allocation error!").c_str()));

		size_t sizeToWrite = 0;
		for (size_t idx = 0; idx < vectSize; idx++)
		{
			m_vectGridVertices[idx].serialize(streamBuffer + sizeToWrite, size);
			sizeToWrite += size;
		}

		FILE* outFile = nullptr;
		errno_t err = fopen_s(&outFile, cacheFileNameFull.c_str(), "wb");
		if (err != 0)
		{
			free(streamBuffer);
			throw(MapManagerException(__FUNCTION__, string("Open " + cacheFileNameFull + " in errore - Err=" + to_string(err)).c_str()));
		}

		if (fwrite("0", 1, 1, outFile) != 1)
		{
			free(streamBuffer);
			throw(MapManagerException(__FUNCTION__, string("Write error 1!").c_str()));
		}

		serializeToByteStream<size_t>(vectSize, shortBuffer, size);
		if (fwrite(shortBuffer, size, 1, outFile) != 1)
		{
			free(streamBuffer);
			throw(MapManagerException(__FUNCTION__, string("Write error 2!").c_str()));
		}

		if (fwrite(streamBuffer, sizeToWrite, 1, outFile) != 1)
		{
			free(streamBuffer);
			throw(MapManagerException(__FUNCTION__, string("Write error 3!").c_str()));
		}

		fclose(outFile);

		free(streamBuffer);

		m_populated = true;

		return;
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
