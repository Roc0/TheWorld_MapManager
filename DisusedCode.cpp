////*************************************************
////*************************************************
////*************************************************
////*************************************************
////*************************************************
//{
//	struct TheWorld_Utils::_RGB rgbNormal;
//	rgbNormal.r = 109;	rgbNormal.g = 14;	rgbNormal.b = 184;
//	Eigen::Vector3d packedNormal((const double)(double(rgbNormal.r) / 255), (const double)(double(rgbNormal.g) / 255), (const double)(double(rgbNormal.b) / 255));	// 0.0f-1.0f
//	float normalPackedX1 = packedNormal.x();
//	float normalPackedY1 = packedNormal.y();
//	float normalPackedZ1 = packedNormal.z();
//	Eigen::Vector3d normal = TheWorld_Utils::unpackNormal(packedNormal);
//	float normalX = normal.x();
//	float normalY = normal.y();
//	float normalZ = normal.z();
//	packedNormal = TheWorld_Utils::packNormal(normal);
//	float normalPackedX2 = packedNormal.x();
//	float normalPackedY2 = packedNormal.y();
//	float normalPackedZ2 = packedNormal.z();
//	rgbNormal.r = (BYTE)(packedNormal.x() * 255);
//	rgbNormal.g = (BYTE)(packedNormal.y() * 255);
//	rgbNormal.b = (BYTE)(packedNormal.z() * 255);
//}
////*************************************************
////*************************************************
////*************************************************
////*************************************************
////*************************************************

////*************************************************
////*************************************************
////*************************************************
////*************************************************
////*************************************************
//{
//	float TESTSTARTX = 0.0f;
//	float TESTSTARTZ = 0.0f;

//	std::string buffer;

//	std::string cacheDir = m_SqlInterface->dataPath();
//	gridStepInWU = MapManager::gridStepInWU();
//	std::string mapName = getMapName();
//	TheWorld_Utils::MeshCacheBuffer cache = TheWorld_Utils::MeshCacheBuffer(cacheDir, mapName, gridStepInWU, numVerticesPerSize, level, TESTSTARTX, TESTSTARTZ);;

//	// Disk Cache Quadrant
//	std::string diskCache_meshId = cache.getMeshIdFromDisk();
//	TheWorld_Utils::TerrainEdit diskCache_terrainEditValues;
//	TheWorld_Utils::MemoryBuffer diskCache__terrainEditValues;
//	TheWorld_Utils::MemoryBuffer diskCache_heights16Buffer;
//	TheWorld_Utils::MemoryBuffer diskCache_heights32Buffer;
//	TheWorld_Utils::MemoryBuffer diskCache_normalsBuffer;
//	TheWorld_Utils::MemoryBuffer diskCache_splatmapBuffer;
//	TheWorld_Utils::MemoryBuffer diskCache_colormapBuffer;
//	TheWorld_Utils::MemoryBuffer diskCache_globalmapBuffer;
//	TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData diskCache_cacheQuadrantData;
//	diskCache_cacheQuadrantData.meshId = diskCache_meshId;
//	diskCache_cacheQuadrantData.terrainEditValues = &diskCache__terrainEditValues;
//	diskCache_cacheQuadrantData.heights16Buffer = &diskCache_heights16Buffer;
//	diskCache_cacheQuadrantData.heights32Buffer = &diskCache_heights32Buffer;
//	diskCache_cacheQuadrantData.normalsBuffer = &diskCache_normalsBuffer;
//	diskCache_cacheQuadrantData.splatmapBuffer = &diskCache_splatmapBuffer;
//	diskCache_cacheQuadrantData.colormapBuffer = &diskCache_colormapBuffer;
//	diskCache_cacheQuadrantData.globalmapBuffer = &diskCache_globalmapBuffer;
//	if (diskCache_meshId.size() > 0)
//	{
//		cache.readBufferFromDisk(diskCache_meshId, buffer);
//		cache.refreshCacheQuadrantDataFromBuffer(buffer, diskCache_cacheQuadrantData, false);
//		diskCache_terrainEditValues.deserialize(diskCache__terrainEditValues);
//	}

//	// DB Quadrant
//	std::string db_meshId;
//	TheWorld_Utils::TerrainEdit db_terrainEditValues;
//	TheWorld_Utils::MemoryBuffer db_heights16Buffer;
//	TheWorld_Utils::MemoryBuffer db_heights32Buffer;
//	TheWorld_Utils::MemoryBuffer db_normalsBuffer;
//	TheWorld_Utils::MemoryBuffer db_splatmapBuffer;
//	TheWorld_Utils::MemoryBuffer db_colormapBuffer;
//	TheWorld_Utils::MemoryBuffer db_globalmapBuffer;
//	enum class SQLInterface::QuadrantStatus status;
//	enum class SQLInterface::QuadrantVertexStoreType vertexStoreType;
//	m_SqlInterface->readQuadrantFromDB(cache, db_meshId, buffer, status, vertexStoreType, db_terrainEditValues);
//	if (status == SQLInterface::QuadrantStatus::Complete && db_meshId.size() != 0 && vertexStoreType == SQLInterface::QuadrantVertexStoreType::eXtended)
//	{

//		float gridStep = cache.getGridStepInWU();
//		int level = cache.getLevel();
//		float quadPosX = cache.getLowerXGridVertex();
//		float quadPosZ = cache.getLowerZGridVertex();
//		float quadEndPosX = quadPosX + (numVerticesPerSize - 1) * gridStep;
//		float quadEndPosZ = quadPosZ + (numVerticesPerSize - 1) * gridStep;

//		int numFoundInDB = 0;
//		std::vector<SQLInterface::GridVertex> vectGridVertex;
//		internalGetVertices(quadPosX, quadEndPosX, quadPosZ, quadEndPosZ, (int)numVerticesPerSize, (int)numVerticesPerSize, vectGridVertex, gridStep, numFoundInDB, level);
//		size_t numVertices = vectGridVertex.size();
//		if (numVertices != 0 && numVertices != numVerticesPerSize * numVerticesPerSize)
//			throw(MapManagerException(__FUNCTION__, (std::string("internalGetVertices returned unexpected size: ") + std::to_string(numVertices) + "(expected" + std::to_string(numVerticesPerSize * numVerticesPerSize) + ")").c_str()));

//		TheWorld_Utils::MemoryBuffer buffer;

//		float minHeight = FLT_MAX, maxHeight = -FLT_MAX;

//		if (numFoundInDB > 0 && numVertices > 0)
//		{
//			db_heights16Buffer.reserve(numVertices * sizeof(uint16_t));
//			uint16_t* _tempHeights16BufferPtr = (uint16_t*)db_heights16Buffer.ptr();
//			db_heights32Buffer.reserve(numVertices * sizeof(float));
//			float* _tempHeights32BufferPtr = (float*)db_heights32Buffer.ptr();
//			db_normalsBuffer.reserve(numVertices * sizeof(TheWorld_Utils::_RGB));
//			TheWorld_Utils::_RGB* _tempNormalsBufferPtr = (TheWorld_Utils::_RGB*)db_normalsBuffer.ptr();
//			db_splatmapBuffer.reserve(numVertices * sizeof(TheWorld_Utils::_RGBA));
//			TheWorld_Utils::_RGBA* _tempSplatmapBufferPtr = (TheWorld_Utils::_RGBA*)db_splatmapBuffer.ptr();
//			db_colormapBuffer.reserve(numVertices * sizeof(TheWorld_Utils::_RGBA));
//			TheWorld_Utils::_RGBA* _tempColormapBufferPtr = (TheWorld_Utils::_RGBA*)db_colormapBuffer.ptr();
//			db_globalmapBuffer.reserve(numVertices * sizeof(TheWorld_Utils::_RGB));
//			TheWorld_Utils::_RGB* _tempGlobalmapBufferPtr = (TheWorld_Utils::_RGB*)db_globalmapBuffer.ptr();

//			bool normalMapEmpty = false, splatMapEmpty = false, colorMapEmpty = false, globalMapEmpty = false;

//			size_t x = 0, z = 0;
//			for (auto& gridVertex : vectGridVertex)
//			{
//				TheWorld_Utils::FLOAT_32 f;
//				f.f32 = gridVertex.altitude();

//				if (f.f32 < minHeight)
//					minHeight = f.f32;

//				if (f.f32 > maxHeight)
//					maxHeight = f.f32;

//				*_tempHeights16BufferPtr = TheWorld_Utils::MeshCacheBuffer::halfFromFloat(f.u32);
//				_tempHeights16BufferPtr++;

//				*_tempHeights32BufferPtr = f.f32;
//				_tempHeights32BufferPtr++;

//				if (!normalMapEmpty)
//				{
//					if (gridVertex.normX() == 0 && gridVertex.normY() == 0 && gridVertex.normZ() == 0)
//						normalMapEmpty = true;
//					else
//					{
//						Eigen::Vector3d normal(gridVertex.normX(), gridVertex.normY(), gridVertex.normZ());
//						Eigen::Vector3d packedNormal = TheWorld_Utils::packNormal(normal);
//						(*_tempNormalsBufferPtr).r = (BYTE)(packedNormal.x() * 255);	// normals coords range from 0 to 1 but if expressed as color in a normalmap range from 0 to 255
//						(*_tempNormalsBufferPtr).g = (BYTE)(packedNormal.y() * 255);
//						(*_tempNormalsBufferPtr).b = (BYTE)(packedNormal.z() * 255);
//						_tempNormalsBufferPtr++;
//					}
//				}

//				if (!splatMapEmpty)
//				{
//					if (gridVertex.lowElevationTexAmount() == -1 && gridVertex.highElevationTexAmount() == -1 && gridVertex.dirtTexAmount() == -1 && gridVertex.rocksTexAmount() == -1)
//						splatMapEmpty = true;
//					else
//					{
//						(*_tempSplatmapBufferPtr).r = BYTE(gridVertex.lowElevationTexAmount());
//						(*_tempSplatmapBufferPtr).g = BYTE(gridVertex.highElevationTexAmount());
//						(*_tempSplatmapBufferPtr).b = BYTE(gridVertex.dirtTexAmount());
//						(*_tempSplatmapBufferPtr).a = BYTE(gridVertex.rocksTexAmount());
//						_tempSplatmapBufferPtr++;
//					}
//				}

//				if (!colorMapEmpty)
//				{
//					if (gridVertex.colorR() == -1 && gridVertex.colorG() == -1 && gridVertex.colorB() == -1 && gridVertex.colorA() == -1)
//						colorMapEmpty = true;
//					else
//					{
//						(*_tempColormapBufferPtr).r = BYTE(gridVertex.colorR());
//						(*_tempColormapBufferPtr).g = BYTE(gridVertex.colorG());
//						(*_tempColormapBufferPtr).b = BYTE(gridVertex.colorB());
//						(*_tempColormapBufferPtr).a = BYTE(gridVertex.colorA());
//						_tempColormapBufferPtr++;
//					}
//				}

//				if (!globalMapEmpty)
//				{
//					if (gridVertex.globalMapR() == -1 && gridVertex.globalMapG() == -1 && gridVertex.globalMapB() == -1)
//						globalMapEmpty = true;
//					else
//					{
//						(*_tempGlobalmapBufferPtr).r = BYTE(gridVertex.globalMapR());
//						(*_tempGlobalmapBufferPtr).g = BYTE(gridVertex.globalMapG());
//						(*_tempGlobalmapBufferPtr).b = BYTE(gridVertex.globalMapB());
//						_tempGlobalmapBufferPtr++;
//					}
//				}

//				x++;
//				if (x == numVerticesPerSize)
//				{
//					x = 0;
//					z++;
//				}
//			}

//			if (x != 0 && z != numVerticesPerSize)
//				throw(MapManagerException(__FUNCTION__, (std::string("something wrong iterating grid vertices: x=") + std::to_string(x) + " z=" + std::to_string(z)).c_str()));

//			db_heights16Buffer.adjustSize(numVertices * sizeof(uint16_t));
//			db_heights32Buffer.adjustSize(numVertices * sizeof(float));
//			if (normalMapEmpty)
//				db_normalsBuffer.clear();
//			else
//				db_normalsBuffer.adjustSize(numVertices * sizeof(TheWorld_Utils::_RGB));
//			if (splatMapEmpty)
//				db_splatmapBuffer.clear();
//			else
//				db_splatmapBuffer.adjustSize(numVertices * sizeof(TheWorld_Utils::_RGBA));
//			if (colorMapEmpty)
//				db_colormapBuffer.clear();
//			else
//				db_colormapBuffer.adjustSize(numVertices * sizeof(TheWorld_Utils::_RGBA));
//			if (globalMapEmpty)
//				db_globalmapBuffer.clear();
//			else
//				db_globalmapBuffer.adjustSize(numVertices * sizeof(TheWorld_Utils::_RGB));

//			TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData cacheQuadrantData;
//			cacheQuadrantData.meshId = meshId;

//			if (db_heights16Buffer.size() > 0)
//				cacheQuadrantData.heights16Buffer = &db_heights16Buffer;

//			if (db_heights32Buffer.size() > 0)
//				cacheQuadrantData.heights32Buffer = &db_heights32Buffer;

//			if (db_normalsBuffer.size() > 0)
//				cacheQuadrantData.normalsBuffer = &db_normalsBuffer;
//			else
//				db_terrainEditValues.normalsNeedRegen = true;

//			if (db_splatmapBuffer.size() > 0)
//				cacheQuadrantData.splatmapBuffer = &db_splatmapBuffer;
//			else
//				db_terrainEditValues.extraValues.splatmapNeedRegen = true;

//			if (db_colormapBuffer.size() > 0)
//				cacheQuadrantData.colormapBuffer = &db_colormapBuffer;
//			else
//				db_terrainEditValues.extraValues.emptyColormap = true;

//			if (db_globalmapBuffer.size() > 0)
//				cacheQuadrantData.globalmapBuffer = &db_globalmapBuffer;
//			else
//				db_terrainEditValues.extraValues.emptyGlobalmap = true;

//			TheWorld_Utils::MemoryBuffer terrainEditValues;
//			if (db_terrainEditValues.size > 0)
//			{
//				db_terrainEditValues.serialize(terrainEditValues);
//				cacheQuadrantData.terrainEditValues = &terrainEditValues;
//				cacheQuadrantData.minHeight = db_terrainEditValues.minHeight;
//				cacheQuadrantData.maxHeight = db_terrainEditValues.maxHeight;
//			}
//			if (minHeight != FLT_MAX)
//			{
//				cacheQuadrantData.minHeight = minHeight;
//			}
//			if (maxHeight != -FLT_MAX)
//			{
//				cacheQuadrantData.maxHeight = maxHeight;
//			}

//			cache.setBufferFromCacheQuadrantData(cache.getNumVerticesPerSize(), cacheQuadrantData, buffer);
//		}
//	}

//	// Compare
//	bool match = true;

//	if ( !(diskCache_terrainEditValues == db_terrainEditValues) )
//		match = false;

//	if (diskCache_heights16Buffer.size() != db_heights16Buffer.size())
//		match = false;
//	if (diskCache_heights32Buffer.size() != db_heights32Buffer.size())
//		match = false;
//	if (diskCache_normalsBuffer.size() != db_normalsBuffer.size())
//		match = false;
//	if (diskCache_splatmapBuffer.size() != db_splatmapBuffer.size())
//		match = false;
//	if (diskCache_colormapBuffer.size() != db_colormapBuffer.size())
//		match = false;
//	if (diskCache_globalmapBuffer.size() != db_globalmapBuffer.size())
//		match = false;

//	for (size_t z = 0; z < numVerticesPerSize; z++)
//	{
//		for (size_t x = 0; x < numVerticesPerSize; x++)
//		{
//			if (diskCache_heights16Buffer.size() > 0 && db_heights16Buffer.size() > 0)
//			{
//				uint16_t diskCache_vertexPosY_16 = diskCache_heights16Buffer.at<uint16_t>(x, z, numVerticesPerSize);
//				uint16_t db_vertexPosY_16 = db_heights16Buffer.at<uint16_t>(x, z, numVerticesPerSize);
//				if (diskCache_vertexPosY_16 != db_vertexPosY_16)
//					match = false;
//			}

//			if (diskCache_heights32Buffer.size() > 0 && db_heights32Buffer.size() > 0)
//			{
//				float diskCache_vertexPosY_32 = diskCache_heights32Buffer.at<float>(x, z, numVerticesPerSize);
//				float db_vertexPosY_32 = db_heights32Buffer.at<float>(x, z, numVerticesPerSize);
//				if (diskCache_vertexPosY_32 != db_vertexPosY_32)
//					match = false;
//			}

//			if (diskCache_normalsBuffer.size() > 0 && db_normalsBuffer.size() > 0)
//			{
//				struct TheWorld_Utils::_RGB diskCache_rgbNormal = diskCache_normalsBuffer.at<TheWorld_Utils::_RGB>(x, z, numVerticesPerSize);					// 0-255
//				struct TheWorld_Utils::_RGB db_rgbNormal = db_normalsBuffer.at<TheWorld_Utils::_RGB>(x, z, numVerticesPerSize);									// 0-255
//				if (diskCache_rgbNormal.r != db_rgbNormal.r || diskCache_rgbNormal.g != db_rgbNormal.g || diskCache_rgbNormal.b != db_rgbNormal.b)
//					match = false;
//			}

//			if (diskCache_splatmapBuffer.size() > 0 && db_splatmapBuffer.size() > 0)
//			{
//				struct TheWorld_Utils::_RGBA diskCache_rgbaSplat = diskCache_splatmapBuffer.at<TheWorld_Utils::_RGBA>(x, z, numVerticesPerSize);					// 0-255
//				struct TheWorld_Utils::_RGBA db_rgbaSplat = db_splatmapBuffer.at<TheWorld_Utils::_RGBA>(x, z, numVerticesPerSize);									// 0-255
//				if (diskCache_rgbaSplat.r != db_rgbaSplat.r || diskCache_rgbaSplat.g != db_rgbaSplat.g || diskCache_rgbaSplat.b != db_rgbaSplat.b || diskCache_rgbaSplat.a != db_rgbaSplat.a)
//					match = false;
//			}

//			if (diskCache_colormapBuffer.size() > 0 && db_colormapBuffer.size() > 0)
//			{
//				struct TheWorld_Utils::_RGBA diskCache_rgbaColor = diskCache_colormapBuffer.at<TheWorld_Utils::_RGBA>(x, z, numVerticesPerSize);					// 0-255
//				struct TheWorld_Utils::_RGBA db_rgbaColor = db_colormapBuffer.at<TheWorld_Utils::_RGBA>(x, z, numVerticesPerSize);									// 0-255
//				if (diskCache_rgbaColor.r != db_rgbaColor.r || diskCache_rgbaColor.g != db_rgbaColor.g || diskCache_rgbaColor.b != db_rgbaColor.b || diskCache_rgbaColor.a != db_rgbaColor.a)
//					match = false;
//			}

//			if (diskCache_globalmapBuffer.size() > 0 && db_globalmapBuffer.size() > 0)
//			{
//				struct TheWorld_Utils::_RGB diskCache_rgbGlobal = diskCache_globalmapBuffer.at<TheWorld_Utils::_RGB>(x, z, numVerticesPerSize);					// 0-255
//				struct TheWorld_Utils::_RGB db_rgbGlobal = db_globalmapBuffer.at<TheWorld_Utils::_RGB>(x, z, numVerticesPerSize);									// 0-255
//				if (diskCache_rgbGlobal.r != db_rgbGlobal.r || diskCache_rgbGlobal.g != db_rgbGlobal.g || diskCache_rgbGlobal.b != db_rgbGlobal.b)
//					match = false;
//			}
//		}
//	}
//}
////*************************************************
////*************************************************
////*************************************************
////*************************************************
////*************************************************

////*************************************************
////*************************************************
////*************************************************
////*************************************************
////*************************************************
//{
//	std::string newMeshId1 = cache.generateNewMeshId();
//	std::string newMeshId2 = cache.generateNewMeshId();
//	bool b = cache.firstMeshIdMoreRecent(newMeshId1, newMeshId2);
//	assert(b == false);
//	b = cache.firstMeshIdMoreRecent(newMeshId2, newMeshId1);
//	assert(b == true);
//	std::string newMeshId3 = cache.generateNewMeshId();
//}
////*************************************************
////*************************************************
////*************************************************
////*************************************************
////*************************************************

////*************************************************
////*************************************************
////*************************************************
////*************************************************
////*************************************************
		//plog::Severity sev = plog::get()->getMaxSeverity();
		//PLOG_DEBUG << "PLOG_DEBUG MapManager::getQuadrantVertices";	// RELEASEDEBUG
		//PLOG_INFO << "PLOG_INFO MapManager::getQuadrantVertices - sev:" << sev;	// RELEASEDEBUG
////*************************************************
////*************************************************
////*************************************************
////*************************************************
////*************************************************
