#include "pch.h"
#include "DBJson.h"

#include "json/json.h"
#include <fstream>

//using namespace std;

namespace TheWorld_MapManager
{
	std::string DBJson::g_filePath;

	DBJson::DBJson(DBType _dbt, const char* _dataPath, bool consoleDebugMode) : DBInterface(_dbt, _dataPath, consoleDebugMode)
	{
		std::recursive_mutex& exclusiveDBAccessMutex = DBInterface::getExclusiveDBAccessMutex();
		std::lock_guard<std::recursive_mutex> lock(exclusiveDBAccessMutex);

		if (g_filePath.length() == 0)
		{
			Json::Value root;
			std::ifstream jsonFile(dataPath() + "\\Conf\\DBJson.json");
			jsonFile >> root;
			g_filePath = dataPath() + "\\" + root["DBFilePath"].asString();
		}
	}

	DBJson::~DBJson()
	{
	}

	std::string DBJson::readParam(std::string paranName)
	{
		std::recursive_mutex& exclusiveDBAccessMutex = DBInterface::getExclusiveDBAccessMutex();
		std::lock_guard<std::recursive_mutex> lock(exclusiveDBAccessMutex);

		Json::Value root;
		std::ifstream jsonFile(g_filePath);
		jsonFile >> root;
		return root[paranName].asString();

		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return "";
	}

	void DBJson::beginTransaction(void)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
	}

	void DBJson::endTransaction(bool commit)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
	}

	__int64 DBJson::addWDAndVertices(WorldDefiner* pWD, std::vector<GridVertex>& vectGridVertices)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return -1;
	}

	bool DBJson::eraseWD(__int64 wdRowid)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return false;
	}

	void DBJson::updateAltitudeOfVertex(__int64 vertexRowid, float altitude)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
	}

	void DBJson::clearVerticesMarkedForUpdate(void)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
	}

	void DBJson::getVertex(__int64 vertexRowid, GridVertex& gridVertex, int level)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
	}

	void DBJson::getVertex(GridVertex& gridVertex)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
	}

	void DBJson::getVertices(float minX, float maxX, float minZ, float maxZ, std::vector<GridVertex>& vectGridVertices, int level)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
	}

	size_t DBJson::eraseVertices(float minX, float maxX, float minZ, float maxZ, int level)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return 0;
	}

	bool DBJson::getWD(float posX, float posZ, int level, WDType type, WorldDefiner& WD)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return false;
	}

	bool DBJson::getWD(__int64 wdRowid, WorldDefiner& WD)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return false;
	}

	void DBJson::getWDRowIdForVertex(__int64 vertexRowid, std::vector<__int64>& vectWDRowId)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
	}

	bool DBJson::getFirstModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return false;
	}

	bool DBJson::getNextModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return false;
	}

	std::string DBJson::getQuadrantHash(float gridStep, size_t vertxePerSize, size_t level, float posX, float posZ, enum class DBInterface::QuadrantStatus& status, enum class QuadrantVertexStoreType& vertexStoreType)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return "";
	}

	bool DBJson::writeQuadrantToDB(TheWorld_Utils::MeshCacheBuffer& cache, TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData& cacheQuadrantData, std::string strBuffer, enum class QuadrantVertexStoreType vertexStoreType, bool& stop)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
		return false;
	}

	void DBJson::readQuadrantFromDB(TheWorld_Utils::MeshCacheBuffer& cache, std::string& meshId, std::string& strBuffer, enum class DBInterface::QuadrantStatus& status, enum class QuadrantVertexStoreType& vertexStoreType, TheWorld_Utils::TerrainEdit& terrainEdit)
	{
		throw(MapManagerExceptionDBExceptionNotImpl(__FUNCTION__));
	}

	void DBJson::finalizeDB(void)
	{
	}
}