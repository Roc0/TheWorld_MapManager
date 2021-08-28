#include "pch.h"

#define _USE_MATH_DEFINES // for C++

#include "assert.h"

#include "json/json.h"
#include <iostream>
#include <fstream>

#include "MapManager.h"
#include "DBSQLLite.h"

#include "shapefil.h"

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
		vector<SQLInterface::MapVertex> v;
		
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

				SQLInterface::MapVertex mapv((float)(x * g_distanceFromVerticesInWU), (float)(z * g_distanceFromVerticesInWU), WD.getLevel());
				v.push_back(mapv);

				if (debugMode() && fmod(numVertices, 1024 * 1000) == 0) debugUtil.printVariablePartOfLine(numVertices);
			}
		}

		if (debugMode()) debugUtil.printVariablePartOfLine(numVertices);

		// Adding / updating WD to DB : this action will add / update all affected point
		__int64 rowid = m_SqlInterface->addWD(WD, v);

		if (instrumented()) clock.printDuration(__FUNCTION__);

		if (debugMode()) debugUtil.printNewLine();

		return rowid;
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
		float altitude = 0.0;
		
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
	
	void MapManager::Test(void)
	{
		TimerMs clock; // Timer<milliseconds, steady_clock>
		if (instrumented()) clock.tick();

		struct point
		{
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
		pointMap mapAltidues;
		pointMap::iterator it;
		
		// *************************************************************************
		// W A R N I N G : Z axis  is UP (Blender uses right hand coordinate system)
		// *************************************************************************

		//string filePath = "D:\\TheWorld\\Client\\Italy_shapefile\\it_10km.shp";
		// https://www.youtube.com/watch?v=lP52QKda3mw
		string filePath = "D:\\TheWorld\\Prove\\untitled2.shp";

		SHPHandle handle = SHPOpen(filePath.c_str(), "rb");
		if (handle <= 0)
		{
			throw(MapManagerException(__FUNCTION__, string("File " + filePath + " not found").c_str()));
		}
		
		string fileName = filePath.substr(filePath.find_last_of("\\") + 1, (filePath.find_last_of(".") - filePath.find_last_of("\\") - 1));
		string outfilePath = filePath.substr(0, filePath.find_last_of("\\") + 1) + fileName + ".txt";
		ofstream outFile;
		outFile.open(outfilePath);

		int shapeType, nEntities;
		double adfMinBound[4], adfMaxBound[4];
		SHPGetInfo(handle, &nEntities, &shapeType, adfMinBound, adfMaxBound);

		outFile << "Shape Type: " << to_string(shapeType) << "\n";
		outFile << "Min Bound X: " << to_string(adfMinBound[0]) << "Min Bound Y: " << to_string(adfMinBound[1]) << "Min Bound Z: " << to_string(adfMinBound[2]) << "Min Bound M: " << to_string(adfMinBound[3]) << "\n";
		outFile << "Max Bound X: " << to_string(adfMaxBound[0]) << "Max Bound Y: " << to_string(adfMaxBound[1]) << "Max Bound Z: " << to_string(adfMaxBound[2]) << "Max Bound M: " << to_string(adfMaxBound[3]) << "\n";
		outFile << "Size X: " << to_string(adfMaxBound[0] - adfMinBound[0]) << "Size Y: " << to_string(adfMaxBound[1] - adfMinBound[1]) << "Size Z: " << to_string(adfMaxBound[2] - adfMinBound[2]) << "Size M: " << to_string(adfMaxBound[3] - adfMinBound[3]) << "\n";

		debugUtils debugUtil;
		debugUtils debugUtil1;
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
				outFile << "Entity: " << to_string(i) << " - Num Vertices: " << to_string(psShape->nVertices) << "\n";

				double* x = psShape->padfX;
				double* y = psShape->padfY;
				double* z = psShape->padfZ;
				double* m = psShape->padfM;

				string s = "Dumping vertices(" + to_string(psShape->nVertices) + "): ";
				if (debugMode()) debugUtil1.printFixedPartOfLine(classname(), __FUNCTION__, s.c_str(), &debugUtil);
				if (debugMode()) debugUtil1.printNewLine();
				for (int v = 0; v < psShape->nVertices; v++)
				{
					outFile << "Vertex X: " << to_string(x[v]) << " - Vertex Y: " << to_string(y[v]) << " - Vertex Z: " << to_string(z[v]) << " - Vertex M: " << to_string(m[v]) << "\n";

					point p = { x[v], y[v] };

					/*if (x[v] == 1195425.1762949340 && z[v] == 869.21911621093750)
					{
						outFile << "eccolo" << endl;
					}*/

					it = mapAltidues.find(p);
					if (it == mapAltidues.end())
					{
						vector<double> altitudes;
						altitudes.push_back(z[v]);
						mapAltidues[p] = altitudes;
					}
					else
					{
						mapAltidues[p].push_back(z[v]);
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

		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Dumping altitudes for every point of the plane: ", &debugUtil1);
		int idxPoint = 0;
		for (it = mapAltidues.begin(); it != mapAltidues.end(); it++)
		{
			idxPoint++;
			for (int idxAltitudes = 0; idxAltitudes < it->second.size(); idxAltitudes++)
			{
				outFile << "Point " << to_string(idxPoint).c_str() << " - Vertex X: " << to_string(it->first.x) << " - Vertex Y: " << to_string(it->first.y) << " - Altitude: " << to_string(it->second[idxAltitudes]) << endl;
			}
			if (debugMode() && fmod(idxPoint, 1000) == 0) debugUtil.printVariablePartOfLine(idxPoint);
		}
		if (debugMode()) debugUtil.printVariablePartOfLine(idxPoint);

		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Dumping max altitude for every point of the plane: ", &debugUtil);
		idxPoint = 0;
		for (it = mapAltidues.begin(); it != mapAltidues.end(); it++)
		{
			idxPoint++;
			double maxAltitude = 0;
			for (int idxAltitudes = 0; idxAltitudes < it->second.size(); idxAltitudes++)
			{
				if (it->second[idxAltitudes] > maxAltitude)
					maxAltitude = it->second[idxAltitudes];
			}
			outFile << "Point " << to_string(idxPoint).c_str() << " - Vertex X: " << to_string(it->first.x) << " - Vertex Y: " << to_string(it->first.y) << " - NumAltitudes: " << to_string(it->second.size()) << " - MaxAltitude: " << to_string(maxAltitude) << endl;
			if (debugMode() && fmod(idxPoint, 1000) == 0) debugUtil.printVariablePartOfLine(idxPoint);
			if (it->second.size() != 1)
				outFile << endl;
		}
		if (debugMode()) debugUtil.printVariablePartOfLine(idxPoint);

		outFile.close();

		if (instrumented()) clock.printDuration(__FUNCTION__);
	}
}
