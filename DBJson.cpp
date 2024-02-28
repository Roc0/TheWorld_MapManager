#include "pch.h"
#include "DBJson.h"
#include "profiler.h"

#include "base64.h"

#include "json/json.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace TheWorld_MapManager
{
	std::string DBJson::g_DBDir;
	std::string DBJson::g_confFilePath;

	DBJson::DBJson(DBType _dbt, const char* _dataPath, bool consoleDebugMode) : DBInterface(_dbt, _dataPath, consoleDebugMode)
	{
		std::recursive_mutex& exclusiveDBAccessMutex = DBInterface::getExclusiveDBAccessMutex();
		std::lock_guard<std::recursive_mutex> lock(exclusiveDBAccessMutex);

		if (g_confFilePath.length() == 0)
		{
			std::string confFilePath = dataPath() + "\\Conf\\DBJson.json";
			std::ifstream jsonFile(confFilePath);
			if (jsonFile.fail() || !jsonFile.is_open())
				throw(MapManagerExceptionDBException(__FUNCTION__, "Could not open json conf file for reading", confFilePath.c_str()));
			Json::Value root;
			jsonFile >> root;
			g_DBDir = dataPath() + "\\" + root["DBDir"].asString();
			g_confFilePath = g_DBDir + "\\" + root["DBConfFileName"].asString();
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
		std::ifstream jsonFile(g_confFilePath);
		jsonFile >> root;
		return root[paranName].asString();
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

	std::string DBJson::getQuadrantHash(float gridStepInWU, size_t numVerticesPerSize, size_t level, float lowerXGridVertex, float lowerZGridVertex, enum class DBInterface::QuadrantStatus& status, enum class QuadrantVertexStoreType& vertexStoreType)
	{
		if (dataPath().length() == 0)
			throw(MapManagerExceptionDBExceptionNotInitialized(__FUNCTION__));

		status = DBInterface::QuadrantStatus::Complete;
		vertexStoreType = DBInterface::QuadrantVertexStoreType::NotSet;

		std::string quadrantFilePath = getQuadrantFilePath(gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);
		if (!fs::exists(quadrantFilePath))
			return "";

		std::ifstream jsonQuadrantFile(quadrantFilePath);
		if (jsonQuadrantFile.fail() || !jsonQuadrantFile.is_open())
			throw(MapManagerExceptionDBException(__FUNCTION__, "Could not open json quadrant file for reading", quadrantFilePath.c_str()));
		Json::Value root;
		jsonQuadrantFile >> root;

		return root["data"]["hash_text"].asString() + base64_decode(root["data"]["hash_binary"].asString());
	}

	bool DBJson::writeQuadrantToDB(TheWorld_Utils::MeshCacheBuffer& cache, TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData& cacheQuadrantData, std::string strBuffer, enum class QuadrantVertexStoreType vertexStoreType, bool& stop)
	{
		if (dataPath().length() == 0)
			throw(MapManagerExceptionDBExceptionNotInitialized(__FUNCTION__));

		TheWorld_Utils::GuardProfiler profiler(std::string("writeQuadrantToDB ") + __FUNCTION__, "ALL");

		PLOG_DEBUG << "Align CACHE <==> DB - Writing to DB quadrant " << cache.getCacheIdStr();

		float gridStepInWU = cache.getGridStepInWU();
		size_t numVerticesPerSize = cache.getNumVerticesPerSize();
		float gridSizeInWU = (numVerticesPerSize - 1) * gridStepInWU;
		int level = cache.getLevel();
		float lowerXGridVertex = cache.getLowerXGridVertex();
		float lowerZGridVertex = cache.getLowerZGridVertex();
		float higherXGridVertex = lowerXGridVertex + (numVerticesPerSize - 1) * gridStepInWU;
		float higherZGridVertex = lowerZGridVertex + (numVerticesPerSize - 1) * gridStepInWU;

		Json::Value root;
		root["gridStepInWU"] = gridStepInWU;
		root["numVerticesPerSize"] = int(numVerticesPerSize);
		root["level"] = level;
		root["lowerXGridVertex"] = lowerXGridVertex;
		root["lowerZGridVertex"] = lowerZGridVertex;
		root["quadrantIndexX"] = int(floor(lowerXGridVertex / gridSizeInWU));
		root["quadrantIndexZ"] = int(floor(lowerZGridVertex / gridSizeInWU));
		root["higherXGridVertex"] = higherXGridVertex;
		root["higherZGridVertex"] = higherZGridVertex;

		std::string hash = cacheQuadrantData.meshId;
		std::string hashWithoutBinaryTime;
		std::chrono::system_clock::time_point t = cache.getTimeFromMeshId(hash, hashWithoutBinaryTime);
		if (t == std::chrono::system_clock::time_point())
			throw(MapManagerExceptionDBException(__FUNCTION__, "MeshId error"));

		std::string toEncode((char*)&t, sizeof(t));
		std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(toEncode.c_str()), toEncode.length());
		std::string decoded = base64_decode(encoded);
		if (toEncode != decoded)
			throw(MapManagerExceptionDBException(__FUNCTION__, "Decoding error"));

		Json::Value rootData;
		rootData["hash_text"] = hashWithoutBinaryTime;
		rootData["hash_binary"] = encoded;

		if (cacheQuadrantData.terrainEditValues->size() > 0)
		{
			TheWorld_Utils::TerrainEdit terrainEdit;
			terrainEdit.deserialize(*cacheQuadrantData.terrainEditValues);
			rootData["001_terrainType"] = int(terrainEdit.terrainType);
			rootData["001_terrainTypeStr"] = TheWorld_Utils::TerrainEdit::terrainTypeString(terrainEdit.terrainType);
			rootData["002_heightsNeedRegen"] = terrainEdit.extraValues1.heightsNeedRegen;
			rootData["003_normalsNeedRegen"] = terrainEdit.normalsNeedRegen;
			rootData["004_splatmapNeedRegen"] = terrainEdit.extraValues.splatmapNeedRegen;
			rootData["005_colormapNeedRegen"] = terrainEdit.extraValues.colormapNeedRegen;
			rootData["006_globalmapNeedRegen"] = terrainEdit.extraValues.globalmapNeedRegen;
			rootData["007_needUploadToServer"] = terrainEdit.needUploadToServer;
			rootData["008_minHeight"] = terrainEdit.minHeight;
			rootData["009_maxHeight"] = terrainEdit.maxHeight;
			rootData["010_lowElevationTexName_r"] = terrainEdit.extraValues.lowElevationTexName_r;
			rootData["011_highElevationTexName_g"] = terrainEdit.extraValues.highElevationTexName_g;
			rootData["012_dirtTexName_b"] = terrainEdit.extraValues.dirtTexName_b;
			rootData["013_rocksTexName_a"] = terrainEdit.extraValues.rocksTexName_a;
			rootData["014_splatmap_slopeVerticalFactor"] = terrainEdit.extraValues.slopeVerticalFactor;
			rootData["015_splatmap_slopeFlatFactor"] = terrainEdit.extraValues.slopeFlatFactor;
			rootData["016_splatmap_dirtOnRocksFactor"] = terrainEdit.extraValues.dirtOnRocksFactor;
			rootData["017_splatmap_highElevationFactor"] = terrainEdit.extraValues.highElevationFactor;
			rootData["018_splatmap_lowElevationFactor"] = terrainEdit.extraValues.lowElevationFactor;
			rootData["019_splatMapMode"] = int(terrainEdit.extraValues.splatMapMode);
			rootData["020_amplitudeBlendFactor"] = terrainEdit.extraValues.amplitudeBlendFactor;
			rootData["021_postGenerateElevation"] = terrainEdit.extraValues1.postGenerateElevation;
			rootData["022_ignoreDesideredMinHeight"] = terrainEdit.extraValues1.ignoreDesideredMinHeight;
			rootData["023_numBlendingCycles"] = int(terrainEdit.extraValues1.numBlendingCycles);
			rootData["024_blendingBendingFactor"] = terrainEdit.extraValues1.blendingBendingFactor;
			rootData["025_minRandomBendingExtension"] = terrainEdit.extraValues1.minRandomBendingExtension;
			rootData["026_maxRandomBendingExtension"] = terrainEdit.extraValues1.maxRandomBendingExtension;

			rootData["grp_eastSideXPlus"]["needBlend"] = terrainEdit.eastSideXPlus.needBlend;
			rootData["grp_eastSideXPlus"]["minHeight"] = terrainEdit.eastSideXPlus.minHeight;
			rootData["grp_eastSideXPlus"]["maxHeight"] = terrainEdit.eastSideXPlus.maxHeight;
			rootData["grp_westSideXMinus"]["needBlend"] = terrainEdit.westSideXMinus.needBlend;
			rootData["grp_westSideXMinus"]["minHeight"] = terrainEdit.westSideXMinus.minHeight;
			rootData["grp_westSideXMinus"]["maxHeight"] = terrainEdit.westSideXMinus.maxHeight;
			rootData["grp_southSideZPlus"]["needBlend"] = terrainEdit.southSideZPlus.needBlend;
			rootData["grp_southSideZPlus"]["minHeight"] = terrainEdit.southSideZPlus.minHeight;
			rootData["grp_southSideZPlus"]["maxHeight"] = terrainEdit.southSideZPlus.maxHeight;
			rootData["grp_northSideZMinus"]["needBlend"] = terrainEdit.northSideZMinus.needBlend;
			rootData["grp_northSideZMinus"]["minHeight"] = terrainEdit.northSideZMinus.minHeight;
			rootData["grp_northSideZMinus"]["maxHeight"] = terrainEdit.northSideZMinus.maxHeight;

			rootData["grp_noise"]["001_noiseType"] = terrainEdit.noise.noiseType;
			rootData["grp_noise"]["002_rotationType3D"] = terrainEdit.noise.rotationType3D;
			rootData["grp_noise"]["003_noiseSeed"] = terrainEdit.noise.noiseSeed;
			rootData["grp_noise"]["004_frequency"] = terrainEdit.noise.frequency;
			rootData["grp_noise"]["005_fractalType"] = terrainEdit.noise.fractalType;
			rootData["grp_noise"]["006_fractalOctaves"] = terrainEdit.noise.fractalOctaves;
			rootData["grp_noise"]["007_fractalLacunarity"] = terrainEdit.noise.fractalLacunarity;
			rootData["grp_noise"]["008_fractalGain"] = terrainEdit.noise.fractalGain;
			rootData["grp_noise"]["009_fractalWeightedStrength"] = terrainEdit.noise.fractalWeightedStrength;
			rootData["grp_noise"]["010_fractalPingPongStrength"] = terrainEdit.noise.fractalPingPongStrength;
			rootData["grp_noise"]["011_cellularDistanceFunction"] = terrainEdit.noise.cellularDistanceFunction;
			rootData["grp_noise"]["012_cellularReturnType"] = terrainEdit.noise.cellularReturnType;
			rootData["grp_noise"]["013_cellularJitter"] = terrainEdit.noise.cellularJitter;
			rootData["grp_noise"]["014_warpNoiseDomainWarpType"] = terrainEdit.noise.warpNoiseDomainWarpType;
			rootData["grp_noise"]["015_warpNoiseRotationType3D"] = terrainEdit.noise.warpNoiseRotationType3D;
			rootData["grp_noise"]["016_warpNoiseSeed"] = terrainEdit.noise.warpNoiseSeed;
			rootData["grp_noise"]["017_warpNoiseDomainWarpAmp"] = terrainEdit.noise.warpNoiseDomainWarpAmp;
			rootData["grp_noise"]["018_warpNoiseFrequency"] = terrainEdit.noise.warpNoiseFrequency;
			rootData["grp_noise"]["019_warpNoieseFractalType"] = terrainEdit.noise.warpNoieseFractalType;
			rootData["grp_noise"]["020_warpNoiseFractalOctaves"] = terrainEdit.noise.warpNoiseFractalOctaves;
			rootData["grp_noise"]["021_warpNoiseFractalLacunarity"] = terrainEdit.noise.warpNoiseFractalLacunarity;
			rootData["grp_noise"]["022_warpNoiseFractalGain"] = terrainEdit.noise.warpNoiseFractalGain;
			rootData["grp_noise"]["023_heightsAmplitude"] = terrainEdit.noise.heightsAmplitude;
			rootData["grp_noise"]["024_scaleFactor_JagIntensity"] = terrainEdit.noise.scaleFactor_JagIntensity;
			rootData["grp_noise"]["025_desideredMinHeight"] = terrainEdit.noise.desideredMinHeight;
			rootData["grp_noise"]["026_desideredMinHeigthMandatory"] = terrainEdit.noise.desideredMinHeigthMandatory;
		}

		root["data"] = rootData;

		std::string quadrantDirPath = getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level);
		if (!fs::exists(quadrantDirPath))
			fs::create_directories(quadrantDirPath);

		std::string quadrantFilePath = getQuadrantFilePath(gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);
		ofstream quadrantFile(quadrantFilePath, ios_base::out);
		if (quadrantFile.fail() || !quadrantFile.is_open())
			throw(MapManagerExceptionDBException(__FUNCTION__, "Could not open json quadrant file for writing", quadrantFilePath.c_str()));
		quadrantFile << root;
		quadrantFile.close();

		return true;
	}

	void DBJson::readQuadrantFromDB(TheWorld_Utils::MeshCacheBuffer& cache, std::string& meshId, std::string& strBuffer, enum class DBInterface::QuadrantStatus& status, enum class QuadrantVertexStoreType& vertexStoreType, TheWorld_Utils::TerrainEdit& terrainEdit)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("readQuadrantFromDB ") + __FUNCTION__, "ALL");

		PLOG_DEBUG << "Align CACHE <==> DB - Reading quadrant from DB " << cache.getCacheIdStr();

		status = DBInterface::QuadrantStatus::Complete;
		vertexStoreType = DBInterface::QuadrantVertexStoreType::NotSet;
		meshId.clear();
		strBuffer.clear();
		terrainEdit.size = 0;

		std::string quadrantFilePath;

		float gridStepInWU = cache.getGridStepInWU();
		size_t numVerticesPerSize = cache.getNumVerticesPerSize();
		int level = cache.getLevel();
		float lowerXGridVertex = cache.getLowerXGridVertex();
		float lowerZGridVertex = cache.getLowerZGridVertex();
		float higherXGridVertex = lowerXGridVertex + (numVerticesPerSize - 1) * gridStepInWU;
		float higherZGridVertex = lowerZGridVertex + (numVerticesPerSize - 1) * gridStepInWU;
		quadrantFilePath = getQuadrantFilePath(gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);

		if (!fs::exists(quadrantFilePath))
			return;

		std::ifstream jsonQuadrantFile(quadrantFilePath);
		if (jsonQuadrantFile.fail() || !jsonQuadrantFile.is_open())
			throw(MapManagerExceptionDBException(__FUNCTION__, "Could not open json quadrant file for reading", quadrantFilePath.c_str()));
		Json::Value root;
		jsonQuadrantFile >> root;
		Json::Value rootData = root["data"];

		// meshId
		meshId = rootData["hash_text"].asString() + base64_decode(rootData["hash_binary"].asString());

		// terrainEdit
		terrainEdit.size = sizeof(TheWorld_Utils::TerrainEdit);
		terrainEdit.needUploadToServer = rootData["007_needUploadToServer"].asBool();
		terrainEdit.normalsNeedRegen = rootData["003_normalsNeedRegen"].asBool();
		terrainEdit.terrainType = (enum class TheWorld_Utils::TerrainEdit::TerrainType)rootData["001_terrainType"].asInt();
		terrainEdit.minHeight = float(rootData["008_minHeight"].asDouble());
		terrainEdit.maxHeight = float(rootData["009_maxHeight"].asDouble());

		terrainEdit.eastSideXPlus.needBlend = rootData["grp_eastSideXPlus"]["needBlend"].asBool();
		terrainEdit.eastSideXPlus.minHeight = float(rootData["grp_eastSideXPlus"]["minHeight"].asDouble());
		terrainEdit.eastSideXPlus.maxHeight = float(rootData["grp_eastSideXPlus"]["maxHeight"].asDouble());
		terrainEdit.westSideXMinus.needBlend = rootData["grp_westSideXMinus"]["needBlend"].asBool();
		terrainEdit.westSideXMinus.minHeight = float(rootData["grp_westSideXMinus"]["minHeight"].asDouble());
		terrainEdit.westSideXMinus.maxHeight = float(rootData["grp_westSideXMinus"]["maxHeight"].asDouble());
		terrainEdit.southSideZPlus.needBlend = rootData["grp_southSideZPlus"]["needBlend"].asBool();
		terrainEdit.southSideZPlus.minHeight = float(rootData["grp_southSideZPlus"]["minHeight"].asDouble());
		terrainEdit.southSideZPlus.maxHeight = float(rootData["grp_southSideZPlus"]["maxHeight"].asDouble());
		terrainEdit.northSideZMinus.needBlend = rootData["grp_northSideZMinus"]["needBlend"].asBool();
		terrainEdit.northSideZMinus.minHeight = float(rootData["grp_northSideZMinus"]["minHeight"].asDouble());
		terrainEdit.northSideZMinus.maxHeight = float(rootData["grp_northSideZMinus"]["maxHeight"].asDouble());

		terrainEdit.noise.noiseType = rootData["grp_noise"]["001_noiseType"].asInt();
		terrainEdit.noise.rotationType3D = rootData["grp_noise"]["002_rotationType3D"].asInt();
		terrainEdit.noise.noiseSeed = rootData["grp_noise"]["003_noiseSeed"].asInt();
		terrainEdit.noise.frequency = float(rootData["grp_noise"]["004_frequency"].asDouble());
		terrainEdit.noise.fractalType = rootData["grp_noise"]["005_fractalType"].asInt();
		terrainEdit.noise.fractalOctaves = rootData["grp_noise"]["006_fractalOctaves"].asInt();
		terrainEdit.noise.fractalLacunarity = float(rootData["grp_noise"]["007_fractalLacunarity"].asDouble());
		terrainEdit.noise.fractalGain = float(rootData["grp_noise"]["008_fractalGain"].asDouble());
		terrainEdit.noise.fractalWeightedStrength = float(rootData["grp_noise"]["009_fractalWeightedStrength"].asDouble());
		terrainEdit.noise.fractalPingPongStrength = float(rootData["grp_noise"]["010_fractalPingPongStrength"].asDouble());
		terrainEdit.noise.cellularDistanceFunction = rootData["grp_noise"]["011_cellularDistanceFunction"].asInt();
		terrainEdit.noise.cellularReturnType = rootData["grp_noise"]["012_cellularReturnType"].asInt();
		terrainEdit.noise.cellularJitter = float(rootData["grp_noise"]["013_cellularJitter"].asDouble());
		terrainEdit.noise.warpNoiseDomainWarpType = rootData["grp_noise"]["014_warpNoiseDomainWarpType"].asInt();
		terrainEdit.noise.warpNoiseRotationType3D = rootData["grp_noise"]["015_warpNoiseRotationType3D"].asInt();
		terrainEdit.noise.warpNoiseSeed = rootData["grp_noise"]["016_warpNoiseSeed"].asInt();
		terrainEdit.noise.warpNoiseDomainWarpAmp = float(rootData["grp_noise"]["017_warpNoiseDomainWarpAmp"].asDouble());
		terrainEdit.noise.warpNoiseFrequency = float(rootData["grp_noise"]["018_warpNoiseFrequency"].asDouble());
		terrainEdit.noise.warpNoieseFractalType = rootData["grp_noise"]["019_warpNoieseFractalType"].asInt();
		terrainEdit.noise.warpNoiseFractalOctaves = rootData["grp_noise"]["020_warpNoiseFractalOctaves"].asInt();
		terrainEdit.noise.warpNoiseFractalLacunarity = float(rootData["grp_noise"]["021_warpNoiseFractalLacunarity"].asDouble());
		terrainEdit.noise.warpNoiseFractalGain = float(rootData["grp_noise"]["022_warpNoiseFractalGain"].asDouble());
		terrainEdit.noise.heightsAmplitude = rootData["grp_noise"]["023_heightsAmplitude"].asInt();
		terrainEdit.noise.scaleFactor_JagIntensity = float(rootData["grp_noise"]["024_scaleFactor_JagIntensity"].asDouble());
		terrainEdit.noise.desideredMinHeight = float(rootData["grp_noise"]["025_desideredMinHeight"].asDouble());
		terrainEdit.noise.desideredMinHeigthMandatory = rootData["grp_noise"]["026_desideredMinHeigthMandatory"].asBool();

		strcpy_s(terrainEdit.extraValues.lowElevationTexName_r, sizeof(terrainEdit.extraValues.lowElevationTexName_r), rootData["010_lowElevationTexName_r"].asString().c_str());
		strcpy_s(terrainEdit.extraValues.highElevationTexName_g, sizeof(terrainEdit.extraValues.highElevationTexName_g), rootData["011_highElevationTexName_g"].asString().c_str());
		strcpy_s(terrainEdit.extraValues.dirtTexName_b, sizeof(terrainEdit.extraValues.dirtTexName_b), rootData["012_dirtTexName_b"].asString().c_str());
		strcpy_s(terrainEdit.extraValues.rocksTexName_a, sizeof(terrainEdit.extraValues.rocksTexName_a), rootData["013_rocksTexName_a"].asString().c_str());
		terrainEdit.extraValues.splatmapNeedRegen = rootData["004_splatmapNeedRegen"].asBool();
		terrainEdit.extraValues.colormapNeedRegen = rootData["005_colormapNeedRegen"].asBool();
		terrainEdit.extraValues.globalmapNeedRegen = rootData["006_globalmapNeedRegen"].asBool();
		terrainEdit.extraValues.slopeVerticalFactor = float(rootData["014_splatmap_slopeVerticalFactor"].asDouble());
		terrainEdit.extraValues.slopeFlatFactor = float(rootData["015_splatmap_slopeFlatFactor"].asDouble());
		terrainEdit.extraValues.dirtOnRocksFactor = float(rootData["016_splatmap_dirtOnRocksFactor"].asDouble());
		terrainEdit.extraValues.highElevationFactor = float(rootData["017_splatmap_highElevationFactor"].asDouble());
		terrainEdit.extraValues.lowElevationFactor = float(rootData["018_splatmap_lowElevationFactor"].asDouble());
		terrainEdit.extraValues.splatMapMode = size_t(rootData["019_splatMapMode"].asInt());
		terrainEdit.extraValues.amplitudeBlendFactor = float(rootData["020_amplitudeBlendFactor"].asDouble());
		terrainEdit.extraValues1.postGenerateElevation = float(rootData["021_postGenerateElevation"].asDouble());
		terrainEdit.extraValues1.ignoreDesideredMinHeight = rootData["022_ignoreDesideredMinHeight"].asBool();
		terrainEdit.extraValues1.numBlendingCycles = size_t(rootData["023_numBlendingCycles"].asInt());
		terrainEdit.extraValues1.blendingBendingFactor = float(rootData["024_blendingBendingFactor"].asDouble());
		terrainEdit.extraValues1.minRandomBendingExtension = float(rootData["025_minRandomBendingExtension"].asDouble());
		terrainEdit.extraValues1.maxRandomBendingExtension = float(rootData["026_maxRandomBendingExtension"].asDouble());
		terrainEdit.extraValues1.heightsNeedRegen = rootData["002_heightsNeedRegen"].asBool();

		// override
		terrainEdit.extraValues1.heightsNeedRegen = true;
		terrainEdit.normalsNeedRegen = true;
		terrainEdit.extraValues.splatmapNeedRegen = true;
		terrainEdit.extraValues.globalmapNeedRegen = true;
		terrainEdit.extraValues.colormapNeedRegen = true;

		// strBuffer
		TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData cacheQuadrantData;
		cacheQuadrantData.meshId = meshId;
		TheWorld_Utils::MemoryBuffer terrainEditValuesBuffer((BYTE*)&terrainEdit, terrainEdit.size);
		cacheQuadrantData.terrainEditValues = &terrainEditValuesBuffer;
		//cacheQuadrantData.terrainEdit = &terrainEdit;
		cacheQuadrantData.minHeight = terrainEdit.minHeight;
		cacheQuadrantData.maxHeight = terrainEdit.maxHeight;
		TheWorld_Utils::MemoryBuffer emptyFloat16HeightsBuffer;
		TheWorld_Utils::MemoryBuffer emptyFloat32HeightsBuffer;
		TheWorld_Utils::MemoryBuffer emptyNormalBuffer;
		TheWorld_Utils::MemoryBuffer emptySplatmapBuffer;
		TheWorld_Utils::MemoryBuffer emptyColormapBuffer;
		TheWorld_Utils::MemoryBuffer emptyGlobalmapBuffer;
		cacheQuadrantData.heights16Buffer = &emptyFloat16HeightsBuffer;
		cacheQuadrantData.heights32Buffer = &emptyFloat32HeightsBuffer;
		cacheQuadrantData.normalsBuffer = &emptyNormalBuffer;
		cacheQuadrantData.splatmapBuffer = &emptySplatmapBuffer;
		cacheQuadrantData.colormapBuffer = &emptyColormapBuffer;
		cacheQuadrantData.globalmapBuffer = &emptyGlobalmapBuffer;
		cache.setBufferFromCacheQuadrantData(numVerticesPerSize, cacheQuadrantData, strBuffer);

		return;
	}

	void DBJson::finalizeDB(void)
	{
	}

	void DBJson::importExternalQuadrants(float gridStepInWU, size_t numVerticesPerSize, int level)
	{
		std::string quadrantImportDirPath = getQuadrantImportDirPath(gridStepInWU, numVerticesPerSize, level);
		if (!fs::exists(quadrantImportDirPath))
			fs::create_directories(quadrantImportDirPath);

		std::string quadrantDirPath = getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level);
		if (!fs::exists(quadrantDirPath))
			fs::create_directories(quadrantDirPath);

		std::string quadrantWorkDirPath = getQuadrantWorkDirPath(gridStepInWU, numVerticesPerSize, level);
		if (!fs::exists(quadrantWorkDirPath))
			fs::create_directories(quadrantWorkDirPath);

		for (const auto& entry : fs::directory_iterator(quadrantImportDirPath))
			if (entry.is_regular_file() && entry.path().extension() == ".json")
			{
				std::string path = entry.path().string();
				std::ifstream jsonQuadrantFile(path);
				if (jsonQuadrantFile.fail() || !jsonQuadrantFile.is_open())
				{
					PLOG_ERROR << "*** " << " cannnot open " << path;
				}
				else
				{
					bool parseError = false;
					Json::Value root;
					try
					{
						jsonQuadrantFile >> root;
					}
					catch (std::exception)
					{
						PLOG_ERROR << "*** " << " error parsing " << path;
						parseError = true;
					}
					
					jsonQuadrantFile.close();

					if (!parseError)
					{
						bool importFile = completeQuadrantDataWithDefaultValues(gridStepInWU, numVerticesPerSize, level, root);

						if (importFile)
						{
							float lowerXGridVertex = float(root["lowerXGridVertex"].asDouble());
							float lowerZGridVertex = float(root["lowerZGridVertex"].asDouble());

							std::filesystem::path _quadrantFilePath = getQuadrantFilePath(gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);
							std::string quadrantFilePath = quadrantWorkDirPath + "\\" + _quadrantFilePath.filename().string();

							ofstream quadrantFile(quadrantFilePath, ios_base::out);
							if (quadrantFile.fail() || !quadrantFile.is_open())
							{
								PLOG_ERROR << "Could not open json quadrant file for writing " << quadrantFilePath;
							}
							else
							{
								quadrantFile << root;
								quadrantFile.close();

								bool ret = false;
								try
								{
									ret = fs::remove(path);
								}
								catch (std::exception ex)
								{
									ret = false;
									PLOG_ERROR << "Exception deleting import quadrant file " << path << " - " << ex.what();
								}
								if (!ret)
								{
									PLOG_ERROR << "Could not open delete import quadrant file " << path;
								}
							}
						}
						else
						{
							PLOG_ERROR << "*** " << " cannnot import " << path;
						}
					}
				}
			}
	}

	bool DBJson::completeQuadrantDataWithDefaultValues(float gridStepInWU, size_t numVerticesPerSize, int level, Json::Value& root)
	{
		float gridSizeInWU = (numVerticesPerSize - 1) * gridStepInWU;
		float lowerXGridVertex = 0.0f, lowerZGridVertex = 0.0f ;
		
		if (root["lowerXGridVertex"].isNull())
		{
			if (root["quadrantIndexX"].isNull())
			{
				PLOG_ERROR << " missing both lowerXGridVertex and quadrantIndexX";
				return false;
			}
			else
			{
				lowerXGridVertex = float(root["quadrantIndexX"].asInt() * gridSizeInWU);
				root["lowerXGridVertex"] = lowerXGridVertex;
			}
		}
		else
		{
			lowerXGridVertex = float(root["lowerXGridVertex"].asDouble());
			root["quadrantIndexX"] = int(floor(lowerXGridVertex / gridSizeInWU));
		}

		if (root["lowerZGridVertex"].isNull())
		{
			if (root["quadrantIndexZ"].isNull())
			{
				PLOG_ERROR << " missing both lowerZGridVertex and quadrantIndexZ";
				return false;
			}
			else
			{
				lowerZGridVertex = float(root["quadrantIndexZ"].asInt() * gridSizeInWU);
				root["lowerZGridVertex"] = lowerZGridVertex;
			}
		}
		else
		{
			lowerZGridVertex = float(root["lowerZGridVertex"].asDouble());
			root["quadrantIndexZ"] = int(floor(lowerZGridVertex / gridSizeInWU));
		}

		root["gridStepInWU"] = gridStepInWU;
		root["numVerticesPerSize"] = int(numVerticesPerSize);
		root["level"] = level;
		root["higherXGridVertex"] = lowerXGridVertex + (numVerticesPerSize - 1) * gridStepInWU;
		root["higherZGridVertex"] = lowerZGridVertex + (numVerticesPerSize - 1) * gridStepInWU;

		std::string newHash = TheWorld_Utils::MeshCacheBuffer::generateNewMeshId();
		std::string hashWithoutBinaryTime;
		std::chrono::system_clock::time_point t = TheWorld_Utils::MeshCacheBuffer::getTimeFromMeshId(newHash, hashWithoutBinaryTime);
		if (t == std::chrono::system_clock::time_point())
			throw(MapManagerExceptionDBException(__FUNCTION__, "MeshId error"));

		std::string toEncode((char*)&t, sizeof(t));
		std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(toEncode.c_str()), toEncode.length());
		std::string decoded = base64_decode(encoded);
		if (toEncode != decoded)
			throw(MapManagerExceptionDBException(__FUNCTION__, "Decoding error"));

		root["data"]["hash_text"] = hashWithoutBinaryTime;
		root["data"]["hash_binary"] = encoded;

		TheWorld_Utils::TerrainEdit::TerrainType type = TheWorld_Utils::TerrainEdit::TerrainType::empty;
		Json::Value v = root["data"]["001_terrainType"];
		if (v.isNull())
			root["data"]["001_terrainType"] = int(type);
		else
		{
			if (v.isString())
			{
				std::string s = v.asString();
				type = TheWorld_Utils::TerrainEdit::terrainTypeEnum(s);
				if (type == TheWorld_Utils::TerrainEdit::TerrainType::notset)
				{
					PLOG_ERROR << __FUNCTION__ << " unknown terrain type <" << s << ">";
					return false;
				}
				root["data"]["001_terrainType"] = int(type);
			}
			else if (v.isInt())
			{
				type = (TheWorld_Utils::TerrainEdit::TerrainType)v.asInt();
			}
			else
			{
				PLOG_ERROR << __FUNCTION__ << " terrain type must be string or int";
				return false;
			}
		}
		
		root["data"]["001_terrainTypeStr"] = TheWorld_Utils::TerrainEdit::terrainTypeString(type);

		TheWorld_Utils::TerrainEdit terrainEdit(type);

		v = root["data"]["002_heightsNeedRegen"];
		if (v.isNull())
			root["data"]["002_heightsNeedRegen"] = terrainEdit.extraValues1.heightsNeedRegen;
		
		v = root["data"]["003_normalsNeedRegen"];
		if (v.isNull())
			root["data"]["003_normalsNeedRegen"] = terrainEdit.normalsNeedRegen;

		v = root["data"]["004_splatmapNeedRegen"];
		if (v.isNull())
			root["data"]["004_splatmapNeedRegen"] = terrainEdit.extraValues.splatmapNeedRegen;

		v = root["data"]["005_colormapNeedRegen"];
		if (v.isNull())
			root["data"]["005_colormapNeedRegen"] = terrainEdit.extraValues.colormapNeedRegen;

		v = root["data"]["006_globalmapNeedRegen"];
		if (v.isNull())
			root["data"]["006_globalmapNeedRegen"] = terrainEdit.extraValues.globalmapNeedRegen;

		v = root["data"]["007_needUploadToServer"];
		if (v.isNull())
			root["data"]["007_needUploadToServer"] = terrainEdit.needUploadToServer;

		v = root["data"]["008_minHeight"];
		if (v.isNull())
			root["data"]["008_minHeight"] = terrainEdit.minHeight;

		v = root["data"]["009_maxHeight"];
		if (v.isNull())
			root["data"]["009_maxHeight"] = terrainEdit.maxHeight;

		v = root["data"]["010_lowElevationTexName_r"];
		if (v.isNull())
			root["data"]["010_lowElevationTexName_r"] = terrainEdit.extraValues.lowElevationTexName_r;

		v = root["data"]["011_highElevationTexName_g"];
		if (v.isNull())
			root["data"]["011_highElevationTexName_g"] = terrainEdit.extraValues.highElevationTexName_g;

		v = root["data"]["012_dirtTexName_b"];
		if (v.isNull())
			root["data"]["012_dirtTexName_b"] = terrainEdit.extraValues.dirtTexName_b;

		v = root["data"]["013_rocksTexName_a"];
		if (v.isNull())
			root["data"]["013_rocksTexName_a"] = terrainEdit.extraValues.rocksTexName_a;

		v = root["data"]["014_splatmap_slopeVerticalFactor"];
		if (v.isNull())
			root["data"]["014_splatmap_slopeVerticalFactor"] = terrainEdit.extraValues.slopeVerticalFactor;

		v = root["data"]["015_splatmap_slopeFlatFactor"];
		if (v.isNull())
			root["data"]["015_splatmap_slopeFlatFactor"] = terrainEdit.extraValues.slopeFlatFactor;

		v = root["data"]["016_splatmap_dirtOnRocksFactor"];
		if (v.isNull())
			root["data"]["016_splatmap_dirtOnRocksFactor"] = terrainEdit.extraValues.dirtOnRocksFactor;

		v = root["data"]["017_splatmap_highElevationFactor"];
		if (v.isNull())
			root["data"]["017_splatmap_highElevationFactor"] = terrainEdit.extraValues.highElevationFactor;

		v = root["data"]["018_splatmap_lowElevationFactor"];
		if (v.isNull())
			root["data"]["018_splatmap_lowElevationFactor"] = terrainEdit.extraValues.lowElevationFactor;

		v = root["data"]["019_splatMapMode"];
		if (v.isNull())
			root["data"]["019_splatMapMode"] = int(terrainEdit.extraValues.splatMapMode);

		v = root["data"]["020_amplitudeBlendFactor"];
		if (v.isNull())
			root["data"]["020_amplitudeBlendFactor"] = terrainEdit.extraValues.amplitudeBlendFactor;

		v = root["data"]["021_postGenerateElevation"];
		if (v.isNull())
			root["data"]["021_postGenerateElevation"] = terrainEdit.extraValues1.postGenerateElevation;

		v = root["data"]["022_ignoreDesideredMinHeight"];
		if (v.isNull())
			root["data"]["022_ignoreDesideredMinHeight"] = terrainEdit.extraValues1.ignoreDesideredMinHeight;

		v = root["data"]["023_numBlendingCycles"];
		if (v.isNull())
			root["data"]["023_numBlendingCycles"] = int(terrainEdit.extraValues1.numBlendingCycles);

		v = root["data"]["024_blendingBendingFactor"];
		if (v.isNull())
			root["data"]["024_blendingBendingFactor"] = terrainEdit.extraValues1.blendingBendingFactor;

		v = root["data"]["025_minRandomBendingExtension"];
		if (v.isNull())
			root["data"]["025_minRandomBendingExtension"] = terrainEdit.extraValues1.minRandomBendingExtension;

		v = root["data"]["026_maxRandomBendingExtension"];
		if (v.isNull())
			root["data"]["026_maxRandomBendingExtension"] = terrainEdit.extraValues1.maxRandomBendingExtension;

		v = root["data"]["grp_eastSideXPlus"]["needBlend"];
		if (v.isNull())
			root["data"]["grp_eastSideXPlus"]["needBlend"] = terrainEdit.eastSideXPlus.needBlend;

		v = root["data"]["grp_eastSideXPlus"]["minHeight"];
		if (v.isNull())
			root["data"]["grp_eastSideXPlus"]["minHeight"] = terrainEdit.eastSideXPlus.minHeight;

		v = root["data"]["grp_eastSideXPlus"]["maxHeight"];
		if (v.isNull())
			root["data"]["grp_eastSideXPlus"]["maxHeight"] = terrainEdit.eastSideXPlus.maxHeight;

		v = root["data"]["grp_westSideXMinus"]["needBlend"];
		if (v.isNull())
			root["data"]["grp_westSideXMinus"]["needBlend"] = terrainEdit.westSideXMinus.needBlend;

		v = root["data"]["grp_westSideXMinus"]["minHeight"];
		if (v.isNull())
			root["data"]["grp_westSideXMinus"]["minHeight"] = terrainEdit.westSideXMinus.minHeight;

		v = root["data"]["grp_westSideXMinus"]["maxHeight"];
		if (v.isNull())
			root["data"]["grp_westSideXMinus"]["maxHeight"] = terrainEdit.westSideXMinus.maxHeight;

		v = root["data"]["grp_southSideZPlus"]["needBlend"];
		if (v.isNull())
			root["data"]["grp_southSideZPlus"]["needBlend"] = terrainEdit.southSideZPlus.needBlend;

		v = root["data"]["grp_southSideZPlus"]["minHeight"];
		if (v.isNull())
			root["data"]["grp_southSideZPlus"]["minHeight"] = terrainEdit.southSideZPlus.minHeight;

		v = root["data"]["grp_southSideZPlus"]["maxHeight"];
		if (v.isNull())
			root["data"]["grp_southSideZPlus"]["maxHeight"] = terrainEdit.southSideZPlus.maxHeight;

		v = root["data"]["grp_northSideZMinus"]["needBlend"];
		if (v.isNull())
			root["data"]["grp_northSideZMinus"]["needBlend"] = terrainEdit.northSideZMinus.needBlend;

		v = root["data"]["grp_northSideZMinus"]["minHeight"];
		if (v.isNull())
			root["data"]["grp_northSideZMinus"]["minHeight"] = terrainEdit.northSideZMinus.minHeight;

		v = root["data"]["grp_northSideZMinus"]["maxHeight"];
		if (v.isNull())
			root["data"]["grp_northSideZMinus"]["maxHeight"] = terrainEdit.northSideZMinus.maxHeight;


		v = root["data"]["grp_noise"]["001_noiseType"];
		if (v.isNull())
			root["data"]["grp_noise"]["001_noiseType"] = terrainEdit.noise.noiseType;

		v = root["data"]["grp_noise"]["002_rotationType3D"];
		if (v.isNull())
			root["data"]["grp_noise"]["002_rotationType3D"] = terrainEdit.noise.rotationType3D;

		v = root["data"]["grp_noise"]["003_noiseSeed"];
		if (v.isNull())
			root["data"]["grp_noise"]["003_noiseSeed"] = terrainEdit.noise.noiseSeed;

		v = root["data"]["grp_noise"]["004_frequency"];
		if (v.isNull())
			root["data"]["grp_noise"]["004_frequency"] = terrainEdit.noise.frequency;

		v = root["data"]["grp_noise"]["005_fractalType"];
		if (v.isNull())
			root["data"]["grp_noise"]["005_fractalType"] = terrainEdit.noise.fractalType;

		v = root["data"]["grp_noise"]["006_fractalOctaves"];
		if (v.isNull())
			root["data"]["grp_noise"]["006_fractalOctaves"] = terrainEdit.noise.fractalOctaves;

		v = root["data"]["grp_noise"]["007_fractalLacunarity"];
		if (v.isNull())
			root["data"]["grp_noise"]["007_fractalLacunarity"] = terrainEdit.noise.fractalLacunarity;

		v = root["data"]["grp_noise"]["008_fractalGain"];
		if (v.isNull())
			root["data"]["grp_noise"]["008_fractalGain"] = terrainEdit.noise.fractalGain;

		v = root["data"]["grp_noise"]["009_fractalWeightedStrength"];
		if (v.isNull())
			root["data"]["grp_noise"]["009_fractalWeightedStrength"] = terrainEdit.noise.fractalWeightedStrength;

		v = root["data"]["grp_noise"]["010_fractalPingPongStrength"];
		if (v.isNull())
			root["data"]["grp_noise"]["010_fractalPingPongStrength"] = terrainEdit.noise.fractalPingPongStrength;

		v = root["data"]["grp_noise"]["011_cellularDistanceFunction"];
		if (v.isNull())
			root["data"]["grp_noise"]["011_cellularDistanceFunction"] = terrainEdit.noise.cellularDistanceFunction;

		v = root["data"]["grp_noise"]["012_cellularReturnType"];
		if (v.isNull())
			root["data"]["grp_noise"]["012_cellularReturnType"] = terrainEdit.noise.cellularReturnType;

		v = root["data"]["grp_noise"]["013_cellularJitter"];
		if (v.isNull())
			root["data"]["grp_noise"]["013_cellularJitter"] = terrainEdit.noise.cellularJitter;

		v = root["data"]["grp_noise"]["014_warpNoiseDomainWarpType"];
		if (v.isNull())
			root["data"]["grp_noise"]["014_warpNoiseDomainWarpType"] = terrainEdit.noise.warpNoiseDomainWarpType;

		v = root["data"]["grp_noise"]["015_warpNoiseRotationType3D"];
		if (v.isNull())
			root["data"]["grp_noise"]["015_warpNoiseRotationType3D"] = terrainEdit.noise.warpNoiseRotationType3D;

		v = root["data"]["grp_noise"]["016_warpNoiseSeed"];
		if (v.isNull())
			root["data"]["grp_noise"]["016_warpNoiseSeed"] = terrainEdit.noise.warpNoiseSeed;

		v = root["data"]["grp_noise"]["017_warpNoiseDomainWarpAmp"];
		if (v.isNull())
			root["data"]["grp_noise"]["017_warpNoiseDomainWarpAmp"] = terrainEdit.noise.warpNoiseDomainWarpAmp;

		v = root["data"]["grp_noise"]["018_warpNoiseFrequency"];
		if (v.isNull())
			root["data"]["grp_noise"]["018_warpNoiseFrequency"] = terrainEdit.noise.warpNoiseFrequency;

		v = root["data"]["grp_noise"]["019_warpNoieseFractalType"];
		if (v.isNull())
			root["data"]["grp_noise"]["019_warpNoieseFractalType"] = terrainEdit.noise.warpNoieseFractalType;

		v = root["data"]["grp_noise"]["020_warpNoiseFractalOctaves"];
		if (v.isNull())
			root["data"]["grp_noise"]["020_warpNoiseFractalOctaves"] = terrainEdit.noise.warpNoiseFractalOctaves;

		v = root["data"]["grp_noise"]["021_warpNoiseFractalLacunarity"];
		if (v.isNull())
			root["data"]["grp_noise"]["021_warpNoiseFractalLacunarity"] = terrainEdit.noise.warpNoiseFractalLacunarity;

		v = root["data"]["grp_noise"]["022_warpNoiseFractalGain"];
		if (v.isNull())
			root["data"]["grp_noise"]["022_warpNoiseFractalGain"] = terrainEdit.noise.warpNoiseFractalGain;

		v = root["data"]["grp_noise"]["023_heightsAmplitude"];
		if (v.isNull())
			root["data"]["grp_noise"]["023_heightsAmplitude"] = terrainEdit.noise.heightsAmplitude;

		v = root["data"]["grp_noise"]["024_scaleFactor_JagIntensity"];
		if (v.isNull())
			root["data"]["grp_noise"]["024_scaleFactor_JagIntensity"] = terrainEdit.noise.scaleFactor_JagIntensity;

		v = root["data"]["grp_noise"]["025_desideredMinHeight"];
		if (v.isNull())
			root["data"]["grp_noise"]["025_desideredMinHeight"] = terrainEdit.noise.desideredMinHeight;

		v = root["data"]["grp_noise"]["026_desideredMinHeigthMandatory"];
		if (v.isNull())
			root["data"]["grp_noise"]["026_desideredMinHeigthMandatory"] = terrainEdit.noise.desideredMinHeigthMandatory;

		return true;
	}

	std::string DBJson::getQuadrantDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level)
	{
		return g_DBDir + "\\maps\\" + readParam(DBInterface::c_mapNameParamName) + "\\" + "ST-" + std::to_string(gridStepInWU) + "_SZ-" + std::to_string(numVerticesPerSize) + "\\L-" + std::to_string(level);
	}

	std::string DBJson::getQuadrantImportDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level)
	{
		return getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level) + "\\Import";
	}

	std::string DBJson::getQuadrantWorkDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level)
	{
		return getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level) + "\\Work";
	}

	std::string DBJson::getQuadrantFilePath(float gridStepInWU, size_t numVerticesPerSize, size_t level, float lowerXGridVertex, float lowerZGridVertex)
	{
		return getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level) + "\\X-" + std::to_string(lowerXGridVertex) + "_Z-" + std::to_string(lowerZGridVertex) + ".json";
	}
}