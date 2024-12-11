#pragma once

#include "DBInterface.h"
#include "MapManagerException.h"
#include "json/json.h"

#include <string>
#include <memory>
#include <thread>

namespace TheWorld_MapManager
{
	class DBJson : public DBInterface
	{
	public:
		DBJson(DBType dbt, const char* dataPath, bool consoleDebugMode = false);
		~DBJson();

		virtual std::string readParam(std::string paranName);
		virtual void beginTransaction(void);
		virtual void endTransaction(bool commit = true);
		virtual __int64 addWDAndVertices(WorldDefiner* pWD, std::vector<GridVertex>& vectGridVertices);
		virtual bool eraseWD(__int64 wdRowid);
		virtual void updateAltitudeOfVertex(__int64 vertexRowid, float altitude);
		virtual void clearVerticesMarkedForUpdate(void);
		virtual void getVertex(__int64 vertexRowid, GridVertex& gridVertex, int level);
		virtual void getVertex(GridVertex& gridVertex);
		virtual void getVertices(float minX, float maxX, float minZ, float maxZ, std::vector<GridVertex>& vectGridVertices, int level);
		virtual size_t eraseVertices(float minX, float maxX, float minZ, float maxZ, int level);
		virtual bool getWD(float posX, float posZ, int level, WDType type, WorldDefiner& WD);
		virtual bool getWD(__int64 wdRowid, WorldDefiner& WD);
		virtual void getWDRowIdForVertex(__int64 vertexRowid, std::vector<__int64>& vectWDRowId);
		virtual bool getFirstModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD);
		virtual bool getNextModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD);
		virtual std::string getQuadrantHash(float gridStepInWU, size_t numVerticesPerSize, size_t level, float lowerXGridVertex, float lowerZGridVertex, enum class DBInterface::QuadrantStatus& status, enum class QuadrantVertexStoreType& vertexStoreType);
		virtual bool writeQuadrantToDB(TheWorld_Utils::MeshCacheBuffer& cache, TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData& cacheQuadrantData, std::string strBuffer, enum class QuadrantVertexStoreType vertexStoreType, bool& stop);
		virtual void readQuadrantFromDB(TheWorld_Utils::MeshCacheBuffer& cache, std::string& meshId, std::string& strBuffer, enum class DBInterface::QuadrantStatus& status, enum class QuadrantVertexStoreType& vertexStoreType, TheWorld_Utils::TerrainEdit& terrainEdit);
		virtual void finalizeDB(void);
		virtual void importExternalQuadrants(float gridStepInWU, size_t numVerticesPerSize, int level);
		virtual void writeExternalQuadrants(float gridStepInWU, size_t numVerticesPerSize, int level, std::map<TheWorld_Utils::QuadrantPos, TheWorld_Utils::TerrainEdit>& externalQuadrants);
		virtual void getAllQuadrants(std::string _cacheDir, std::string mapName, float gridStepInWU, size_t numVerticesPerSize, int level, std::map<TheWorld_Utils::QuadrantPos, TheWorld_Utils::MeshCacheBuffer>& diskCacheMap, std::time_t& fromFileTimeType);
		virtual std::chrono::system_clock::time_point getTextureRectTime(TheWorld_Utils::MapTextureRectPos& textureRectPos, size_t numVerticesPerSize, float gridStepInWU);
		virtual void setTextureRectTime(std::chrono::system_clock::time_point textureRectTime, TheWorld_Utils::MapTextureRectPos& textureRectPos, size_t numVerticesPerSize, float gridStepInWU);

	private:
		bool completeQuadrantDataWithDefaultValues(float gridStepInWU, size_t numVerticesPerSize, int level, Json::Value& root);
		void terrainDataToQuadrant(Json::Value& quadrant, const TheWorld_Utils::QuadrantPos& pos, const TheWorld_Utils::TerrainEdit& terrainEdit);
		std::string getTextureRectDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level);
		std::string getQuadrantDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level);
		std::string getQuadrantFilePath(float gridStepInWU, size_t numVerticesPerSize, size_t level, float lowerXGridVertex, float lowerZGridVertex);
		std::string getQuadrantImportDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level);
		//std::string getQuadrantWorkDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level);

	private:
		static std::string g_DBDir;
		static std::string g_confFilePath;
	};
}

