#include "pch.h"

#include "json/json.h"
#include <iostream>
#include <fstream>
#include <string>

#include "Utils.h"
#include "DBSQLLite.h"

using namespace std;

namespace TheWorld_MapManager
{
	DBSQLLiteConn DBSQLLiteOps::s_conn;
		
	DBSQLLite::DBSQLLite(DBType _dbt, const char* _dataPath, bool _debugMode) : SQLInterface(_dbt, _dataPath, _debugMode)
	{
		Json::Value root;
		std::ifstream jsonFile(dataPath() + "\\Conf\\DBSQLLite.json");
		jsonFile >> root;
		m_dbFilePath = dataPath() + "\\" + root["DBFilePath"].asString();

		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		dbOps.reset();
	}

	DBSQLLite::~DBSQLLite()
	{
		if (m_dbOpsInternalTransaction.isTransactionOpened())
			m_dbOpsInternalTransaction.endTransaction(false);
	}

	void DBSQLLite::beginTransaction(void)
	{
		if (!m_dbOpsInternalTransaction.isInitialized())
			m_dbOpsInternalTransaction.init(dbFilePath());

		if (m_dbOpsInternalTransaction.isTransactionOpened())
			throw(MapManagerExceptionDBException("DB Transaction already opened!", m_dbOpsInternalTransaction.errMsg()));

		m_dbOpsInternalTransaction.beginTransaction();
	}

	void DBSQLLite::endTransaction(bool commit)
	{
		if (!m_dbOpsInternalTransaction.isInitialized())
			m_dbOpsInternalTransaction.init(dbFilePath());

		if (!m_dbOpsInternalTransaction.isTransactionOpened())
			throw(MapManagerExceptionDBException("Transaction not opened!", m_dbOpsInternalTransaction.errMsg()));

		m_dbOpsInternalTransaction.endTransaction(commit);
	}

	__int64 DBSQLLite::addWD(WorldDefiner& WD, vector<MapVertex>& mapVertices)
	{
		vector<sqlite3_int64> MapVertexRowId;
		debugUtils debugUtil;

		/*
		* Initialize DB operations
		*/
		DBSQLLiteOps* dbOps = NULL;
		DBSQLLiteOps temporarydbOps(dbFilePath());
		bool endTransactionRequired = true;
		if (m_dbOpsInternalTransaction.isTransactionOpened())
		{
			dbOps = &m_dbOpsInternalTransaction;
			endTransactionRequired = false;
		}
		else
		{
			temporarydbOps.init();
			temporarydbOps.beginTransaction();
			dbOps = &temporarydbOps;
		}

		/*
		* INSERT in table WorldDefiner
		*/
		string sql = "INSERT INTO WorldDefiner (PosX, PosZ, radius, azimuth, azimuthDegree, Level, Type, Strength, AOE, FunctionType) VALUES ("
			+ std::to_string(WD.getPosX())
			+ "," + std::to_string(WD.getPosZ())
			+ "," + std::to_string(WD.getRadius())
			+ "," + std::to_string(WD.getAzimuth())
			+ "," + std::to_string(WD.getAzimuthDegree())
			+ "," + std::to_string(WD.getLevel())
			+ "," + std::to_string((int)WD.getType())
			+ "," + std::to_string(WD.getStrength())
			+ "," + std::to_string(WD.getAOE())
			+ "," + std::to_string((int)WD.getFunctionType())
			+ ");";
		dbOps->acquireLock();
		int rc = sqlite3_exec(dbOps->getConn(), sql.c_str(), NULL, NULL, NULL);
		sqlite3_int64 WDRowID = sqlite3_last_insert_rowid(dbOps->getConn());
		dbOps->releaseLock();
		if (rc != SQLITE_OK && rc != SQLITE_CONSTRAINT)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite insert World Definer failed!", dbOps->errMsg(), rc));
		if (rc == SQLITE_CONSTRAINT)
			return -1;
			//throw(MapManagerExceptionDuplicate(__FUNCTION__, "DB SQLite World Definer duplicate!"));

		/*
		* INSERT in table MapVertex
		*/
		int numVertices = (int)mapVertices.size();
		int idx = 0;
		int affectedByWD = 0;
		int notAffectedByWD = 0;
		int inserted = 0;
		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing vertices to MapVertex Table: ");
		if (numVertices > 0)
		{
			string sql = "INSERT INTO MapVertex (PosX, PosZ, radius, azimuth, Level, PosY) VALUES (?, ?, ?, ?, ?, ?);";
			dbOps->prepareStmt(sql.c_str());

			for (idx = 0; idx < numVertices; idx++)
			{
				bool vertexAffectedByWD = false;
				if (getDistance(WD.getPosX(), WD.getPosZ(), mapVertices[idx].posX(), mapVertices[idx].posZ()) <= WD.getAOE())
				{
					vertexAffectedByWD = true;
					affectedByWD++;
				}
				else
					notAffectedByWD++;

				rc = sqlite3_bind_double(dbOps->getStmt(), 1, mapVertices[idx].posX());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind MapVertex.PosX failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				rc = sqlite3_bind_double(dbOps->getStmt(), 2, mapVertices[idx].posZ());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind MapVertex.PosZ failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				rc = sqlite3_bind_double(dbOps->getStmt(), 3, mapVertices[idx].radius());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind MapVertex.radius failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				rc = sqlite3_bind_double(dbOps->getStmt(), 4, mapVertices[idx].azimuth());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind MapVertex.azimuth failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				rc = sqlite3_bind_int(dbOps->getStmt(), 5, mapVertices[idx].level());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind MapVertex.level failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				rc = sqlite3_bind_double(dbOps->getStmt(), 6, 0.0);	// Not affected vertices have 0.0 altitude, for affected vertices altitude will be computed later as they are inserted in MapVertex_Mod table
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind MapVertex.PosY failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->acquireLock();
				rc = sqlite3_step(dbOps->getStmt());
				sqlite3_int64 VertexRowId = sqlite3_last_insert_rowid(dbOps->getConn());
				dbOps->releaseLock();
				if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				if (rc == SQLITE_DONE)
					inserted++;
				
				dbOps->resetStmt();

				if (vertexAffectedByWD)
				{
					if (rc == SQLITE_CONSTRAINT)
					{
						// The Vertex was present in MapVertex table so we have to acquire his rowid
						DBSQLLiteOps dbOps1(dbFilePath());
						dbOps1.init();
						string sql = "SELECT rowid FROM MapVertex WHERE PosX = %s AND PosZ = %s AND Level = %s;";
						string sql1 = dbOps1.completeSQL(sql.c_str(), to_string(mapVertices[idx].posX()).c_str(), to_string(mapVertices[idx].posZ()).c_str(), to_string(mapVertices[idx].level()).c_str());
						dbOps1.prepareStmt(sql1.c_str());
						dbOps1.acquireLock();
						rc = sqlite3_step(dbOps1.getStmt());
						dbOps1.releaseLock();
						if (rc != SQLITE_ROW)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read rowid vertex from MapVertex table!", sqlite3_errmsg(dbOps1.getConn()), rc));
						VertexRowId = sqlite3_column_int64(dbOps1.getStmt(), 0);
						dbOps1.finalizeStmt();
					}

					MapVertexRowId.push_back(VertexRowId);
				}

				if (debugMode() && fmod(idx, 1024 * 100) == 0)
				{
					string s = to_string(idx + 1);	s += " - Affected ";	s += to_string(affectedByWD);	s += " - Not affected ";	s += to_string(notAffectedByWD);	s += " - Inserted ";	s += to_string(inserted);
					debugUtil.printVariablePartOfLine(s.c_str());
				}
			}

			dbOps->finalizeStmt();
		}
		if (debugMode())
		{
			string s = to_string(idx);	s += " - Affected ";	s += to_string(affectedByWD);	s += " - Not affected ";	s += to_string(notAffectedByWD);	s += " - Inserted ";	s += to_string(inserted);
			debugUtil.printVariablePartOfLine(s.c_str());
		}

		/*
		* INSERT in table MapVertex_WD affecting WD
		*/
		int numAffectedVertices = (int)MapVertexRowId.size();
		idx = 0;
		inserted = 0;
		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing affected vertices to MapVertex_WD Table: ");
		if (numAffectedVertices > 0)
		{
			string sql = "INSERT INTO MapVertex_WD (VertexRowId, WDRowId) VALUES (?, ?);";
			dbOps->prepareStmt(sql.c_str());

			for (idx = 0; idx < numAffectedVertices; idx++)
			{
				rc = sqlite3_bind_int64(dbOps->getStmt(), 1, MapVertexRowId[idx]);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB MapVertex_WD.VertexRowId PosX failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				rc = sqlite3_bind_int64(dbOps->getStmt(), 2, WDRowID);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB MapVertex_WD.VertexRowId PosX failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->acquireLock();
				rc = sqlite3_step(dbOps->getStmt());
				dbOps->releaseLock();
				if (rc == SQLITE_DONE)
					inserted++;
				else
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->resetStmt();

				if (debugMode() && fmod(idx, 1024 * 100) == 0)
				{
					string s = to_string(idx + 1);	s += " - Inserted ";	s += to_string(inserted);
					debugUtil.printVariablePartOfLine(s.c_str());
				}
			}

			dbOps->finalizeStmt();
		}
		if (debugMode())
		{
			string s = to_string(idx);	s += " - Inserted ";	s += to_string(inserted);
			debugUtil.printVariablePartOfLine(s.c_str());
		}

		/*
		* INSERT in table MapVertex_Mod vertices affected by WD inserted
		*/
		idx = 0;
		inserted = 0;
		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing affected vertices to MapVertex_Mod Table: ");
		if (numAffectedVertices > 0)
		{
			string sql = "INSERT INTO MapVertex_Mod (VertexRowId) VALUES (?);";
			dbOps->prepareStmt(sql.c_str());

			for (idx = 0; idx < numAffectedVertices; idx++)
			{
				// insert vertices not affected by WD (to complete the squres)
				// affected vertices will be inserted in MapVertex_Mod table to be computed later
				rc = sqlite3_bind_int64(dbOps->getStmt(), 1, MapVertexRowId[idx]);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind MapVertex_Mod.VertexRowId failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->acquireLock();
				rc = sqlite3_step(dbOps->getStmt());
				dbOps->releaseLock();
				if (rc == SQLITE_DONE)
					inserted++;
				if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->resetStmt();

				if (debugMode() && fmod(idx, 1024 * 100) == 0)
				{
					string s = to_string(idx + 1);	s += " - Inserted ";	s += to_string(inserted);
					debugUtil.printVariablePartOfLine(s.c_str());
				}
			}

			dbOps->finalizeStmt();
		}
		if (debugMode())
		{
			string s = to_string(idx);	s += " - Inserted ";	s += to_string(inserted);
			debugUtil.printVariablePartOfLine(s.c_str());
		}

		/*
		* Finalize DB operations
		*/
		if (endTransactionRequired)
		{
			dbOps->endTransaction();
			dbOps->reset();
		}

		return WDRowID;
	}

	bool DBSQLLite::eraseWD(__int64 wd_rowid)
	{
		debugUtils debugUtil;

		/*
		* Initialize DB operations
		*/
		DBSQLLiteOps* dbOps = NULL;
		DBSQLLiteOps temporarydbOps(dbFilePath());
		bool endTransactionRequired = true;
		if (m_dbOpsInternalTransaction.isTransactionOpened())
		{
			dbOps = &m_dbOpsInternalTransaction;
			endTransactionRequired = false;
		}
		else
		{
			temporarydbOps.init();
			temporarydbOps.beginTransaction();
			dbOps = &temporarydbOps;
		}

		/*
		* DELETE WD from WorldDefiner
		*/
		string sql = "DELETE FROM WorldDefiner WHERE rowid = %s;";
		string sql1 = dbOps->completeSQL(sql.c_str(), to_string(wd_rowid).c_str());
		dbOps->prepareStmt(sql1.c_str());
		dbOps->acquireLock();
		int rc = sqlite3_step(dbOps->getStmt());
		int numDeleted = sqlite3_changes(dbOps->getConn());
		dbOps->releaseLock();
		if (rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to update vertex altitude in MapVertex table!", sqlite3_errmsg(dbOps->getConn()), rc));
		dbOps->finalizeStmt();
		
		if (numDeleted > 1)
			throw(MapManagerExceptionDBException(__FUNCTION__, "Impossible 1!", sqlite3_errmsg(dbOps->getConn()), rc));

		if (numDeleted == 1)
		{
			vector<__int64> MapVertexAffectedByWD;

			/*
			SELECTING vertices affected by deleted WD
			*/
			string sql = "SELECT VertexRowId FROM MapVertex_WD WHERE WDRowId = %s;";
			string sql1 = dbOps->completeSQL(sql.c_str(), to_string(wd_rowid).c_str());
			dbOps->prepareStmt(sql1.c_str());
			dbOps->acquireLock();
			int rc = sqlite3_step(dbOps->getStmt());
			dbOps->releaseLock();
			if (rc != SQLITE_ROW && rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertices affected by WordlDefiner being deleted from MapVertex_WD table!", sqlite3_errmsg(dbOps->getConn()), rc));
			while (rc == SQLITE_ROW)
			{
				MapVertexAffectedByWD.push_back(sqlite3_column_int64(dbOps->getStmt(), 0));

				dbOps->acquireLock();
				rc = sqlite3_step(dbOps->getStmt());
				dbOps->releaseLock();
				if (rc != SQLITE_ROW && rc != SQLITE_DONE)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiners affecting a Vertex from MapVertex_WD table!", sqlite3_errmsg(dbOps->getConn()), rc));
			}
			dbOps->finalizeStmt();

			/*
			DELETE rows associating vertices to deleted WD
			*/
			sql = "DELETE FROM MapVertex_WD WHERE WDRowId = %s;";
			sql1 = dbOps->completeSQL(sql.c_str(), to_string(wd_rowid).c_str());
			dbOps->prepareStmt(sql1.c_str());
			dbOps->acquireLock();
			rc = sqlite3_step(dbOps->getStmt());
			int numDeleted = sqlite3_changes(dbOps->getConn());
			dbOps->releaseLock();
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to update vertex altitude in MapVertex table!", sqlite3_errmsg(dbOps->getConn()), rc));
			dbOps->finalizeStmt();

			int numVerticesAffectedByWD = (int)MapVertexAffectedByWD.size();
			
			if (numDeleted != numVerticesAffectedByWD)
				throw(MapManagerExceptionDBException(__FUNCTION__, "Impossible 2!", sqlite3_errmsg(dbOps->getConn()), rc));

			/*
			* INSERT in table MapVertex_Mod vertices affected by WD deleted
			*/
			int idx = 0;
			int inserted = 0;
			if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing affected vertices to MapVertex_Mod Table: ");
			if (numVerticesAffectedByWD > 0)
			{
				string sql = "INSERT INTO MapVertex_Mod (VertexRowId) VALUES (?);";
				dbOps->prepareStmt(sql.c_str());

				for (idx = 0; idx < numVerticesAffectedByWD; idx++)
				{
					// insert vertices not affected by WD (to complete the squres)
					// affected vertices will be inserted in MapVertex_Mod table to be computed later
					rc = sqlite3_bind_int64(dbOps->getStmt(), 1, MapVertexAffectedByWD[idx]);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind MapVertex_Mod.VertexRowId failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					dbOps->acquireLock();
					rc = sqlite3_step(dbOps->getStmt());
					dbOps->releaseLock();
					if (rc == SQLITE_DONE)
						inserted++;
					if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					dbOps->resetStmt();

					if (debugMode() && fmod(idx, 1024 * 100) == 0)
					{
						string s = to_string(idx + 1);	s += " - Inserted ";	s += to_string(inserted);
						debugUtil.printVariablePartOfLine(s.c_str());
					}
				}

				dbOps->finalizeStmt();
			}
			if (debugMode())
			{
				string s = to_string(idx);	s += " - Inserted ";	s += to_string(inserted);
				debugUtil.printVariablePartOfLine(s.c_str());
			}
		}

		/*
		* Finalize DB operations
		*/
		if (endTransactionRequired)
		{
			dbOps->endTransaction();
			dbOps->reset();
		}

		return !(numDeleted == 0);
	}

	void DBSQLLite::updateAltitudeOfVertex(__int64 vertexRowid, float posY)
	{
		/*
		* Initialize DB operations
		*/
		DBSQLLiteOps* dbOps = NULL;
		DBSQLLiteOps temporarydbOps(dbFilePath());
		bool endTransactionRequired = true;
		if (m_dbOpsInternalTransaction.isTransactionOpened())
		{
			dbOps = &m_dbOpsInternalTransaction;
			endTransactionRequired = false;
		}
		else
		{
			temporarydbOps.init();
			temporarydbOps.beginTransaction();
			dbOps = &temporarydbOps;
		}

		string sql = "UPDATE MapVertex SET PosY = %s WHERE rowid = %s;";
		string sql1 = dbOps->completeSQL(sql.c_str(), to_string(posY).c_str(), to_string(vertexRowid).c_str());
		dbOps->prepareStmt(sql1.c_str());
		dbOps->acquireLock();
		int rc = sqlite3_step(dbOps->getStmt());
		dbOps->releaseLock();
		dbOps->finalizeStmt();
		if (rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to update vertex altitude in MapVertex table!", sqlite3_errmsg(dbOps->getConn()), rc));

		/*
		* Finalize DB operations
		*/
		if (endTransactionRequired)
		{
			dbOps->endTransaction();
			dbOps->reset();
		}
	}

	void DBSQLLite::clearVerticesMarkedForUpdate(void)
	{
		// WARNING: in case of error / rollback iterated vertices are lost anycase
		std::vector<__int64> iteratedModifiedVerticesMap = m_iteratedModifiedVerticesMap;
		m_iteratedModifiedVerticesMap.clear();

		/*
		* Initialize DB operations
		*/
		DBSQLLiteOps* dbOps = NULL;
		DBSQLLiteOps temporarydbOps(dbFilePath());
		bool endTransactionRequired = true;
		if (m_dbOpsInternalTransaction.isTransactionOpened())
		{
			dbOps = &m_dbOpsInternalTransaction;
			endTransactionRequired = false;
		}
		else
		{
			temporarydbOps.init();
			temporarydbOps.beginTransaction();
			dbOps = &temporarydbOps;
		}

		int numVertices = (int)iteratedModifiedVerticesMap.size();
		for (int idx = 0; idx < numVertices; idx++)
		{
			string sql = "DELETE FROM MapVertex_Mod WHERE VertexRowId = %s;";
			string sql1 = dbOps->completeSQL(sql.c_str(), to_string(iteratedModifiedVerticesMap[idx]).c_str());
			dbOps->prepareStmt(sql1.c_str());
			dbOps->acquireLock();
			int rc = sqlite3_step(dbOps->getStmt());
			dbOps->releaseLock();
			dbOps->finalizeStmt();
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to delete vertex from MapVertex_Mod table!", sqlite3_errmsg(dbOps->getConn()), rc));
		}

		/*
		* Finalize DB operations
		*/
		if (endTransactionRequired)
		{
			dbOps->endTransaction();
			dbOps->reset();
		}
	}


	void DBSQLLite::getVertex(__int64 vertexRowid, MapVertex& mapVertex)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT PosX, PosY, PosZ, Radius, Azimuth, Level FROM MapVertex WHERE rowid = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(vertexRowid).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertex with rowid from MapVertex table!", sqlite3_errmsg(dbOps.getConn()), rc));
		
		mapVertex.setInternalValues((float)sqlite3_column_double(dbOps.getStmt(), 0),	// PosX
									(float)sqlite3_column_double(dbOps.getStmt(), 1),	// PosY
									(float)sqlite3_column_double(dbOps.getStmt(), 2),	// PosZ
									(float)sqlite3_column_double(dbOps.getStmt(), 3),	// Radius
									(float)sqlite3_column_double(dbOps.getStmt(), 4),	// Azimuth
									sqlite3_column_int(dbOps.getStmt(), 5),				// Level
									vertexRowid);										// rowid
		dbOps.finalizeStmt();
	}

	bool DBSQLLite::getWD(__int64 wdRowid, WorldDefiner& wd)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT PosX, PosZ, Level, Type, radius, azimuth, azimuthDegree, Strength, AOE, FunctionType FROM WorldDefiner WHERE rowid = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(wdRowid).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiner!", sqlite3_errmsg(dbOps.getConn()), rc));
		if (rc == SQLITE_ROW)
		{
			wd.setInternalValues((float)sqlite3_column_double(dbOps.getStmt(), 0),	// PosX
				(float)sqlite3_column_double(dbOps.getStmt(), 1),					// PosZ
				sqlite3_column_int(dbOps.getStmt(), 2),								// level
				(WDType)sqlite3_column_int(dbOps.getStmt(), 3),						// Type
				(float)sqlite3_column_double(dbOps.getStmt(), 4),					// radius
				(float)sqlite3_column_double(dbOps.getStmt(), 5),					// azimuth
				(float)sqlite3_column_double(dbOps.getStmt(), 6),					// azimuthDegree
				(float)sqlite3_column_double(dbOps.getStmt(), 7),					// Strength
				(float)sqlite3_column_double(dbOps.getStmt(), 8),					// AOE
				(WDFunctionType)sqlite3_column_int(dbOps.getStmt(), 9),				// FunctionType
				wdRowid);															// rowid
		}
		dbOps.finalizeStmt();
		
		if (rc == SQLITE_ROW)
			return true;
		else
			return false;
	}

	bool DBSQLLite::getWD(float posX, float posZ, int level, WDType type, WorldDefiner& wd)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT rowid FROM WorldDefiner WHERE PosX = %s AND PosZ=%s AND Level = %s AND Type = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(posX).c_str(), to_string(posZ).c_str(), to_string(level).c_str(), to_string((int)type).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiner!", sqlite3_errmsg(dbOps.getConn()), rc));
		if (rc == SQLITE_ROW)
		{
			__int64 WDRowId = sqlite3_column_int64(dbOps.getStmt(), 0);
			bool bFound = getWD(WDRowId, wd);
			if (!bFound)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiner!", sqlite3_errmsg(dbOps.getConn()), rc));
		}
		dbOps.finalizeStmt();

		if (rc == SQLITE_ROW)
			return true;
		else
			return false;
	}
	
	void DBSQLLite::getWDRowIdForVertex(__int64 vertexRowid, vector<__int64>& MapWDRowId)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT WDRowId FROM MapVertex_WD WHERE VertexRowId = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(vertexRowid).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiners affecting a Vertex from MapVertex_WD table!", sqlite3_errmsg(dbOps.getConn()), rc));
		while (rc == SQLITE_ROW)
		{
			MapWDRowId.push_back(sqlite3_column_int64(dbOps.getStmt(), 0));

			dbOps.acquireLock();
			rc = sqlite3_step(dbOps.getStmt());
			dbOps.releaseLock();
			if (rc != SQLITE_ROW && rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiners affecting a Vertex from MapVertex_WD table!", sqlite3_errmsg(dbOps.getConn()), rc));
		}
		dbOps.finalizeStmt();
	}

	bool DBSQLLite::getFirstModfiedVertex(MapVertex& mapVertex, std::vector<WorldDefiner>& wdMap)
	{
		if (!m_dbOpsIterationModifiedVertices.isInitialized())
			m_dbOpsIterationModifiedVertices.init(dbFilePath());

		if (m_dbOpsIterationModifiedVertices.getStmt() != NULL)
			m_dbOpsIterationModifiedVertices.finalizeStmt();

		m_iteratedModifiedVerticesMap.clear();
		
		string sql = "SELECT VertexRowId FROM MapVertex_Mod;";
		m_dbOpsIterationModifiedVertices.prepareStmt(sql.c_str());
		m_dbOpsIterationModifiedVertices.acquireLock();
		int rc = sqlite3_step(m_dbOpsIterationModifiedVertices.getStmt());
		m_dbOpsIterationModifiedVertices.releaseLock();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read modified vertex!", sqlite3_errmsg(m_dbOpsIterationModifiedVertices.getConn()), rc));
		
		if (rc == SQLITE_DONE)
		{
			m_dbOpsIterationModifiedVertices.finalizeStmt();
			return false;
		}
		else
		{
			sqlite3_int64 VertexRowId = sqlite3_column_int64(m_dbOpsIterationModifiedVertices.getStmt(), 0);
			getVertex(VertexRowId, mapVertex);
			
			vector<sqlite3_int64> MapWDRowId;
			getWDRowIdForVertex(VertexRowId, MapWDRowId);
			int numAffectingWDRowid = (int)MapWDRowId.size();
			for (int idx = 0; idx < numAffectingWDRowid; idx++)
			{
				WorldDefiner wd;
				bool bFound = getWD(MapWDRowId[idx], wd);
				if (!bFound)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiner!", sqlite3_errmsg(m_dbOpsIterationModifiedVertices.getConn()), rc));
				wdMap.push_back(wd);
			}

			m_iteratedModifiedVerticesMap.push_back(VertexRowId);
			
			return true;
		}
	}
	
	bool DBSQLLite::getNextModfiedVertex(MapVertex& mapVertex, std::vector<WorldDefiner>& wdMap)
	{
		if (!m_dbOpsIterationModifiedVertices.isInitialized())
			throw(MapManagerExceptionDBException(__FUNCTION__, "getFirstModfiedVertex not executed!"));

		if (m_dbOpsIterationModifiedVertices.getStmt() == NULL)
			throw(MapManagerExceptionDBException(__FUNCTION__, "getFirstModfiedVertex not executed!"));

		m_dbOpsIterationModifiedVertices.acquireLock();
		int rc = sqlite3_step(m_dbOpsIterationModifiedVertices.getStmt());
		m_dbOpsIterationModifiedVertices.releaseLock();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read modified vertex!", sqlite3_errmsg(m_dbOpsIterationModifiedVertices.getConn()), rc));

		if (rc == SQLITE_DONE)
		{
			m_dbOpsIterationModifiedVertices.finalizeStmt();
			return false;
		}
		else
		{
			sqlite3_int64 VertexRowId = sqlite3_column_int64(m_dbOpsIterationModifiedVertices.getStmt(), 0);
			getVertex(VertexRowId, mapVertex);

			vector<sqlite3_int64> MapWDRowId;
			getWDRowIdForVertex(VertexRowId, MapWDRowId);
			int numAffectingWDRowid = (int)MapWDRowId.size();
			for (int idx = 0; idx < numAffectingWDRowid; idx++)
			{
				WorldDefiner wd;
				bool bFound = getWD(MapWDRowId[idx], wd);
				if (!bFound)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiner!", sqlite3_errmsg(m_dbOpsIterationModifiedVertices.getConn()), rc));
				wdMap.push_back(wd);
			}

			m_iteratedModifiedVerticesMap.push_back(VertexRowId);

			return true;
		}
	}

	void DBSQLLite::finalizeDB(void)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.finalizeDB();
	}
}