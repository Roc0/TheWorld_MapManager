#include "pch.h"

#include "Utils.h"
#include "DBInterface.h"
#include "MapManager_Utils.h"
#include "DBSQLLite.h"
#include "DBJson.h"

namespace TheWorld_MapManager
{
	const std::string DBInterface::c_mapNameParamName = "MapName";
	const std::string DBInterface::c_gridStepInWUParamName = "GridStepInWU";
	const std::string DBInterface::c_growingBlockVertexNumberShiftParamName = "GrowingBlockVertexNumberShift";

	std::recursive_mutex DBInterface::s_DBAccessMtx;
	
	std::recursive_mutex& DBInterface::getExclusiveDBAccessMutex(void)
	{
		//TheWorld_Utils::Timer<std::chrono::milliseconds, std::chrono::steady_clock> clock(false, false);
		return s_DBAccessMtx;
	}

	void DBInterface::acquireExclusiveAccessToDB(void)
	{
		s_DBAccessMtx.lock();
	}

#pragma warning(push)
#pragma warning(disable : 26110)
	void DBInterface::releaseExclusiveAccessToDB(void)
	{
		s_DBAccessMtx.unlock();
	}
#pragma warning(pop)
}