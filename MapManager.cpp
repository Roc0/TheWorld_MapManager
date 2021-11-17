#include "pch.h"

#define _USE_MATH_DEFINES // for C++

#include "assert.h"

#include "json/json.h"
#include <iostream>
#include <fstream>

#include "MapManager.h"
#include "DBSQLLite.h"

#include "shapefil.h"
#include <proj.h>

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

	__int64 MapManager::addWD(WorldDefiner& WD)
	{
		debugUtils debugUtil;
		TimerMs clock; // Timer<milliseconds, steady_clock>
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
		// they are spaced by a number of WU equal to gridStepInWU (g_gridStepInWU)
		calcSquareGridMinMax(minAOEX, maxAOEX, minAOEZ, maxAOEZ, minGridPosX, maxGridPosX, minGridPosZ, maxGridPosZ, gridStepInWU);

		int gridSize = (maxGridPosX - minGridPosX + 1) * (maxGridPosZ - minGridPosZ + 1);
		vector<SQLInterface::MapVertex> v;
		
		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Computing affected vertices by WorldDefiner: ");

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

				SQLInterface::MapVertex mapv(float(x) * gridStepInWU, float(z) * gridStepInWU, WD.getLevel());
				v.push_back(mapv);

				if (debugMode() && fmod(numVertices, 1024 * 1000) == 0) debugUtil.printVariablePartOfLine(numVertices);
			}
		}

		if (debugMode()) debugUtil.printVariablePartOfLine(numVertices);

		// Adding / updating WD to DB : this action will add / update all affected point
		__int64 rowid = m_SqlInterface->addWDAndVertices(&WD, v);

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (debugMode()) debugUtil.printNewLine();

		return rowid;
	}

	// Return a square grid with a number of vertices for every size multiple of g_DBGrowingBlockVertexNumber, they are spaced by a number of WU equal to g_gridStepInWU
	// every point of the grid is defined by its X and Z coord expressed in WU in whole numbers 
	void MapManager::calcSquareGridMinMax(float minAOEX, float maxAOEX, float minAOEZ, float maxAOEZ, int& minGridPosX, int& maxGridPosX, int& minGridPosZ, int& maxGridPosZ, float& gridStepInWU)
	{
		if (minAOEX < 0 && -minAOEX < g_DBGrowingBlockVertexNumber)
			minAOEX = -g_DBGrowingBlockVertexNumber;

		if (maxAOEX > 0 && maxAOEX < g_DBGrowingBlockVertexNumber)
			maxAOEX = g_DBGrowingBlockVertexNumber;

		if (minAOEZ < 0 && -minAOEZ < g_DBGrowingBlockVertexNumber)
			minAOEZ = -g_DBGrowingBlockVertexNumber;

		if (maxAOEZ > 0 && maxAOEZ < g_DBGrowingBlockVertexNumber)
			maxAOEZ = g_DBGrowingBlockVertexNumber;

		// transform: WU ==> grid step
		minAOEX /= g_gridStepInWU;
		maxAOEX /= g_gridStepInWU;
		minAOEZ /= g_gridStepInWU;
		maxAOEZ /= g_gridStepInWU;

		minGridPosX = int(minAOEX / g_DBGrowingBlockVertexNumber) * g_DBGrowingBlockVertexNumber;
		if (minGridPosX < 0 && minGridPosX != minAOEX)
			minGridPosX -= g_DBGrowingBlockVertexNumber;

		maxGridPosX = int(maxAOEX / g_DBGrowingBlockVertexNumber) * g_DBGrowingBlockVertexNumber;
		if (maxGridPosX > 0 && maxGridPosX != maxAOEX)
			maxGridPosX += g_DBGrowingBlockVertexNumber;

		minGridPosZ = int(minAOEZ / g_DBGrowingBlockVertexNumber) * g_DBGrowingBlockVertexNumber;
		if (minGridPosZ < 0 && minGridPosZ != minAOEZ)
			minGridPosZ -= g_DBGrowingBlockVertexNumber;

		maxGridPosZ = int(maxAOEZ / g_DBGrowingBlockVertexNumber) * g_DBGrowingBlockVertexNumber;
		if (maxGridPosZ > 0 && maxGridPosZ != maxAOEZ)
			maxGridPosZ += g_DBGrowingBlockVertexNumber;

		gridStepInWU = g_gridStepInWU;
	}
	
	void MapManager::getSquareGrid(float minAOEX, float maxAOEX, float minAOEZ, float maxAOEZ, vector<gridPoint>& grid, int& numPointX, int& numPointZ, float& gridStepInWU)
	{
		int minGridPosX = 0;
		int maxGridPosX = 0;
		int minGridPosZ = 0;
		int maxGridPosZ = 0;

		calcSquareGridMinMax(minAOEX, maxAOEX, minAOEZ, maxAOEZ, minGridPosX, maxGridPosX, minGridPosZ, maxGridPosZ, gridStepInWU);

		numPointX = maxGridPosX - minGridPosX + 1;
		numPointZ = maxGridPosZ - minGridPosZ + 1;

		grid.clear();

		for (int x = minGridPosX; x <= maxGridPosX; x++)
		{
			for (int z = minGridPosZ; z <= maxGridPosZ; z++)
			{
				gridPoint p;
				p.x = float(x) * gridStepInWU;
				p.z = float(z) * gridStepInWU;
				grid.push_back(p);
			}
		}
	}

	bool MapManager::eraseWD(__int64 wdRowid)
	{
		debugUtils debugUtil;
		TimerMs clock; // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();

		bool bDeleted = m_SqlInterface->eraseWD(wdRowid);

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (debugMode()) debugUtil.printNewLine();

		return bDeleted;
	}

	bool MapManager::eraseWD(float posX, float posZ, int level, WDType type)
	{
		debugUtils debugUtil;
		TimerMs clock; // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();
		bool bDeleted = false;

		WorldDefiner wd;
		bool bFound = m_SqlInterface->getWD(posX, posZ, level, type, wd);
		if (bFound)
			bDeleted = m_SqlInterface->eraseWD(wd.getRowid());

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (debugMode()) debugUtil.printNewLine();

		return bDeleted;
	}

	bool MapManager::eraseWD(WorldDefiner& WD)
	{
		debugUtils debugUtil;
		TimerMs clock; // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();

		bool bDeleted = eraseWD(WD.getPosX(), WD.getPosZ(), WD.getLevel(), WD.getType());

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (debugMode()) debugUtil.printNewLine();

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
		SQLInterface::MapVertex mapVertex;
		vector<WorldDefiner> wdMap;
		bool bFound = m_SqlInterface->getFirstModfiedVertex(mapVertex, wdMap);
		while (bFound)
		{
			idx++;

			float altitude = computeAltitude(mapVertex, wdMap);
			m_SqlInterface->updateAltitudeOfVertex(mapVertex.rowid(), altitude);

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

		m_SqlInterface->clearVerticesMarkedForUpdate();
			
			
		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (debugMode()) debugUtil.printNewLine();

		/*
		* Close Transaction
		*/
		m_SqlInterface->endTransaction();
	}

	float MapManager::computeAltitude(SQLInterface::MapVertex& mapVertex, std::vector<WorldDefiner>& wdMap)
	{
		float altitude = mapVertex.initialAltitude();
		
		// Scanning all WDs
		int numWDs = (int)wdMap.size();
		float distanceFromWD = -1;
		for (int idx = 0; idx < numWDs; idx++)
		{
			float AOE = wdMap[idx].getAOE();
			distanceFromWD = getDistance(mapVertex.posX(), mapVertex.posZ(), wdMap[idx].getPosX(), wdMap[idx].getPosZ());
			if (distanceFromWD <= AOE)	// Vertex is influenced by current WD
			{
				switch (wdMap[idx].getType())
				{
				case WDType::elevator:
					altitude += computeAltitudeElevator(mapVertex, wdMap[idx], distanceFromWD);
					break;
				default:
					break;
				}
			}
		}

		return altitude;
	}

	float MapManager::computeAltitudeElevator(SQLInterface::MapVertex& mapVertex, WorldDefiner& wd, float distanceFromWD)
	{
		float altitude = 0.0;

		if (distanceFromWD == -1)
			distanceFromWD = getDistance(mapVertex.posX(), mapVertex.posZ(), wd.getPosX(), wd.getPosZ());
		
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
		SQLInterface::MapVertex mapVertex;
		vector<WorldDefiner> wdMap;
		bool bFound = m_SqlInterface->getFirstModfiedVertex(mapVertex, wdMap);
		int idx = 0;
		while (bFound)
		{
			idx++;
			bFound = m_SqlInterface->getNextModfiedVertex(mapVertex, wdMap);
		}
		return idx;
	}
	
	void MapManager::DumpDB(void)
	{
		// RMTODO
	}

	void MapManager::LoadGISMap(const char* fileInput, bool writeReport, float metersInWU, int level)
	{
		// ***************************************************************************************************
		// W A R N I N G : assuming coordinates are in projected coordinates EPSG 3857 where the unit is metre
		// ***************************************************************************************************

		TimerMs clock; // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();

		debugUtils debugUtil;
		debugUtils debugUtil1;

		// *************************************************************************
		// W A R N I N G : Z axis  is UP (Blender uses right hand coordinate system)
		// *************************************************************************

		//string filePath = "D:\\TheWorld\\Client\\Italy_shapefile\\it_10km.shp";
		// https://www.youtube.com/watch?v=lP52QKda3mw
		string filePath = fileInput;

		SHPHandle handle = SHPOpen(filePath.c_str(), "rb");
		if (handle <= 0)
		{
			throw(MapManagerException(__FUNCTION__, string("File " + filePath + " not found").c_str()));
		}
		
		string fileName, outfilePath;
		ofstream outFile;
		if (writeReport)
		{
			fileName = filePath.substr(filePath.find_last_of("\\") + 1, (filePath.find_last_of(".") - filePath.find_last_of("\\") - 1));
			outfilePath = filePath.substr(0, filePath.find_last_of("\\") + 1) + fileName + ".txt";
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

		// transfom: meters ==> WUs
		float minAOEX = (float)adfMinBound[0] / metersInWU;
		float maxAOEX = (float)adfMaxBound[0] / metersInWU;
		float minAOEZ = (float)adfMinBound[1] / metersInWU;
		float maxAOEZ = (float)adfMaxBound[1] / metersInWU;

		vector<gridPoint> grid;
		int numPointX = 0;
		int numPointZ = 0;
		float gridStepInWU = 0.0;

		// we need to calculate the grid so that it is expressed of square patches with a number of vertices for every size equal to g_DBGrowingBlockVertexNumber
		// so the grid has a number of vertices equal to a multiple of g_DBGrowingBlockVertexNumber, they are spaced by a number of WU equal to gridStepInWU (g_gridStepInWU)
		getSquareGrid(minAOEX, maxAOEX, minAOEZ, maxAOEZ, grid, numPointX, numPointZ, gridStepInWU);

		if (writeReport)
		{
			string s = "Dumping grid - Num points: " + to_string(numPointX * numPointZ) + " (" + to_string(numPointX) + " x " + to_string(numPointZ) + ") - Point: ";

			outFile << endl << "Inizio grid Num points: " << to_string(numPointX * numPointZ) << " (" << to_string(numPointX) << " x " << to_string(numPointZ) << ")" << endl;
			if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, s.c_str());
			int idxPoint = 0, row = 0, col = 0;
			for (int x = 0; x < numPointX; x++)
			{
				row++;
				col = 0;
				for (int z = 0; z < numPointZ; z++)
				{
					if (z == 0)
						outFile << "Row: " << to_string(row) << " - X: " << to_string(grid[idxPoint].x) << endl;

					col++;
					outFile << "   Col: " << to_string(col) << " - Z: " << to_string(grid[idxPoint].z) << endl;
					idxPoint++;
					if (debugMode() && fmod(idxPoint + 1, 1000) == 0) debugUtil.printVariablePartOfLine(idxPoint);
				}
			}
			outFile << endl << "Fine grid" << endl;

			if (debugMode())
			{
				debugUtil.printVariablePartOfLine(idxPoint);
				debugUtil.printNewLine();
			}
		}
			
		struct point
		{
			// use to keep the map sorted by x, y
			bool operator()(const point& p1, const point& p2) const
			{
				if (p1.x < p2.x)
					return true;
				if (p1.x > p2.x)
					return false;
				else
					return p1.y < p2.y;
			}
			double x;
			double y;
		};

		typedef map<point, vector<double>, point> pointMap;
		pointMap altiduesMap;
		pointMap::iterator it;

		string s = "Looping into entities of: " + filePath + " - Entities(" + to_string(nEntities) + "): ";
		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, s.c_str());
		for (int i = 0; i < nEntities; i++)
		{
			SHPObject * psShape = SHPReadObject(handle, i);

			// Read only polygons, and only those without holes
			if (
					((psShape->nSHPType == SHPT_MULTIPOINT || psShape->nSHPType == SHPT_MULTIPOINTZ || psShape->nSHPType == SHPT_MULTIPOINTM) && psShape->nParts == 0)
					//|| ((psShape->nSHPType == SHPT_POLYGON || psShape->nSHPType == SHPT_POLYGONZ || psShape->nSHPType == SHPT_POLYGONM) && psShape->nParts == 1)
				)
			{
				if (writeReport)
					outFile << "Entity: " << to_string(i) << " - Num Vertices: " << to_string(psShape->nVertices) << "\n";

				double* x = psShape->padfX;
				double* y = psShape->padfY;
				double* z = psShape->padfZ;
				double* m = psShape->padfM;

				if (debugMode())
				{
					string s = "Dumping vertices(" + to_string(psShape->nVertices) + "): ";
					debugUtil1.printFixedPartOfLine(classname(), __FUNCTION__, s.c_str(), &debugUtil);
					debugUtil1.printNewLine();
				}
				for (int v = 0; v < psShape->nVertices; v++)
				{
					if (writeReport)
						outFile << "Vertex X: " << to_string(x[v]) << " - Vertex Y: " << to_string(y[v]) << " - Vertex Z: " << to_string(z[v]) << " - Vertex M: " << to_string(m[v]) << "\n";

					// For every point we colect all its alitudes (detecting if it can have more than one)
					{
						point p = { x[v], y[v] };
						/*if (x[v] == 1195425.1762949340 && z[v] == 869.21911621093750)
						{
							outFile << "eccolo" << endl;
						}*/
						it = altiduesMap.find(p);
						if (it == altiduesMap.end())
						{
							vector<double> altitudes;
							altitudes.push_back(z[v]);
							altiduesMap[p] = altitudes;
						}
						else
						{
							altiduesMap[p].push_back(z[v]);
						}
					}

					if (debugMode() && fmod(v + 1, 1000) == 0) debugUtil1.printVariablePartOfLine(v + 1);
				}
				if (debugMode()) debugUtil1.printVariablePartOfLine(psShape->nVertices);
			}

			SHPDestroyObject(psShape);

			if (debugMode() && fmod(i + 1, 1000) == 0) debugUtil.printVariablePartOfLine(i + 1);
		}
		if (debugMode()) debugUtil.printVariablePartOfLine(nEntities);

		SHPClose(handle);

		/*if (writeReport)
		{
			if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Dumping altitudes for every point of the plane: ", &debugUtil1);
			int idxPoint = 0;
			for (it = altiduesMap.begin(); it != altiduesMap.end(); it++)
			{
				idxPoint++;
				for (int idxAltitudes = 0; idxAltitudes < it->second.size(); idxAltitudes++)
				{
					outFile << "Point " << to_string(idxPoint).c_str() << " - Vertex X: " << to_string(it->first.x) << " - Vertex Y: " << to_string(it->first.y) << " - Altitude: " << to_string(it->second[idxAltitudes]) << endl;
				}
				if (debugMode() && fmod(idxPoint, 1000) == 0) debugUtil.printVariablePartOfLine(idxPoint);
			}
			if (debugMode()) debugUtil.printVariablePartOfLine(idxPoint);
		}*/

		// Test
		if (writeReport) 
		{
			if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Dumping Row / Column for every point of the plane: ", &debugUtil1);

			int idxPoint = 0;
			outFile << endl << "Inizio test" << endl;
			int row = 0, col = 0, maxCols = 0;
			double lastX = 0.0;
			for (it = altiduesMap.begin(); it != altiduesMap.end(); it++)
			{
				if (it->second.size() != 1)
					throw(MapManagerException(__FUNCTION__, "Found point with a number of altitudes not equal 1"));

				idxPoint++;

				if (it->first.x != lastX)
				{
					lastX = it->first.x;
					row++;
					if (col > maxCols)
						maxCols = col;
					col = 0;
					outFile << "Row: " << to_string(row) << " - Vertex X: " << to_string(it->first.x) << endl;
				}
				col++;
				outFile << "   Col: " << to_string(col) << " - Vertex Y: " << to_string(it->first.y) << " - Altitude: " << it->second[0] << endl;

				if (debugMode() && fmod(idxPoint, 1000) == 0) debugUtil.printVariablePartOfLine(idxPoint);
			}
			outFile << endl << "Fine test - Rows: " << to_string(row) << " - MaxCols: " << to_string(maxCols) << endl;

			if (debugMode()) debugUtil.printVariablePartOfLine(idxPoint);
		}

		vector<SQLInterface::MapVertex> vectMapVertices;

		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Dumping max altitude for every point of the plane: ", &debugUtil);
		int idxPoint = 0;
		for (it = altiduesMap.begin(); it != altiduesMap.end(); it++)
		{
			if (it->second.size() != 1)
				throw(MapManagerException(__FUNCTION__, "Found point with a number of altitudes not equal 1"));

			idxPoint++;
			double maxAltitude = 0;
			for (int idxAltitudes = 0; idxAltitudes < it->second.size(); idxAltitudes++)
			{
				if (it->second[idxAltitudes] > maxAltitude)
					maxAltitude = it->second[idxAltitudes];
			}
			if (writeReport)
				outFile << "Point " << to_string(idxPoint).c_str() << " - Vertex X: " << to_string(it->first.x) << " - Vertex Y: " << to_string(it->first.y) << " - NumAltitudes: " << to_string(it->second.size()) << " - MaxAltitude: " << to_string(maxAltitude) << endl;
			if (debugMode() && fmod(idxPoint, 1000) == 0) debugUtil.printVariablePartOfLine(idxPoint);
			
			SQLInterface::MapVertex mapVertex((float)(it->first.x) / metersInWU, (float)(it->first.y) / metersInWU, (float)maxAltitude / g_gridStepInWU, level);
			vectMapVertices.push_back(mapVertex);
		}
		if (debugMode()) debugUtil.printVariablePartOfLine(idxPoint);

		if (writeReport)
			outFile.close();

		//__int64 rowid = m_SqlInterface->addWDAndVertices(NULL, vectMapVertices);

		
		if (instrumented()) clock.printDuration(__FUNCTION__);
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
}
