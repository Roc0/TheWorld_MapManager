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

	std::chrono::system_clock::time_point DBJson::getTextureRectTime(TheWorld_Utils::MapTextureRectPos& textureRectPos, size_t numVerticesPerSize, float gridStepInWU)
	{
		// return value: -1 = value not set, 0 = all quadrants contained are empty, else time_t of the most recent quadrant
		
		std::chrono::system_clock::time_point textureRectTime;

		if (dataPath().length() == 0)
			throw(MapManagerExceptionDBExceptionNotInitialized(__FUNCTION__));

		std::string textureRectDirPath = getTextureRectDirPath(gridStepInWU, numVerticesPerSize, textureRectPos.getLevel());
		if (!fs::exists(textureRectDirPath))
			fs::create_directories(textureRectDirPath);

		std::string textureRectPath = textureRectDirPath + "\\" + textureRectPos.getName() + ".json";;
		if (!fs::exists(textureRectPath))
			return textureRectTime;

		std::ifstream jsonTextureRectFile(textureRectPath);
		if (jsonTextureRectFile.fail() || !jsonTextureRectFile.is_open())
			throw(MapManagerExceptionDBException(__FUNCTION__, "Could not open json texture rect file for reading", textureRectPath.c_str()));
		Json::Value root;
		jsonTextureRectFile >> root;

		std::string s = root["Time"].asString();
		
		if (s == "nul")
			return textureRectTime;

		//std::tm t = {};
		//std::istringstream ss(s);
		//ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
		//if (not ss.fail())
		//	textureRectTime = std::mktime(&t);
		textureRectTime = TheWorld_Utils::str_time_to_time_point(s);

		return textureRectTime;
	}

	void DBJson::setTextureRectTime(std::chrono::system_clock::time_point textureRectTime, TheWorld_Utils::MapTextureRectPos& textureRectPos, size_t numVerticesPerSize, float gridStepInWU)
	{
		// textureRectTime: -1 = value not set, 0 = all quadrants contained are empty, else time_t of the most recent quadrant

		if (dataPath().length() == 0)
			throw(MapManagerExceptionDBExceptionNotInitialized(__FUNCTION__));

		std::string textureRectDirPath = getTextureRectDirPath(gridStepInWU, numVerticesPerSize, textureRectPos.getLevel());
		if (!fs::exists(textureRectDirPath))
			fs::create_directories(textureRectDirPath);

		std::string textureRectPath = textureRectDirPath + "\\" + textureRectPos.getName() + ".json";;

		std::string timeStr;
		if (TheWorld_Utils::time_point_zero(textureRectTime))
		{
			timeStr = "nul";
		}
		else
		{
			//std::tm tm;
			//localtime_s(&tm, &textureRectTime);
			//std::strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", &tm);
			//timeStr = buffer;
			timeStr = TheWorld_Utils::time_point_to_str_time(textureRectTime);
		}

		Json::Value root;
		root["Time"] = timeStr;

		std::ofstream jsonTextureRectFile(textureRectPath, ios_base::out);
		if (jsonTextureRectFile.fail() || !jsonTextureRectFile.is_open())
			throw(MapManagerExceptionDBException(__FUNCTION__, "Could not open json texture rect file for writing", textureRectPath.c_str()));
		jsonTextureRectFile << root;
		jsonTextureRectFile.close();
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

		return root["hash_text"].asString() + base64_decode(root["hash_binary"].asString());
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

		root["hash_text"] = hashWithoutBinaryTime;
		root["hash_binary"] = encoded;

		Json::Value rootTerrainData;
		if (cacheQuadrantData.terrainEditValues->size() > 0)
		{
			TheWorld_Utils::TerrainEdit terrainEdit;
			terrainEdit.deserialize(*cacheQuadrantData.terrainEditValues);
			rootTerrainData["001_terrainType"] = int(terrainEdit.terrainType);
			rootTerrainData["001_terrainTypeStr"] = TheWorld_Utils::TerrainEdit::terrainTypeString(terrainEdit.terrainType);
			rootTerrainData["002_heightsNeedRegen"] = terrainEdit.heightsNeedRegen;
			rootTerrainData["003_normalsNeedRegen"] = terrainEdit.normalsNeedRegen;
			rootTerrainData["004_splatmapNeedRegen"] = terrainEdit.splatmapNeedRegen();
			rootTerrainData["005_colormapNeedRegen"] = terrainEdit.colormapNeedRegen;
			rootTerrainData["006_globalmapNeedRegen"] = terrainEdit.globalmapNeedRegen;
			rootTerrainData["007_needUploadToServer"] = terrainEdit.needUploadToServer;
			rootTerrainData["008_minHeight"] = terrainEdit.minHeight;
			rootTerrainData["009_maxHeight"] = terrainEdit.maxHeight;
			rootTerrainData["010_lowElevationTexName_r"] = terrainEdit.extraValues.lowElevationTexName_r;
			rootTerrainData["011_highElevationTexName_g"] = terrainEdit.extraValues.highElevationTexName_g;
			rootTerrainData["012_dirtTexName_b"] = terrainEdit.extraValues.dirtTexName_b;
			rootTerrainData["013_rocksTexName_a"] = terrainEdit.extraValues.rocksTexName_a;
			rootTerrainData["014_splatmap_slopeVerticalFactor"] = terrainEdit.extraValues.slopeVerticalFactor;
			rootTerrainData["015_splatmap_slopeFlatFactor"] = terrainEdit.extraValues.slopeFlatFactor;
			rootTerrainData["016_splatmap_dirtOnRocksFactor"] = terrainEdit.extraValues.dirtOnRocksFactor;
			rootTerrainData["017_splatmap_highElevationFactor"] = terrainEdit.extraValues.highElevationFactor;
			rootTerrainData["018_splatmap_lowElevationFactor"] = terrainEdit.extraValues.lowElevationFactor;
			rootTerrainData["019_splatMapMode"] = int(terrainEdit.extraValues.splatMapMode);
			rootTerrainData["020_amplitudeBlendFactor"] = terrainEdit.extraValues.amplitudeBlendFactor;
			rootTerrainData["021_postGenerateElevation"] = terrainEdit.extraValues.postGenerateElevation;
			rootTerrainData["022_ignoreDesideredMinHeight"] = terrainEdit.extraValues.ignoreDesideredMinHeight;
			rootTerrainData["023_numBlendingCycles"] = int(terrainEdit.extraValues.numBlendingCycles);
			rootTerrainData["024_blendingBendingFactor"] = terrainEdit.extraValues.blendingBendingFactor;
			rootTerrainData["025_minRandomBendingExtension"] = terrainEdit.extraValues.minRandomBendingExtension;
			rootTerrainData["026_maxRandomBendingExtension"] = terrainEdit.extraValues.maxRandomBendingExtension;
			rootTerrainData["027_seaInfoSet"] = terrainEdit.extraValues.seaInfoSet;
			rootTerrainData["028_containsSea"] = terrainEdit.extraValues.canContainSea;
			rootTerrainData["029_continentId"] = (int)terrainEdit.extraValues1.continentId;
			
			rootTerrainData["grp_northGroundTextureLayers"]["layer1"] = (int)terrainEdit.extraValues1.northGroundTextureLayers.layer1;
			rootTerrainData["grp_northGroundTextureLayers"]["layer2"] = (int)terrainEdit.extraValues1.northGroundTextureLayers.layer2;
			rootTerrainData["grp_northGroundTextureLayers"]["layer3"] = (int)terrainEdit.extraValues1.northGroundTextureLayers.layer3;
			rootTerrainData["grp_northGroundTextureLayers"]["layer4"] = (int)terrainEdit.extraValues1.northGroundTextureLayers.layer4;
			rootTerrainData["grp_westGroundTextureLayers"]["layer1"] = (int)terrainEdit.extraValues1.westGroundTextureLayers.layer1;
			rootTerrainData["grp_westGroundTextureLayers"]["layer2"] = (int)terrainEdit.extraValues1.westGroundTextureLayers.layer2;
			rootTerrainData["grp_westGroundTextureLayers"]["layer3"] = (int)terrainEdit.extraValues1.westGroundTextureLayers.layer3;
			rootTerrainData["grp_westGroundTextureLayers"]["layer4"] = (int)terrainEdit.extraValues1.westGroundTextureLayers.layer4;
			rootTerrainData["grp_northwestGroundTextureLayers"]["layer1"] = (int)terrainEdit.extraValues1.northwestGroundTextureLayers.layer1;
			rootTerrainData["grp_northwestGroundTextureLayers"]["layer2"] = (int)terrainEdit.extraValues1.northwestGroundTextureLayers.layer2;
			rootTerrainData["grp_northwestGroundTextureLayers"]["layer3"] = (int)terrainEdit.extraValues1.northwestGroundTextureLayers.layer3;
			rootTerrainData["grp_northwestGroundTextureLayers"]["layer4"] = (int)terrainEdit.extraValues1.northwestGroundTextureLayers.layer4;

			rootTerrainData["grp_eastSideXPlus"]["needBlend"] = terrainEdit.eastSideXPlus.needBlend;
			rootTerrainData["grp_eastSideXPlus"]["minHeight"] = terrainEdit.eastSideXPlus.minHeight;
			rootTerrainData["grp_eastSideXPlus"]["maxHeight"] = terrainEdit.eastSideXPlus.maxHeight;
			rootTerrainData["grp_westSideXMinus"]["needBlend"] = terrainEdit.westSideXMinus.needBlend;
			rootTerrainData["grp_westSideXMinus"]["minHeight"] = terrainEdit.westSideXMinus.minHeight;
			rootTerrainData["grp_westSideXMinus"]["maxHeight"] = terrainEdit.westSideXMinus.maxHeight;
			rootTerrainData["grp_southSideZPlus"]["needBlend"] = terrainEdit.southSideZPlus.needBlend;
			rootTerrainData["grp_southSideZPlus"]["minHeight"] = terrainEdit.southSideZPlus.minHeight;
			rootTerrainData["grp_southSideZPlus"]["maxHeight"] = terrainEdit.southSideZPlus.maxHeight;
			rootTerrainData["grp_northSideZMinus"]["needBlend"] = terrainEdit.northSideZMinus.needBlend;
			rootTerrainData["grp_northSideZMinus"]["minHeight"] = terrainEdit.northSideZMinus.minHeight;
			rootTerrainData["grp_northSideZMinus"]["maxHeight"] = terrainEdit.northSideZMinus.maxHeight;

			rootTerrainData["grp_eastNeighbourXPlus"]["minHeight"] = terrainEdit.deprecated_eastNeighbourXPlus.minHeight();
			rootTerrainData["grp_eastNeighbourXPlus"]["maxHeight"] = terrainEdit.deprecated_eastNeighbourXPlus.maxHeight();
			rootTerrainData["grp_westNeighbourXMinus"]["minHeight"] = terrainEdit.deprecated_westNeighbourXMinus.minHeight();
			rootTerrainData["grp_westNeighbourXMinus"]["maxHeight"] = terrainEdit.deprecated_westNeighbourXMinus.maxHeight();
			rootTerrainData["grp_southNeighbourZPlus"]["minHeight"] = terrainEdit.deprecated_southNeighbourZPlus.minHeight();
			rootTerrainData["grp_southNeighbourZPlus"]["maxHeight"] = terrainEdit.deprecated_southNeighbourZPlus.maxHeight();
			rootTerrainData["grp_northNeighbourZMinus"]["minHeight"] = terrainEdit.deprecated_northNeighbourZMinus.minHeight();
			rootTerrainData["grp_northNeighbourZMinus"]["maxHeight"] = terrainEdit.deprecated_northNeighbourZMinus.maxHeight();

			rootTerrainData["grp_noise"]["001_noiseType"] = terrainEdit.noise.noiseType;
			rootTerrainData["grp_noise"]["002_rotationType3D"] = terrainEdit.noise.rotationType3D;
			rootTerrainData["grp_noise"]["003_noiseSeed"] = terrainEdit.noise.noiseSeed;
			rootTerrainData["grp_noise"]["004_frequency"] = terrainEdit.noise.frequency;
			rootTerrainData["grp_noise"]["005_fractalType"] = terrainEdit.noise.fractalType;
			rootTerrainData["grp_noise"]["006_fractalOctaves"] = terrainEdit.noise.fractalOctaves;
			rootTerrainData["grp_noise"]["007_fractalLacunarity"] = terrainEdit.noise.fractalLacunarity;
			rootTerrainData["grp_noise"]["008_fractalGain"] = terrainEdit.noise.fractalGain;
			rootTerrainData["grp_noise"]["009_fractalWeightedStrength"] = terrainEdit.noise.fractalWeightedStrength;
			rootTerrainData["grp_noise"]["010_fractalPingPongStrength"] = terrainEdit.noise.fractalPingPongStrength;
			rootTerrainData["grp_noise"]["011_cellularDistanceFunction"] = terrainEdit.noise.cellularDistanceFunction;
			rootTerrainData["grp_noise"]["012_cellularReturnType"] = terrainEdit.noise.cellularReturnType;
			rootTerrainData["grp_noise"]["013_cellularJitter"] = terrainEdit.noise.cellularJitter;
			rootTerrainData["grp_noise"]["014_warpNoiseDomainWarpType"] = terrainEdit.noise.warpNoiseDomainWarpType;
			rootTerrainData["grp_noise"]["015_warpNoiseRotationType3D"] = terrainEdit.noise.warpNoiseRotationType3D;
			rootTerrainData["grp_noise"]["016_warpNoiseSeed"] = terrainEdit.noise.warpNoiseSeed;
			rootTerrainData["grp_noise"]["017_warpNoiseDomainWarpAmp"] = terrainEdit.noise.warpNoiseDomainWarpAmp;
			rootTerrainData["grp_noise"]["018_warpNoiseFrequency"] = terrainEdit.noise.warpNoiseFrequency;
			rootTerrainData["grp_noise"]["019_warpNoieseFractalType"] = terrainEdit.noise.warpNoieseFractalType;
			rootTerrainData["grp_noise"]["020_warpNoiseFractalOctaves"] = terrainEdit.noise.warpNoiseFractalOctaves;
			rootTerrainData["grp_noise"]["021_warpNoiseFractalLacunarity"] = terrainEdit.noise.warpNoiseFractalLacunarity;
			rootTerrainData["grp_noise"]["022_warpNoiseFractalGain"] = terrainEdit.noise.warpNoiseFractalGain;
			rootTerrainData["grp_noise"]["023_heightsAmplitude"] = terrainEdit.noise.heightsAmplitude;
			rootTerrainData["grp_noise"]["024_scaleFactor_JagIntensity"] = terrainEdit.noise.scaleFactor_JagIntensity;
			rootTerrainData["grp_noise"]["025_desideredMinHeight"] = terrainEdit.noise.desideredMinHeight;
			rootTerrainData["grp_noise"]["026_desideredMinHeigthMandatory"] = terrainEdit.noise.desideredMinHeigthMandatory;
		}

		root["terrain_data"] = rootTerrainData;

		std::string quadrantDirPath = getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level);
		if (!fs::exists(quadrantDirPath))
			fs::create_directories(quadrantDirPath);

		std::string quadrantFilePath = getQuadrantFilePath(gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);
		std::ofstream quadrantFile(quadrantFilePath, ios_base::out);
		if (quadrantFile.fail() || !quadrantFile.is_open())
			throw(MapManagerExceptionDBException(__FUNCTION__, "Could not open json quadrant file for writing", quadrantFilePath.c_str()));
		quadrantFile << root;
		quadrantFile.close();

		return true;
	}

	void DBJson::readQuadrantFromDB(TheWorld_Utils::MeshCacheBuffer& cache, std::string& meshId, std::string& strBuffer, enum class DBInterface::QuadrantStatus& status, enum class QuadrantVertexStoreType& vertexStoreType, TheWorld_Utils::TerrainEdit& terrainEdit)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("readQuadrantFromDB ") + __FUNCTION__, "ALL");

		PLOG_DEBUG << "Reading quadrant from DB " << cache.getCacheIdStr();

		status = DBInterface::QuadrantStatus::Complete;
		vertexStoreType = DBInterface::QuadrantVertexStoreType::NotSet;
		meshId.clear();
		strBuffer.clear();
		terrainEdit.size = 0;

		float gridStepInWU = cache.getGridStepInWU();
		size_t numVerticesPerSize = cache.getNumVerticesPerSize();
		int level = cache.getLevel();
		float lowerXGridVertex = cache.getLowerXGridVertex();
		float lowerZGridVertex = cache.getLowerZGridVertex();
		float higherXGridVertex = lowerXGridVertex + (numVerticesPerSize - 1) * gridStepInWU;
		float higherZGridVertex = lowerZGridVertex + (numVerticesPerSize - 1) * gridStepInWU;
		std::string quadrantFilePath = getQuadrantFilePath(gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);

		if (!fs::exists(quadrantFilePath))
			return;

		std::ifstream jsonQuadrantFile(quadrantFilePath);
		if (jsonQuadrantFile.fail() || !jsonQuadrantFile.is_open())
			throw(MapManagerExceptionDBException(__FUNCTION__, "Could not open json quadrant file for reading", quadrantFilePath.c_str()));
		Json::Value root;
		jsonQuadrantFile >> root;
		Json::Value rootTerrainData = root["terrain_data"];

		// meshId
		meshId = root["hash_text"].asString() + base64_decode(root["hash_binary"].asString());

		// terrainEdit
		terrainEdit.size = sizeof(TheWorld_Utils::TerrainEdit);
		terrainEdit.needUploadToServer = rootTerrainData["007_needUploadToServer"].asBool();
		terrainEdit.normalsNeedRegen = rootTerrainData["003_normalsNeedRegen"].asBool();
		terrainEdit.terrainType = (enum class TheWorld_Utils::TerrainEdit::TerrainType)rootTerrainData["001_terrainType"].asInt();
		terrainEdit.minHeight = float(rootTerrainData["008_minHeight"].asDouble());
		terrainEdit.maxHeight = float(rootTerrainData["009_maxHeight"].asDouble());

		terrainEdit.eastSideXPlus.needBlend = rootTerrainData["grp_eastSideXPlus"]["needBlend"].asBool();
		terrainEdit.eastSideXPlus.minHeight = float(rootTerrainData["grp_eastSideXPlus"]["minHeight"].asDouble());
		terrainEdit.eastSideXPlus.maxHeight = float(rootTerrainData["grp_eastSideXPlus"]["maxHeight"].asDouble());
		terrainEdit.westSideXMinus.needBlend = rootTerrainData["grp_westSideXMinus"]["needBlend"].asBool();
		terrainEdit.westSideXMinus.minHeight = float(rootTerrainData["grp_westSideXMinus"]["minHeight"].asDouble());
		terrainEdit.westSideXMinus.maxHeight = float(rootTerrainData["grp_westSideXMinus"]["maxHeight"].asDouble());
		terrainEdit.southSideZPlus.needBlend = rootTerrainData["grp_southSideZPlus"]["needBlend"].asBool();
		terrainEdit.southSideZPlus.minHeight = float(rootTerrainData["grp_southSideZPlus"]["minHeight"].asDouble());
		terrainEdit.southSideZPlus.maxHeight = float(rootTerrainData["grp_southSideZPlus"]["maxHeight"].asDouble());
		terrainEdit.northSideZMinus.needBlend = rootTerrainData["grp_northSideZMinus"]["needBlend"].asBool();
		terrainEdit.northSideZMinus.minHeight = float(rootTerrainData["grp_northSideZMinus"]["minHeight"].asDouble());
		terrainEdit.northSideZMinus.maxHeight = float(rootTerrainData["grp_northSideZMinus"]["maxHeight"].asDouble());

		terrainEdit.deprecated_eastNeighbourXPlus.minHeight(float(rootTerrainData["grp_eastNeighbourXPlus"]["minHeight"].asDouble()));
		terrainEdit.deprecated_eastNeighbourXPlus.maxHeight(float(rootTerrainData["grp_eastNeighbourXPlus"]["maxHeight"].asDouble()));
		terrainEdit.deprecated_westNeighbourXMinus.minHeight(float(rootTerrainData["grp_westNeighbourXMinus"]["minHeight"].asDouble()));
		terrainEdit.deprecated_westNeighbourXMinus.maxHeight(float(rootTerrainData["grp_westNeighbourXMinus"]["maxHeight"].asDouble()));
		terrainEdit.deprecated_southNeighbourZPlus.minHeight(float(rootTerrainData["grp_southNeighbourZPlus"]["minHeight"].asDouble()));
		terrainEdit.deprecated_southNeighbourZPlus.maxHeight(float(rootTerrainData["grp_southNeighbourZPlus"]["maxHeight"].asDouble()));
		terrainEdit.deprecated_northNeighbourZMinus.minHeight(float(rootTerrainData["grp_northNeighbourZMinus"]["minHeight"].asDouble()));
		terrainEdit.deprecated_northNeighbourZMinus.maxHeight(float(rootTerrainData["grp_northNeighbourZMinus"]["maxHeight"].asDouble()));

		terrainEdit.noise.noiseType = rootTerrainData["grp_noise"]["001_noiseType"].asInt();
		terrainEdit.noise.rotationType3D = rootTerrainData["grp_noise"]["002_rotationType3D"].asInt();
		terrainEdit.noise.noiseSeed = rootTerrainData["grp_noise"]["003_noiseSeed"].asInt();
		terrainEdit.noise.frequency = float(rootTerrainData["grp_noise"]["004_frequency"].asDouble());
		terrainEdit.noise.fractalType = rootTerrainData["grp_noise"]["005_fractalType"].asInt();
		terrainEdit.noise.fractalOctaves = rootTerrainData["grp_noise"]["006_fractalOctaves"].asInt();
		terrainEdit.noise.fractalLacunarity = float(rootTerrainData["grp_noise"]["007_fractalLacunarity"].asDouble());
		terrainEdit.noise.fractalGain = float(rootTerrainData["grp_noise"]["008_fractalGain"].asDouble());
		terrainEdit.noise.fractalWeightedStrength = float(rootTerrainData["grp_noise"]["009_fractalWeightedStrength"].asDouble());
		terrainEdit.noise.fractalPingPongStrength = float(rootTerrainData["grp_noise"]["010_fractalPingPongStrength"].asDouble());
		terrainEdit.noise.cellularDistanceFunction = rootTerrainData["grp_noise"]["011_cellularDistanceFunction"].asInt();
		terrainEdit.noise.cellularReturnType = rootTerrainData["grp_noise"]["012_cellularReturnType"].asInt();
		terrainEdit.noise.cellularJitter = float(rootTerrainData["grp_noise"]["013_cellularJitter"].asDouble());
		terrainEdit.noise.warpNoiseDomainWarpType = rootTerrainData["grp_noise"]["014_warpNoiseDomainWarpType"].asInt();
		terrainEdit.noise.warpNoiseRotationType3D = rootTerrainData["grp_noise"]["015_warpNoiseRotationType3D"].asInt();
		terrainEdit.noise.warpNoiseSeed = rootTerrainData["grp_noise"]["016_warpNoiseSeed"].asInt();
		terrainEdit.noise.warpNoiseDomainWarpAmp = float(rootTerrainData["grp_noise"]["017_warpNoiseDomainWarpAmp"].asDouble());
		terrainEdit.noise.warpNoiseFrequency = float(rootTerrainData["grp_noise"]["018_warpNoiseFrequency"].asDouble());
		terrainEdit.noise.warpNoieseFractalType = rootTerrainData["grp_noise"]["019_warpNoieseFractalType"].asInt();
		terrainEdit.noise.warpNoiseFractalOctaves = rootTerrainData["grp_noise"]["020_warpNoiseFractalOctaves"].asInt();
		terrainEdit.noise.warpNoiseFractalLacunarity = float(rootTerrainData["grp_noise"]["021_warpNoiseFractalLacunarity"].asDouble());
		terrainEdit.noise.warpNoiseFractalGain = float(rootTerrainData["grp_noise"]["022_warpNoiseFractalGain"].asDouble());
		terrainEdit.noise.heightsAmplitude = rootTerrainData["grp_noise"]["023_heightsAmplitude"].asInt();
		terrainEdit.noise.scaleFactor_JagIntensity = float(rootTerrainData["grp_noise"]["024_scaleFactor_JagIntensity"].asDouble());
		terrainEdit.noise.desideredMinHeight = float(rootTerrainData["grp_noise"]["025_desideredMinHeight"].asDouble());
		terrainEdit.noise.desideredMinHeigthMandatory = rootTerrainData["grp_noise"]["026_desideredMinHeigthMandatory"].asBool();

		strcpy_s(terrainEdit.extraValues.lowElevationTexName_r, sizeof(terrainEdit.extraValues.lowElevationTexName_r), rootTerrainData["010_lowElevationTexName_r"].asString().c_str());
		strcpy_s(terrainEdit.extraValues.highElevationTexName_g, sizeof(terrainEdit.extraValues.highElevationTexName_g), rootTerrainData["011_highElevationTexName_g"].asString().c_str());
		strcpy_s(terrainEdit.extraValues.dirtTexName_b, sizeof(terrainEdit.extraValues.dirtTexName_b), rootTerrainData["012_dirtTexName_b"].asString().c_str());
		strcpy_s(terrainEdit.extraValues.rocksTexName_a, sizeof(terrainEdit.extraValues.rocksTexName_a), rootTerrainData["013_rocksTexName_a"].asString().c_str());
		terrainEdit.splatmapNeedRegen(rootTerrainData["004_splatmapNeedRegen"].asBool());
		terrainEdit.colormapNeedRegen = rootTerrainData["005_colormapNeedRegen"].asBool();
		terrainEdit.globalmapNeedRegen = rootTerrainData["006_globalmapNeedRegen"].asBool();
		terrainEdit.extraValues.slopeVerticalFactor = float(rootTerrainData["014_splatmap_slopeVerticalFactor"].asDouble());
		terrainEdit.extraValues.slopeFlatFactor = float(rootTerrainData["015_splatmap_slopeFlatFactor"].asDouble());
		terrainEdit.extraValues.dirtOnRocksFactor = float(rootTerrainData["016_splatmap_dirtOnRocksFactor"].asDouble());
		terrainEdit.extraValues.highElevationFactor = float(rootTerrainData["017_splatmap_highElevationFactor"].asDouble());
		terrainEdit.extraValues.lowElevationFactor = float(rootTerrainData["018_splatmap_lowElevationFactor"].asDouble());
		terrainEdit.extraValues.splatMapMode = size_t(rootTerrainData["019_splatMapMode"].asInt());
		terrainEdit.extraValues.amplitudeBlendFactor = float(rootTerrainData["020_amplitudeBlendFactor"].asDouble());
		terrainEdit.extraValues.postGenerateElevation = float(rootTerrainData["021_postGenerateElevation"].asDouble());
		terrainEdit.extraValues.ignoreDesideredMinHeight = rootTerrainData["022_ignoreDesideredMinHeight"].asBool();
		terrainEdit.extraValues.numBlendingCycles = size_t(rootTerrainData["023_numBlendingCycles"].asInt());
		terrainEdit.extraValues.blendingBendingFactor = float(rootTerrainData["024_blendingBendingFactor"].asDouble());
		terrainEdit.extraValues.minRandomBendingExtension = float(rootTerrainData["025_minRandomBendingExtension"].asDouble());
		terrainEdit.extraValues.maxRandomBendingExtension = float(rootTerrainData["026_maxRandomBendingExtension"].asDouble());
		terrainEdit.heightsNeedRegen = rootTerrainData["002_heightsNeedRegen"].asBool();
		terrainEdit.extraValues.seaInfoSet = rootTerrainData["027_seaInfoSet"].asInt();
		terrainEdit.extraValues.canContainSea = rootTerrainData["028_containsSea"].asInt();
		terrainEdit.extraValues1.continentId = rootTerrainData["029_continentId"].asInt();

		terrainEdit.extraValues1.northGroundTextureLayers.layer1 = rootTerrainData["grp_northGroundTextureLayers"]["layer1"].asInt();
		terrainEdit.extraValues1.northGroundTextureLayers.layer2 = rootTerrainData["grp_northGroundTextureLayers"]["layer2"].asInt();
		terrainEdit.extraValues1.northGroundTextureLayers.layer3 = rootTerrainData["grp_northGroundTextureLayers"]["layer3"].asInt();
		terrainEdit.extraValues1.northGroundTextureLayers.layer4 = rootTerrainData["grp_northGroundTextureLayers"]["layer4"].asInt();
		terrainEdit.extraValues1.westGroundTextureLayers.layer1 = rootTerrainData["grp_westGroundTextureLayers"]["layer1"].asInt();
		terrainEdit.extraValues1.westGroundTextureLayers.layer2 = rootTerrainData["grp_westGroundTextureLayers"]["layer2"].asInt();
		terrainEdit.extraValues1.westGroundTextureLayers.layer3 = rootTerrainData["grp_westGroundTextureLayers"]["layer3"].asInt();
		terrainEdit.extraValues1.westGroundTextureLayers.layer4 = rootTerrainData["grp_westGroundTextureLayers"]["layer4"].asInt();
		terrainEdit.extraValues1.northwestGroundTextureLayers.layer1 = rootTerrainData["grp_northwestGroundTextureLayers"]["layer1"].asInt();
		terrainEdit.extraValues1.northwestGroundTextureLayers.layer2 = rootTerrainData["grp_northwestGroundTextureLayers"]["layer2"].asInt();
		terrainEdit.extraValues1.northwestGroundTextureLayers.layer3 = rootTerrainData["grp_northwestGroundTextureLayers"]["layer3"].asInt();
		terrainEdit.extraValues1.northwestGroundTextureLayers.layer4 = rootTerrainData["grp_northwestGroundTextureLayers"]["layer4"].asInt();

		// override: forcing terrain [re-]generation because terrain data are not available on the DB
		terrainEdit.heightsNeedRegen = true;
		terrainEdit.normalsNeedRegen = true;
		terrainEdit.splatmapNeedRegen(true);
		terrainEdit.globalmapNeedRegen = true;
		terrainEdit.colormapNeedRegen = true;
		terrainEdit.extraValues.seaInfoSet = QUADRANT_SEA_INFO_FALSE;

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

	void DBJson::writeExternalQuadrants(float gridStepInWU, size_t numVerticesPerSize, int level, std::map<TheWorld_Utils::QuadrantPos, TheWorld_Utils::TerrainEdit>& externalQuadrants)
	{
		std::string quadrantImportDirPath = getQuadrantImportDirPath(gridStepInWU, numVerticesPerSize, level);
		std::string fileName = "Editor_" + TheWorld_Utils::time_point_to_str_time(TheWorld_Utils::current_utc_time_point());
		std::replace(fileName.begin(), fileName.end(), ' ', '_');
		std::replace(fileName.begin(), fileName.end(), '.', '_');
		std::replace(fileName.begin(), fileName.end(), ':', '-');
		std::string quadrantImportFilePath = quadrantImportDirPath + "\\" + fileName + ".json";

		Json::Value quadrants;
		//int index = 0;
		for (auto& item : externalQuadrants)
		{
			Json::Value quadrant;
			terrainDataToQuadrant(quadrant, item.first, item.second);
			bool ok = completeQuadrantDataWithDefaultValues(gridStepInWU, numVerticesPerSize, level, quadrant);
			if (not ok)
			{
				std::string s = "Could not set terrain data for quadrant " + item.first.getIdStr();
				PLOG_ERROR << s;
				throw(MapManagerExceptionDBException(__FUNCTION__, s.c_str()));
			}
			//quadrants[index++] = quadrant;
			quadrants.append(quadrant);
		}

		Json::Value root;
		root["quadrants"] = quadrants;
		std::ofstream quadrantImportFile(quadrantImportFilePath, ios_base::out);
		if (quadrantImportFile.fail() || !quadrantImportFile.is_open())
		{
			char errMsg[512];
			int err = errno;
			strerror_s(errMsg, sizeof(errMsg), err);
			std::string s = "Could not open json import quadrants file for writing " + quadrantImportFilePath + " - " + errMsg + "(" + std::to_string(err) + ")";
			PLOG_ERROR << s;
			throw(MapManagerExceptionDBException(__FUNCTION__, s.c_str()));
		}
		else
		{

			quadrantImportFile << root;
			quadrantImportFile.close();
		}

	}

	void DBJson::importExternalQuadrants(float gridStepInWU, size_t numVerticesPerSize, int level)
	{
		std::string quadrantImportDirPath = getQuadrantImportDirPath(gridStepInWU, numVerticesPerSize, level);
		if (!fs::exists(quadrantImportDirPath))
			fs::create_directories(quadrantImportDirPath);

		std::string quadrantDirPath = getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level);
		if (!fs::exists(quadrantDirPath))
			fs::create_directories(quadrantDirPath);

		//std::string quadrantWorkDirPath = getQuadrantWorkDirPath(gridStepInWU, numVerticesPerSize, level);
		//if (!fs::exists(quadrantWorkDirPath))
		//	fs::create_directories(quadrantWorkDirPath);

		for (const auto& entry : fs::directory_iterator(quadrantImportDirPath))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".json")
			{
				std::string importingFilePath = entry.path().string();
				std::ifstream jsonQuadrantFile(importingFilePath);
				if (jsonQuadrantFile.fail() || !jsonQuadrantFile.is_open())
				{
					PLOG_ERROR << "*** " << " cannnot open " << importingFilePath;
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
						PLOG_ERROR << "*** " << " error parsing " << importingFilePath;
						parseError = true;
					}

					jsonQuadrantFile.close();

					if (!parseError)
					{
						const Json::Value quadrants = root["quadrants"];
						for (Json::Value::UInt index = 0; index < quadrants.size(); ++index)
						{
							Json::Value quadrant = quadrants[index];
							bool importFile = completeQuadrantDataWithDefaultValues(gridStepInWU, numVerticesPerSize, level, quadrant);

							if (importFile)
							{
								float lowerXGridVertex = float(quadrant["lowerXGridVertex"].asDouble());
								float lowerZGridVertex = float(quadrant["lowerZGridVertex"].asDouble());

								std::filesystem::path _quadrantFilePath = getQuadrantFilePath(gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);
								//std::string quadrantFilePath = quadrantWorkDirPath + "\\" + _quadrantFilePath.filename().string();
								std::string quadrantFilePath = _quadrantFilePath.string();

								std::ofstream quadrantFile(quadrantFilePath, ios_base::out);
								if (quadrantFile.fail() || !quadrantFile.is_open())
								{
									PLOG_ERROR << "Could not open json quadrant file for writing " << quadrantFilePath;
								}
								else
								{
									quadrantFile << quadrant;
									quadrantFile.close();
								}
							}
							else
							{
								PLOG_ERROR << "*** " << " cannnot import " << importingFilePath;
							}
						}

						bool ret = false;

						try
						{
							std::string importedFilePath = importingFilePath + ".imported";
							ret = fs::remove(importedFilePath);
							fs::rename(importingFilePath.c_str(), importedFilePath.c_str());
							ret = true;
						}
						catch (std::exception ex)
						{
							ret = false;
							std::string s = "Exception deleting import quadrant file " + importingFilePath + " - " + ex.what();
							PLOG_ERROR << "Exception deleting import quadrant file " << s;
							throw(MapManagerExceptionDBException(__FUNCTION__, s.c_str()));
						}
						if (!ret)
						{
							std::string s = "Could not delete import quadrant file " + importingFilePath;
							PLOG_ERROR << s;
							throw(MapManagerExceptionDBException(__FUNCTION__, s.c_str()));
						}
					}
				}
			}
		}
	}

	void DBJson::terrainDataToQuadrant(Json::Value& quadrant, const TheWorld_Utils::QuadrantPos& pos, const TheWorld_Utils::TerrainEdit& terrainEdit)
	{
		quadrant["gridStepInWU"] = pos.getGridStepInWU();
		quadrant["numVerticesPerSize"] = int(pos.getNumVerticesPerSize());
		quadrant["level"] = pos.getLevel();
		quadrant["lowerXGridVertex"] = pos.getLowerXGridVertex();
		quadrant["lowerZGridVertex"] = pos.getLowerZGridVertex();
		quadrant["quadrantIndexX"] = pos.getIndexPosX();
		quadrant["quadrantIndexZ"] = pos.getIndexPosZ();
		quadrant["terrain_data"]["001_terrainType"] = int(terrainEdit.terrainType);
		quadrant["terrain_data"]["002_heightsNeedRegen"] = terrainEdit.heightsNeedRegen;
		quadrant["terrain_data"]["003_normalsNeedRegen"] = terrainEdit.normalsNeedRegen;
		quadrant["terrain_data"]["004_splatmapNeedRegen"] = terrainEdit.splatmapNeedRegen();
		quadrant["terrain_data"]["005_colormapNeedRegen"] = terrainEdit.colormapNeedRegen;
		quadrant["terrain_data"]["006_globalmapNeedRegen"] = terrainEdit.globalmapNeedRegen;
		quadrant["terrain_data"]["007_needUploadToServer"] = terrainEdit.needUploadToServer;
		quadrant["terrain_data"]["008_minHeight"] = terrainEdit.minHeight;
		quadrant["terrain_data"]["009_maxHeight"] = terrainEdit.maxHeight;
		quadrant["terrain_data"]["010_lowElevationTexName_r"] = terrainEdit.extraValues.lowElevationTexName_r;
		quadrant["terrain_data"]["011_highElevationTexName_g"] = terrainEdit.extraValues.highElevationTexName_g;
		quadrant["terrain_data"]["012_dirtTexName_b"] = terrainEdit.extraValues.dirtTexName_b;
		quadrant["terrain_data"]["013_rocksTexName_a"] = terrainEdit.extraValues.rocksTexName_a;
		quadrant["terrain_data"]["014_splatmap_slopeVerticalFactor"] = terrainEdit.extraValues.slopeVerticalFactor;
		quadrant["terrain_data"]["015_splatmap_slopeFlatFactor"] = terrainEdit.extraValues.slopeFlatFactor;
		quadrant["terrain_data"]["016_splatmap_dirtOnRocksFactor"] = terrainEdit.extraValues.dirtOnRocksFactor;
		quadrant["terrain_data"]["017_splatmap_highElevationFactor"] = terrainEdit.extraValues.highElevationFactor;
		quadrant["terrain_data"]["018_splatmap_lowElevationFactor"] = terrainEdit.extraValues.lowElevationFactor;
		quadrant["terrain_data"]["019_splatMapMode"] = int(terrainEdit.extraValues.splatMapMode);
		quadrant["terrain_data"]["020_amplitudeBlendFactor"] = terrainEdit.extraValues.amplitudeBlendFactor;
		quadrant["terrain_data"]["021_postGenerateElevation"] = terrainEdit.extraValues.postGenerateElevation;
		quadrant["terrain_data"]["022_ignoreDesideredMinHeight"] = terrainEdit.extraValues.ignoreDesideredMinHeight;
		quadrant["terrain_data"]["023_numBlendingCycles"] = int(terrainEdit.extraValues.numBlendingCycles);
		quadrant["terrain_data"]["024_blendingBendingFactor"] = terrainEdit.extraValues.blendingBendingFactor;
		quadrant["terrain_data"]["025_minRandomBendingExtension"] = terrainEdit.extraValues.minRandomBendingExtension;
		quadrant["terrain_data"]["026_maxRandomBendingExtension"] = terrainEdit.extraValues.maxRandomBendingExtension;
		quadrant["terrain_data"]["027_seaInfoSet"] = terrainEdit.extraValues.seaInfoSet;
		quadrant["terrain_data"]["028_containsSea"] = terrainEdit.extraValues.canContainSea;
		quadrant["terrain_data"]["029_continentId"] = terrainEdit.extraValues1.continentId;
		quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer1"] = int(terrainEdit.extraValues1.northGroundTextureLayers.layer1);
		quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer2"] = int(terrainEdit.extraValues1.northGroundTextureLayers.layer2);
		quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer3"] = int(terrainEdit.extraValues1.northGroundTextureLayers.layer3);
		quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer4"] = int(terrainEdit.extraValues1.northGroundTextureLayers.layer4);
		quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer1"] = int(terrainEdit.extraValues1.westGroundTextureLayers.layer1);
		quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer2"] = int(terrainEdit.extraValues1.westGroundTextureLayers.layer2);
		quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer3"] = int(terrainEdit.extraValues1.westGroundTextureLayers.layer3);
		quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer4"] = int(terrainEdit.extraValues1.westGroundTextureLayers.layer4);
		quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer1"] = int(terrainEdit.extraValues1.northwestGroundTextureLayers.layer1);
		quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer2"] = int(terrainEdit.extraValues1.northwestGroundTextureLayers.layer2);
		quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer3"] = int(terrainEdit.extraValues1.northwestGroundTextureLayers.layer3);
		quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer4"] = int(terrainEdit.extraValues1.northwestGroundTextureLayers.layer4);
		quadrant["terrain_data"]["grp_eastSideXPlus"]["needBlend"] = terrainEdit.eastSideXPlus.needBlend;
		quadrant["terrain_data"]["grp_eastSideXPlus"]["minHeight"] = terrainEdit.eastSideXPlus.minHeight;
		quadrant["terrain_data"]["grp_eastSideXPlus"]["maxHeight"] = terrainEdit.eastSideXPlus.maxHeight;
		quadrant["terrain_data"]["grp_westSideXMinus"]["needBlend"] = terrainEdit.westSideXMinus.needBlend;
		quadrant["terrain_data"]["grp_westSideXMinus"]["minHeight"] = terrainEdit.westSideXMinus.minHeight;
		quadrant["terrain_data"]["grp_westSideXMinus"]["maxHeight"] = terrainEdit.westSideXMinus.maxHeight;
		quadrant["terrain_data"]["grp_southSideZPlus"]["needBlend"] = terrainEdit.southSideZPlus.needBlend;
		quadrant["terrain_data"]["grp_southSideZPlus"]["minHeight"] = terrainEdit.southSideZPlus.minHeight;
		quadrant["terrain_data"]["grp_southSideZPlus"]["maxHeight"] = terrainEdit.southSideZPlus.maxHeight;
		quadrant["terrain_data"]["grp_northSideZMinus"]["needBlend"] = terrainEdit.northSideZMinus.needBlend;
		quadrant["terrain_data"]["grp_northSideZMinus"]["minHeight"] = terrainEdit.northSideZMinus.minHeight;
		quadrant["terrain_data"]["grp_northSideZMinus"]["maxHeight"] = terrainEdit.northSideZMinus.maxHeight;
		quadrant["terrain_data"]["grp_eastNeighbourXPlus"]["minHeight"] = terrainEdit.deprecated_eastNeighbourXPlus.minHeight();
		quadrant["terrain_data"]["grp_eastNeighbourXPlus"]["maxHeight"] = terrainEdit.deprecated_eastNeighbourXPlus.maxHeight();
		quadrant["terrain_data"]["grp_westNeighbourXMinus"]["minHeight"] = terrainEdit.deprecated_westNeighbourXMinus.minHeight();
		quadrant["terrain_data"]["grp_westNeighbourXMinus"]["maxHeight"] = terrainEdit.deprecated_westNeighbourXMinus.maxHeight();
		quadrant["terrain_data"]["grp_southNeighbourZPlus"]["minHeight"] = terrainEdit.deprecated_southNeighbourZPlus.minHeight();
		quadrant["terrain_data"]["grp_southNeighbourZPlus"]["maxHeight"] = terrainEdit.deprecated_southNeighbourZPlus.maxHeight();
		quadrant["terrain_data"]["grp_northNeighbourZMinus"]["minHeight"] = terrainEdit.deprecated_northNeighbourZMinus.minHeight();
		quadrant["terrain_data"]["grp_northNeighbourZMinus"]["maxHeight"] = terrainEdit.deprecated_northNeighbourZMinus.maxHeight();
		quadrant["terrain_data"]["grp_noise"]["001_noiseType"] = terrainEdit.noise.noiseType;
		quadrant["terrain_data"]["grp_noise"]["002_rotationType3D"] = terrainEdit.noise.rotationType3D;
		quadrant["terrain_data"]["grp_noise"]["003_noiseSeed"] = terrainEdit.noise.noiseSeed;
		quadrant["terrain_data"]["grp_noise"]["004_frequency"] = terrainEdit.noise.frequency;
		quadrant["terrain_data"]["grp_noise"]["005_fractalType"] = terrainEdit.noise.fractalType;
		quadrant["terrain_data"]["grp_noise"]["006_fractalOctaves"] = terrainEdit.noise.fractalOctaves;
		quadrant["terrain_data"]["grp_noise"]["007_fractalLacunarity"] = terrainEdit.noise.fractalLacunarity;
		quadrant["terrain_data"]["grp_noise"]["008_fractalGain"] = terrainEdit.noise.fractalGain;
		quadrant["terrain_data"]["grp_noise"]["009_fractalWeightedStrength"] = terrainEdit.noise.fractalWeightedStrength;
		quadrant["terrain_data"]["grp_noise"]["010_fractalPingPongStrength"] = terrainEdit.noise.fractalPingPongStrength;
		quadrant["terrain_data"]["grp_noise"]["011_cellularDistanceFunction"] = terrainEdit.noise.cellularDistanceFunction;
		quadrant["terrain_data"]["grp_noise"]["012_cellularReturnType"] = terrainEdit.noise.cellularReturnType;
		quadrant["terrain_data"]["grp_noise"]["013_cellularJitter"] = terrainEdit.noise.cellularJitter;
		quadrant["terrain_data"]["grp_noise"]["014_warpNoiseDomainWarpType"] = terrainEdit.noise.warpNoiseDomainWarpType;
		quadrant["terrain_data"]["grp_noise"]["015_warpNoiseRotationType3D"] = terrainEdit.noise.warpNoiseRotationType3D;
		quadrant["terrain_data"]["grp_noise"]["016_warpNoiseSeed"] = terrainEdit.noise.warpNoiseSeed;
		quadrant["terrain_data"]["grp_noise"]["017_warpNoiseDomainWarpAmp"] = terrainEdit.noise.warpNoiseDomainWarpAmp;
		quadrant["terrain_data"]["grp_noise"]["018_warpNoiseFrequency"] = terrainEdit.noise.warpNoiseFrequency;
		quadrant["terrain_data"]["grp_noise"]["019_warpNoieseFractalType"] = terrainEdit.noise.warpNoieseFractalType;
		quadrant["terrain_data"]["grp_noise"]["020_warpNoiseFractalOctaves"] = terrainEdit.noise.warpNoiseFractalOctaves;
		quadrant["terrain_data"]["grp_noise"]["021_warpNoiseFractalLacunarity"] = terrainEdit.noise.warpNoiseFractalLacunarity;
		quadrant["terrain_data"]["grp_noise"]["022_warpNoiseFractalGain"] = terrainEdit.noise.warpNoiseFractalGain;
		quadrant["terrain_data"]["grp_noise"]["023_heightsAmplitude"] = terrainEdit.noise.heightsAmplitude;
		quadrant["terrain_data"]["grp_noise"]["024_scaleFactor_JagIntensity"] = terrainEdit.noise.scaleFactor_JagIntensity;
		quadrant["terrain_data"]["grp_noise"]["025_desideredMinHeight"] = terrainEdit.noise.desideredMinHeight;
		quadrant["terrain_data"]["grp_noise"]["026_desideredMinHeigthMandatory"] = terrainEdit.noise.desideredMinHeigthMandatory;
	}
		
	bool DBJson::completeQuadrantDataWithDefaultValues(float gridStepInWU, size_t numVerticesPerSize, int level, Json::Value& quadrant)
	{
		float gridSizeInWU = (numVerticesPerSize - 1) * gridStepInWU;
		float lowerXGridVertex = 0.0f, lowerZGridVertex = 0.0f ;
		
		if (quadrant["lowerXGridVertex"].isNull())
		{
			if (quadrant["quadrantIndexX"].isNull())
			{
				PLOG_ERROR << " missing both lowerXGridVertex and quadrantIndexX";
				return false;
			}
			else
			{
				lowerXGridVertex = float(quadrant["quadrantIndexX"].asInt() * gridSizeInWU);
				quadrant["lowerXGridVertex"] = lowerXGridVertex;
			}
		}
		else
		{
			lowerXGridVertex = float(quadrant["lowerXGridVertex"].asDouble());
			quadrant["quadrantIndexX"] = int(floor(lowerXGridVertex / gridSizeInWU));
		}

		if (quadrant["lowerZGridVertex"].isNull())
		{
			if (quadrant["quadrantIndexZ"].isNull())
			{
				PLOG_ERROR << " missing both lowerZGridVertex and quadrantIndexZ";
				return false;
			}
			else
			{
				lowerZGridVertex = float(quadrant["quadrantIndexZ"].asInt() * gridSizeInWU);
				quadrant["lowerZGridVertex"] = lowerZGridVertex;
			}
		}
		else
		{
			lowerZGridVertex = float(quadrant["lowerZGridVertex"].asDouble());
			quadrant["quadrantIndexZ"] = int(floor(lowerZGridVertex / gridSizeInWU));
		}

		quadrant["gridStepInWU"] = gridStepInWU;
		quadrant["numVerticesPerSize"] = int(numVerticesPerSize);
		quadrant["level"] = level;
		quadrant["higherXGridVertex"] = lowerXGridVertex + (numVerticesPerSize - 1) * gridStepInWU;
		quadrant["higherZGridVertex"] = lowerZGridVertex + (numVerticesPerSize - 1) * gridStepInWU;

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

		quadrant["hash_text"] = hashWithoutBinaryTime;
		quadrant["hash_binary"] = encoded;

		TheWorld_Utils::TerrainEdit::TerrainType type = TheWorld_Utils::TerrainEdit::TerrainType::empty;
		Json::Value v = quadrant["terrain_data"]["001_terrainType"];
		if (v.isNull())
			quadrant["terrain_data"]["001_terrainType"] = int(type);
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
				quadrant["terrain_data"]["001_terrainType"] = int(type);
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
		
		quadrant["terrain_data"]["001_terrainTypeStr"] = TheWorld_Utils::TerrainEdit::terrainTypeString(type);

		TheWorld_Utils::TerrainEdit terrainEdit(type);

		v = quadrant["terrain_data"]["002_heightsNeedRegen"];
		if (v.isNull())
			quadrant["terrain_data"]["002_heightsNeedRegen"] = terrainEdit.heightsNeedRegen;
		
		v = quadrant["terrain_data"]["003_normalsNeedRegen"];
		if (v.isNull())
			quadrant["terrain_data"]["003_normalsNeedRegen"] = terrainEdit.normalsNeedRegen;

		v = quadrant["terrain_data"]["004_splatmapNeedRegen"];
		if (v.isNull())
			quadrant["terrain_data"]["004_splatmapNeedRegen"] = terrainEdit.splatmapNeedRegen();

		v = quadrant["terrain_data"]["005_colormapNeedRegen"];
		if (v.isNull())
			quadrant["terrain_data"]["005_colormapNeedRegen"] = terrainEdit.colormapNeedRegen;

		v = quadrant["terrain_data"]["006_globalmapNeedRegen"];
		if (v.isNull())
			quadrant["terrain_data"]["006_globalmapNeedRegen"] = terrainEdit.globalmapNeedRegen;

		v = quadrant["terrain_data"]["007_needUploadToServer"];
		if (v.isNull())
			quadrant["terrain_data"]["007_needUploadToServer"] = terrainEdit.needUploadToServer;

		v = quadrant["terrain_data"]["008_minHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["008_minHeight"] = terrainEdit.minHeight;

		v = quadrant["terrain_data"]["009_maxHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["009_maxHeight"] = terrainEdit.maxHeight;

		v = quadrant["terrain_data"]["010_lowElevationTexName_r"];
		if (v.isNull())
			quadrant["terrain_data"]["010_lowElevationTexName_r"] = terrainEdit.extraValues.lowElevationTexName_r;

		v = quadrant["terrain_data"]["011_highElevationTexName_g"];
		if (v.isNull())
			quadrant["terrain_data"]["011_highElevationTexName_g"] = terrainEdit.extraValues.highElevationTexName_g;

		v = quadrant["terrain_data"]["012_dirtTexName_b"];
		if (v.isNull())
			quadrant["terrain_data"]["012_dirtTexName_b"] = terrainEdit.extraValues.dirtTexName_b;

		v = quadrant["terrain_data"]["013_rocksTexName_a"];
		if (v.isNull())
			quadrant["terrain_data"]["013_rocksTexName_a"] = terrainEdit.extraValues.rocksTexName_a;

		v = quadrant["terrain_data"]["014_splatmap_slopeVerticalFactor"];
		if (v.isNull())
			quadrant["terrain_data"]["014_splatmap_slopeVerticalFactor"] = terrainEdit.extraValues.slopeVerticalFactor;

		v = quadrant["terrain_data"]["015_splatmap_slopeFlatFactor"];
		if (v.isNull())
			quadrant["terrain_data"]["015_splatmap_slopeFlatFactor"] = terrainEdit.extraValues.slopeFlatFactor;

		v = quadrant["terrain_data"]["016_splatmap_dirtOnRocksFactor"];
		if (v.isNull())
			quadrant["terrain_data"]["016_splatmap_dirtOnRocksFactor"] = terrainEdit.extraValues.dirtOnRocksFactor;

		v = quadrant["terrain_data"]["017_splatmap_highElevationFactor"];
		if (v.isNull())
			quadrant["terrain_data"]["017_splatmap_highElevationFactor"] = terrainEdit.extraValues.highElevationFactor;

		v = quadrant["terrain_data"]["018_splatmap_lowElevationFactor"];
		if (v.isNull())
			quadrant["terrain_data"]["018_splatmap_lowElevationFactor"] = terrainEdit.extraValues.lowElevationFactor;

		v = quadrant["terrain_data"]["019_splatMapMode"];
		if (v.isNull())
			quadrant["terrain_data"]["019_splatMapMode"] = int(terrainEdit.extraValues.splatMapMode);

		v = quadrant["terrain_data"]["020_amplitudeBlendFactor"];
		if (v.isNull())
			quadrant["terrain_data"]["020_amplitudeBlendFactor"] = terrainEdit.extraValues.amplitudeBlendFactor;

		v = quadrant["terrain_data"]["021_postGenerateElevation"];
		if (v.isNull())
			quadrant["terrain_data"]["021_postGenerateElevation"] = terrainEdit.extraValues.postGenerateElevation;

		v = quadrant["terrain_data"]["022_ignoreDesideredMinHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["022_ignoreDesideredMinHeight"] = terrainEdit.extraValues.ignoreDesideredMinHeight;

		v = quadrant["terrain_data"]["023_numBlendingCycles"];
		if (v.isNull())
			quadrant["terrain_data"]["023_numBlendingCycles"] = int(terrainEdit.extraValues.numBlendingCycles);

		v = quadrant["terrain_data"]["024_blendingBendingFactor"];
		if (v.isNull())
			quadrant["terrain_data"]["024_blendingBendingFactor"] = terrainEdit.extraValues.blendingBendingFactor;

		v = quadrant["terrain_data"]["025_minRandomBendingExtension"];
		if (v.isNull())
			quadrant["terrain_data"]["025_minRandomBendingExtension"] = terrainEdit.extraValues.minRandomBendingExtension;

		v = quadrant["terrain_data"]["026_maxRandomBendingExtension"];
		if (v.isNull())
			quadrant["terrain_data"]["026_maxRandomBendingExtension"] = terrainEdit.extraValues.maxRandomBendingExtension;

		v = quadrant["terrain_data"]["027_seaInfoSet"];
		if (v.isNull())
			quadrant["terrain_data"]["027_seaInfoSet"] = terrainEdit.extraValues.seaInfoSet;

		v = quadrant["terrain_data"]["028_containsSea"];
		if (v.isNull())
			quadrant["terrain_data"]["028_containsSea"] = terrainEdit.extraValues.canContainSea;

		v = quadrant["terrain_data"]["029_continentId"];
		if (v.isNull())
			quadrant["terrain_data"]["029_continentId"] = terrainEdit.extraValues1.continentId;

		v = quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer1"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer1"] = int(terrainEdit.extraValues1.northGroundTextureLayers.layer1);

		v = quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer2"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer2"] = int(terrainEdit.extraValues1.northGroundTextureLayers.layer2);

		v = quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer3"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer3"] = int(terrainEdit.extraValues1.northGroundTextureLayers.layer3);

		v = quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer4"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northGroundTextureLayers"]["layer4"] = int(terrainEdit.extraValues1.northGroundTextureLayers.layer4);

		v = quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer1"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer1"] = int(terrainEdit.extraValues1.westGroundTextureLayers.layer1);

		v = quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer2"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer2"] = int(terrainEdit.extraValues1.westGroundTextureLayers.layer2);

		v = quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer3"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer3"] = int(terrainEdit.extraValues1.westGroundTextureLayers.layer3);

		v = quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer4"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_westGroundTextureLayers"]["layer4"] = int(terrainEdit.extraValues1.westGroundTextureLayers.layer4);

		v = quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer1"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer1"] = int(terrainEdit.extraValues1.northwestGroundTextureLayers.layer1);

		v = quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer2"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer2"] = int(terrainEdit.extraValues1.northwestGroundTextureLayers.layer2);

		v = quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer3"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer3"] = int(terrainEdit.extraValues1.northwestGroundTextureLayers.layer3);

		v = quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer4"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northwestGroundTextureLayers"]["layer4"] = int(terrainEdit.extraValues1.northwestGroundTextureLayers.layer4);

		v = quadrant["terrain_data"]["grp_eastSideXPlus"]["needBlend"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_eastSideXPlus"]["needBlend"] = terrainEdit.eastSideXPlus.needBlend;

		v = quadrant["terrain_data"]["grp_eastSideXPlus"]["minHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_eastSideXPlus"]["minHeight"] = terrainEdit.eastSideXPlus.minHeight;

		v = quadrant["terrain_data"]["grp_eastSideXPlus"]["maxHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_eastSideXPlus"]["maxHeight"] = terrainEdit.eastSideXPlus.maxHeight;

		v = quadrant["terrain_data"]["grp_westSideXMinus"]["needBlend"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_westSideXMinus"]["needBlend"] = terrainEdit.westSideXMinus.needBlend;

		v = quadrant["terrain_data"]["grp_westSideXMinus"]["minHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_westSideXMinus"]["minHeight"] = terrainEdit.westSideXMinus.minHeight;

		v = quadrant["terrain_data"]["grp_westSideXMinus"]["maxHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_westSideXMinus"]["maxHeight"] = terrainEdit.westSideXMinus.maxHeight;

		v = quadrant["terrain_data"]["grp_southSideZPlus"]["needBlend"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_southSideZPlus"]["needBlend"] = terrainEdit.southSideZPlus.needBlend;

		v = quadrant["terrain_data"]["grp_southSideZPlus"]["minHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_southSideZPlus"]["minHeight"] = terrainEdit.southSideZPlus.minHeight;

		v = quadrant["terrain_data"]["grp_southSideZPlus"]["maxHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_southSideZPlus"]["maxHeight"] = terrainEdit.southSideZPlus.maxHeight;

		v = quadrant["terrain_data"]["grp_northSideZMinus"]["needBlend"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northSideZMinus"]["needBlend"] = terrainEdit.northSideZMinus.needBlend;

		v = quadrant["terrain_data"]["grp_northSideZMinus"]["minHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northSideZMinus"]["minHeight"] = terrainEdit.northSideZMinus.minHeight;

		v = quadrant["terrain_data"]["grp_northSideZMinus"]["maxHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northSideZMinus"]["maxHeight"] = terrainEdit.northSideZMinus.maxHeight;

		v = quadrant["terrain_data"]["grp_eastNeighbourXPlus"]["minHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_eastNeighbourXPlus"]["minHeight"] = terrainEdit.deprecated_eastNeighbourXPlus.minHeight();

		v = quadrant["terrain_data"]["grp_eastNeighbourXPlus"]["maxHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_eastNeighbourXPlus"]["maxHeight"] = terrainEdit.deprecated_eastNeighbourXPlus.maxHeight();

		v = quadrant["terrain_data"]["grp_westNeighbourXMinus"]["minHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_westNeighbourXMinus"]["minHeight"] = terrainEdit.deprecated_westNeighbourXMinus.minHeight();

		v = quadrant["terrain_data"]["grp_westNeighbourXMinus"]["maxHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_westNeighbourXMinus"]["maxHeight"] = terrainEdit.deprecated_westNeighbourXMinus.maxHeight();

		v = quadrant["terrain_data"]["grp_southNeighbourZPlus"]["minHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_southNeighbourZPlus"]["minHeight"] = terrainEdit.deprecated_southNeighbourZPlus.minHeight();

		v = quadrant["terrain_data"]["grp_southNeighbourZPlus"]["maxHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_southNeighbourZPlus"]["maxHeight"] = terrainEdit.deprecated_southNeighbourZPlus.maxHeight();

		v = quadrant["terrain_data"]["grp_northNeighbourZMinus"]["minHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northNeighbourZMinus"]["minHeight"] = terrainEdit.deprecated_northNeighbourZMinus.minHeight();

		v = quadrant["terrain_data"]["grp_northNeighbourZMinus"]["maxHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_northNeighbourZMinus"]["maxHeight"] = terrainEdit.deprecated_northNeighbourZMinus.maxHeight();

		v = quadrant["terrain_data"]["grp_noise"]["001_noiseType"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["001_noiseType"] = terrainEdit.noise.noiseType;

		v = quadrant["terrain_data"]["grp_noise"]["002_rotationType3D"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["002_rotationType3D"] = terrainEdit.noise.rotationType3D;

		v = quadrant["terrain_data"]["grp_noise"]["003_noiseSeed"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["003_noiseSeed"] = terrainEdit.noise.noiseSeed;

		v = quadrant["terrain_data"]["grp_noise"]["004_frequency"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["004_frequency"] = terrainEdit.noise.frequency;

		v = quadrant["terrain_data"]["grp_noise"]["005_fractalType"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["005_fractalType"] = terrainEdit.noise.fractalType;

		v = quadrant["terrain_data"]["grp_noise"]["006_fractalOctaves"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["006_fractalOctaves"] = terrainEdit.noise.fractalOctaves;

		v = quadrant["terrain_data"]["grp_noise"]["007_fractalLacunarity"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["007_fractalLacunarity"] = terrainEdit.noise.fractalLacunarity;

		v = quadrant["terrain_data"]["grp_noise"]["008_fractalGain"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["008_fractalGain"] = terrainEdit.noise.fractalGain;

		v = quadrant["terrain_data"]["grp_noise"]["009_fractalWeightedStrength"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["009_fractalWeightedStrength"] = terrainEdit.noise.fractalWeightedStrength;

		v = quadrant["terrain_data"]["grp_noise"]["010_fractalPingPongStrength"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["010_fractalPingPongStrength"] = terrainEdit.noise.fractalPingPongStrength;

		v = quadrant["terrain_data"]["grp_noise"]["011_cellularDistanceFunction"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["011_cellularDistanceFunction"] = terrainEdit.noise.cellularDistanceFunction;

		v = quadrant["terrain_data"]["grp_noise"]["012_cellularReturnType"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["012_cellularReturnType"] = terrainEdit.noise.cellularReturnType;

		v = quadrant["terrain_data"]["grp_noise"]["013_cellularJitter"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["013_cellularJitter"] = terrainEdit.noise.cellularJitter;

		v = quadrant["terrain_data"]["grp_noise"]["014_warpNoiseDomainWarpType"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["014_warpNoiseDomainWarpType"] = terrainEdit.noise.warpNoiseDomainWarpType;

		v = quadrant["terrain_data"]["grp_noise"]["015_warpNoiseRotationType3D"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["015_warpNoiseRotationType3D"] = terrainEdit.noise.warpNoiseRotationType3D;

		v = quadrant["terrain_data"]["grp_noise"]["016_warpNoiseSeed"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["016_warpNoiseSeed"] = terrainEdit.noise.warpNoiseSeed;

		v = quadrant["terrain_data"]["grp_noise"]["017_warpNoiseDomainWarpAmp"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["017_warpNoiseDomainWarpAmp"] = terrainEdit.noise.warpNoiseDomainWarpAmp;

		v = quadrant["terrain_data"]["grp_noise"]["018_warpNoiseFrequency"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["018_warpNoiseFrequency"] = terrainEdit.noise.warpNoiseFrequency;

		v = quadrant["terrain_data"]["grp_noise"]["019_warpNoieseFractalType"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["019_warpNoieseFractalType"] = terrainEdit.noise.warpNoieseFractalType;

		v = quadrant["terrain_data"]["grp_noise"]["020_warpNoiseFractalOctaves"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["020_warpNoiseFractalOctaves"] = terrainEdit.noise.warpNoiseFractalOctaves;

		v = quadrant["terrain_data"]["grp_noise"]["021_warpNoiseFractalLacunarity"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["021_warpNoiseFractalLacunarity"] = terrainEdit.noise.warpNoiseFractalLacunarity;

		v = quadrant["terrain_data"]["grp_noise"]["022_warpNoiseFractalGain"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["022_warpNoiseFractalGain"] = terrainEdit.noise.warpNoiseFractalGain;

		v = quadrant["terrain_data"]["grp_noise"]["023_heightsAmplitude"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["023_heightsAmplitude"] = terrainEdit.noise.heightsAmplitude;

		v = quadrant["terrain_data"]["grp_noise"]["024_scaleFactor_JagIntensity"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["024_scaleFactor_JagIntensity"] = terrainEdit.noise.scaleFactor_JagIntensity;

		v = quadrant["terrain_data"]["grp_noise"]["025_desideredMinHeight"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["025_desideredMinHeight"] = terrainEdit.noise.desideredMinHeight;

		v = quadrant["terrain_data"]["grp_noise"]["026_desideredMinHeigthMandatory"];
		if (v.isNull())
			quadrant["terrain_data"]["grp_noise"]["026_desideredMinHeigthMandatory"] = terrainEdit.noise.desideredMinHeigthMandatory;

		return true;
	}

	std::string DBJson::getTextureRectDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level)
	{
		std::string textureRectDirPath = getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level) + "\\MapRects";
		if (!fs::exists(textureRectDirPath))
			fs::create_directories(textureRectDirPath);
		return textureRectDirPath;
	}

	std::string DBJson::getQuadrantDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level)
	{
		return g_DBDir + "\\maps\\" + readParam(DBInterface::c_mapNameParamName) + "\\" + "ST-" + std::to_string(gridStepInWU) + "_SZ-" + std::to_string(numVerticesPerSize) + "\\L-" + std::to_string(level);
	}

	std::string DBJson::getQuadrantImportDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level)
	{
		std::string quadrantImportDirPath = getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level) + "\\Import";
		if (!fs::exists(quadrantImportDirPath))
			fs::create_directories(quadrantImportDirPath);
		return quadrantImportDirPath;
	}

	//std::string DBJson::getQuadrantWorkDirPath(float gridStepInWU, size_t numVerticesPerSize, size_t level)
	//{
	//	return getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level) + "\\Work";
	//}

	std::string DBJson::getQuadrantFilePath(float gridStepInWU, size_t numVerticesPerSize, size_t level, float lowerXGridVertex, float lowerZGridVertex)
	{
		std::string quadrantDirPath =getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level) + "\\X-" + std::to_string(lowerXGridVertex) + "_Z-" + std::to_string(lowerZGridVertex) + ".json";
		return quadrantDirPath;
	}

	void DBJson::getAllQuadrants(std::string _cacheDir, std::string mapName, float gridStepInWU, size_t numVerticesPerSize, int level, std::map<TheWorld_Utils::QuadrantPos, TheWorld_Utils::MeshCacheBuffer>& diskCacheMap, std::time_t& fromFileTimeType)
	{
		std::string quadrantDirPath = getQuadrantDirPath(gridStepInWU, numVerticesPerSize, level);
		if (!fs::exists(quadrantDirPath))
			return;

		std::time_t _fromFileTimeType = fromFileTimeType;
		fromFileTimeType = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		for (const auto& entry : fs::directory_iterator(quadrantDirPath))
			if (entry.is_regular_file())
			{
				auto fileTimeType = entry.last_write_time();
				const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTimeType);
				const auto fileTime = std::chrono::system_clock::to_time_t(systemTime);
				if (fileTime > _fromFileTimeType)
				{
					std::string filename = entry.path().filename().generic_string();
					size_t pos = filename.find_last_of('.');
					if (pos != std::string::npos)
					{
						std::string filenameNoExt = filename.substr(0, pos);
						std::vector<std::string> v1 = TheWorld_Utils::Utils::split(filenameNoExt, '_');
						if (v1.size() == 2)
						{
							float lowerXGridVertex = FLT_MAX, lowerZGridVertex = FLT_MAX;

							pos = v1[0].find_first_of('-');
							if (pos != std::string::npos)
							{
								std::string coord = v1[0].substr(0, pos);
								std::string value = v1[0].substr(pos + 1);
								if (coord == "X")
									lowerXGridVertex = std::stof(value);
							}

							pos = v1[1].find_first_of('-');
							if (pos != std::string::npos)
							{
								std::string coord = v1[1].substr(0, pos);
								std::string value = v1[1].substr(pos + 1);
								if (coord == "Z")
									lowerZGridVertex = std::stof(value);
							}

							if (lowerXGridVertex != FLT_MAX && lowerZGridVertex != FLT_MAX)
							{
								TheWorld_Utils::QuadrantPos pos(lowerXGridVertex, lowerZGridVertex, level, int(numVerticesPerSize), gridStepInWU);
								diskCacheMap[pos] = TheWorld_Utils::MeshCacheBuffer(_cacheDir, mapName, gridStepInWU, numVerticesPerSize, level, lowerXGridVertex, lowerZGridVertex);
							}
						}
					}
				}
			}
	}
}