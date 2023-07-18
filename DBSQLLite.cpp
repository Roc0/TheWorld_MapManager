#include "pch.h"

#include "json/json.h"
#include <iostream>
#include <fstream>
#include <string>

#include "MapManager_Utils.h"
#include "DBSQLLite.h"
#include "Profiler.h"

using namespace std;

namespace TheWorld_MapManager
{
	DBSQLLiteConn DBSQLLiteOps::s_conn;
	DBThreadContextPool DBSQLLiteOps::s_connPool;
	enum class DBSQLLiteOps::ConnectionType DBSQLLiteOps::s_connType = DBSQLLiteOps::ConnectionType::SingleConn;
	std::recursive_mutex DBSQLLiteOps::s_DBAccessMtx;
	std::map<std::string, std::recursive_mutex*> DBSQLLiteConn::s_mutexPool;
	std::recursive_mutex DBSQLLiteConn::s_mutexPoolMtx;
		
	DBSQLLite::DBSQLLite(DBType _dbt, const char* _dataPath, bool consoelDebugMode) : SQLInterface(_dbt, _dataPath, consoelDebugMode)
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

	std::string DBSQLLite::readParam(std::string paranName)
	{
		std::string paramValue = "";
		
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		std::string sql = "SELECT ParamValue FROM Params WHERE ParamName = '" + paranName + "';";
		dbOps.prepareStmt(sql.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc == SQLITE_ROW)
			paramValue = (char *)sqlite3_column_text(dbOps.getStmt(), 0);
		dbOps.finalizeStmt();

		return paramValue;
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

	__int64 DBSQLLite::addWDAndVertices(WorldDefiner* pWD, vector<GridVertex>& vectGridVertices)
	{
		vector<sqlite3_int64> GridVertexRowId;
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());

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

		sqlite3_int64 WDRowID = -1;

		if (pWD)
		{
			/*
			* INSERT in table WorldDefiner
			*/
			string sql = "INSERT INTO WorldDefiner (PosX, PosZ, radius, azimuth, azimuthDegree, Level, Type, Strength, AOE, FunctionType) VALUES (" 
				+ std::to_string(pWD->getPosX())
				+ "," + std::to_string(pWD->getPosZ())
				+ "," + std::to_string(pWD->getRadius())
				+ "," + std::to_string(pWD->getAzimuth())
				+ "," + std::to_string(pWD->getAzimuthDegree())
				+ "," + std::to_string(pWD->getLevel())
				+ "," + std::to_string((int)pWD->getType())
				+ "," + std::to_string(pWD->getStrength())
				+ "," + std::to_string(pWD->getAOE())
				+ "," + std::to_string((int)pWD->getFunctionType())
				+ ");";
			dbOps->acquireLock();
			int rc = sqlite3_exec(dbOps->getConn(), sql.c_str(), NULL, NULL, NULL);
			WDRowID = sqlite3_last_insert_rowid(dbOps->getConn());
			dbOps->releaseLock();
			if (rc != SQLITE_OK && rc != SQLITE_CONSTRAINT)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite insert World Definer failed!", dbOps->errMsg(), rc));
			if (rc == SQLITE_CONSTRAINT)
				return -1;
			//throw(MapManagerExceptionDuplicate(__FUNCTION__, "DB SQLite World Definer duplicate!"));
		}

		/*
		* INSERT in table GridVertex
		*/
		int numVertices = (int)vectGridVertices.size();
		int idx = 0;
		int affectedByWD = 0;
		int notAffectedByWD = 0;
		int inserted = 0;
		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing vertices to GridVertex Table: ");
		if (numVertices > 0)
		{
			string sql = "INSERT INTO GridVertex (PosX, PosZ, Radius, Azimuth, Level, InitialAltitude, PosY) VALUES (?, ?, ?, ?, ?, ?, ?);";
			dbOps->prepareStmt(sql.c_str());

			for (idx = 0; idx < numVertices; idx++)
			{
				bool vertexAffectedByWD = false;
				if (pWD && getDistance(pWD->getPosX(), pWD->getPosZ(), vectGridVertices[idx].posX(), vectGridVertices[idx].posZ()) <= pWD->getAOE())
				{
					vertexAffectedByWD = true;
					affectedByWD++;
				}
				else
					notAffectedByWD++;

				int rc = sqlite3_bind_double(dbOps->getStmt(), 1, vectGridVertices[idx].posX());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.PosX failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				
				rc = sqlite3_bind_double(dbOps->getStmt(), 2, vectGridVertices[idx].posZ());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.PosZ failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				
				rc = sqlite3_bind_double(dbOps->getStmt(), 3, vectGridVertices[idx].radius());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.radius failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				
				rc = sqlite3_bind_double(dbOps->getStmt(), 4, vectGridVertices[idx].azimuth());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.azimuth failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				
				rc = sqlite3_bind_int(dbOps->getStmt(), 5, vectGridVertices[idx].level());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.level failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				
				rc = sqlite3_bind_double(dbOps->getStmt(), 6, vectGridVertices[idx].initialAltitude());
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.initialAltitude failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				
				rc = sqlite3_bind_double(dbOps->getStmt(), 7, vectGridVertices[idx].initialAltitude());	// Not affected vertices have 0.0 altitude, for affected vertices altitude will be computed later as they are inserted in GridVertex_Mod table
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.PosY failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->acquireLock();
				rc = sqlite3_step(dbOps->getStmt());
				sqlite3_int64 VertexRowId = sqlite3_last_insert_rowid(dbOps->getConn());
				dbOps->releaseLock();
				if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				if (rc == SQLITE_CONSTRAINT && vectGridVertices[idx].initialAltitude() != 0.0)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB insert vertex failed (duplicate Vertex in imported mesh)", sqlite3_errmsg(dbOps->getConn()), rc));
				if (rc == SQLITE_DONE)
					inserted++;
				
				dbOps->resetStmt();

				if (vertexAffectedByWD)
				{
					if (rc == SQLITE_CONSTRAINT)
					{
						// The Vertex was present in GridVertex table so we have to acquire his rowid
						DBSQLLiteOps dbOps1(dbFilePath());
						dbOps1.init();
						string sql = "SELECT rowid FROM GridVertex WHERE PosX = %s AND PosZ = %s AND Level = %s;";
						string sql1 = dbOps1.completeSQL(sql.c_str(), to_string(vectGridVertices[idx].posX()).c_str(), to_string(vectGridVertices[idx].posZ()).c_str(), to_string(vectGridVertices[idx].level()).c_str());
						dbOps1.prepareStmt(sql1.c_str());
						dbOps1.acquireLock();
						rc = sqlite3_step(dbOps1.getStmt());
						dbOps1.releaseLock();
						if (rc != SQLITE_ROW)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read rowid vertex from GridVertex table!", sqlite3_errmsg(dbOps1.getConn()), rc));
						VertexRowId = sqlite3_column_int64(dbOps1.getStmt(), 0);
						dbOps1.finalizeStmt();
					}

					GridVertexRowId.push_back(VertexRowId);
				}

				if (consoleDebugMode() && fmod(idx, 1024 * 100) == 0)
				{
					string s = to_string(idx + 1);	s += " - Affected ";	s += to_string(affectedByWD);	s += " - Not affected ";	s += to_string(notAffectedByWD);	s += " - Inserted ";	s += to_string(inserted);
					_consoleDebugUtil.printVariablePartOfLine(s.c_str());
				}
			}

			dbOps->finalizeStmt();
		}
		if (consoleDebugMode())
		{
			string s = to_string(idx);	s += " - Affected ";	s += to_string(affectedByWD);	s += " - Not affected ";	s += to_string(notAffectedByWD);	s += " - Inserted ";	s += to_string(inserted);
			_consoleDebugUtil.printVariablePartOfLine(s.c_str());
		}

		/*
		* INSERT in table GridVertex_WD affecting WD
		*/
		int numAffectedVertices = (int)GridVertexRowId.size();
		idx = 0;
		inserted = 0;
		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing affected vertices to GridVertex_WD Table: ");
		if (numAffectedVertices > 0)
		{
			if (WDRowID == -1)
				throw(MapManagerExceptionDBException(__FUNCTION__, "WD RowId not set (impossible)!", sqlite3_errmsg(dbOps->getConn())));

			string sql = "INSERT INTO GridVertex_WD (VertexRowId, WDRowId) VALUES (?, ?);";
			dbOps->prepareStmt(sql.c_str());

			for (idx = 0; idx < numAffectedVertices; idx++)
			{
				int rc = sqlite3_bind_int64(dbOps->getStmt(), 1, GridVertexRowId[idx]);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB GridVertex_WD.VertexRowId PosX failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				
				rc = sqlite3_bind_int64(dbOps->getStmt(), 2, WDRowID);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB GridVertex_WD.VertexRowId PosX failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->acquireLock();
				rc = sqlite3_step(dbOps->getStmt());
				dbOps->releaseLock();
				if (rc == SQLITE_DONE)
					inserted++;
				else
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->resetStmt();

				if (consoleDebugMode() && fmod(idx, 1024 * 100) == 0)
				{
					string s = to_string(idx + 1);	s += " - Inserted ";	s += to_string(inserted);
					_consoleDebugUtil.printVariablePartOfLine(s.c_str());
				}
			}

			dbOps->finalizeStmt();
		}
		if (consoleDebugMode())
		{
			string s = to_string(idx);	s += " - Inserted ";	s += to_string(inserted);
			_consoleDebugUtil.printVariablePartOfLine(s.c_str());
		}

		/*
		* INSERT in table GridVertex_Mod vertices affected by WD inserted
		*/
		idx = 0;
		inserted = 0;
		if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing affected vertices to GridVertex_Mod Table: ");
		if (numAffectedVertices > 0)
		{
			if (WDRowID == -1)
				throw(MapManagerExceptionDBException(__FUNCTION__, "WD RowId not set (impossible)!", sqlite3_errmsg(dbOps->getConn())));

			string sql = "INSERT INTO GridVertex_Mod (VertexRowId) VALUES (?);";
			dbOps->prepareStmt(sql.c_str());

			for (idx = 0; idx < numAffectedVertices; idx++)
			{
				// insert vertices not affected by WD (to complete the squres)
				// affected vertices will be inserted in GridVertex_Mod table to be computed later
				int rc = sqlite3_bind_int64(dbOps->getStmt(), 1, GridVertexRowId[idx]);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex_Mod.VertexRowId failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->acquireLock();
				rc = sqlite3_step(dbOps->getStmt());
				dbOps->releaseLock();
				if (rc == SQLITE_DONE)
					inserted++;
				if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				dbOps->resetStmt();

				if (consoleDebugMode() && fmod(idx, 1024 * 100) == 0)
				{
					string s = to_string(idx + 1);	s += " - Inserted ";	s += to_string(inserted);
					_consoleDebugUtil.printVariablePartOfLine(s.c_str());
				}
			}

			dbOps->finalizeStmt();
		}
		if (consoleDebugMode())
		{
			string s = to_string(idx);	s += " - Inserted ";	s += to_string(inserted);
			_consoleDebugUtil.printVariablePartOfLine(s.c_str());
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
		consoleDebugUtil _consoleDebugUtil(consoleDebugMode());

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
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to delete from WorldDefiner table!", sqlite3_errmsg(dbOps->getConn()), rc));
		dbOps->finalizeStmt();
		
		if (numDeleted > 1)
			throw(MapManagerExceptionDBException(__FUNCTION__, "Impossible 1!", sqlite3_errmsg(dbOps->getConn()), rc));

		if (numDeleted == 1)
		{
			vector<__int64> GridVertexAffectedByWD;

			/*
			SELECTING vertices affected by deleted WD
			*/
			string sql = "SELECT VertexRowId FROM GridVertex_WD WHERE WDRowId = %s;";
			string sql1 = dbOps->completeSQL(sql.c_str(), to_string(wd_rowid).c_str());
			dbOps->prepareStmt(sql1.c_str());
			dbOps->acquireLock();
			int rc = sqlite3_step(dbOps->getStmt());
			dbOps->releaseLock();
			if (rc != SQLITE_ROW && rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertices affected by WordlDefiner being deleted from GridVertex_WD table!", sqlite3_errmsg(dbOps->getConn()), rc));
			while (rc == SQLITE_ROW)
			{
				GridVertexAffectedByWD.push_back(sqlite3_column_int64(dbOps->getStmt(), 0));

				dbOps->acquireLock();
				rc = sqlite3_step(dbOps->getStmt());
				dbOps->releaseLock();
				if (rc != SQLITE_ROW && rc != SQLITE_DONE)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiners affecting a Vertex from GridVertex_WD table!", sqlite3_errmsg(dbOps->getConn()), rc));
			}
			dbOps->finalizeStmt();

			/*
			DELETE rows associating vertices to deleted WD
			*/
			sql = "DELETE FROM GridVertex_WD WHERE WDRowId = %s;";
			sql1 = dbOps->completeSQL(sql.c_str(), to_string(wd_rowid).c_str());
			dbOps->prepareStmt(sql1.c_str());
			dbOps->acquireLock();
			rc = sqlite3_step(dbOps->getStmt());
			int numDeleted = sqlite3_changes(dbOps->getConn());
			dbOps->releaseLock();
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to delete from GridVertex_WD table!", sqlite3_errmsg(dbOps->getConn()), rc));
			dbOps->finalizeStmt();

			int numVerticesAffectedByWD = (int)GridVertexAffectedByWD.size();
			
			if (numDeleted != numVerticesAffectedByWD)
				throw(MapManagerExceptionDBException(__FUNCTION__, "Impossible 2!", sqlite3_errmsg(dbOps->getConn()), rc));

			/*
			* INSERT in table GridVertex_Mod vertices affected by WD deleted
			*/
			int idx = 0;
			int inserted = 0;
			if (consoleDebugMode()) _consoleDebugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing affected vertices to GridVertex_Mod Table: ");
			if (numVerticesAffectedByWD > 0)
			{
				string sql = "INSERT INTO GridVertex_Mod (VertexRowId) VALUES (?);";
				dbOps->prepareStmt(sql.c_str());

				for (idx = 0; idx < numVerticesAffectedByWD; idx++)
				{
					// insert vertices not affected by WD (to complete the squres)
					// affected vertices will be inserted in GridVertex_Mod table to be computed later
					rc = sqlite3_bind_int64(dbOps->getStmt(), 1, GridVertexAffectedByWD[idx]);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex_Mod.VertexRowId failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					dbOps->acquireLock();
					rc = sqlite3_step(dbOps->getStmt());
					dbOps->releaseLock();
					if (rc == SQLITE_DONE)
						inserted++;
					if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					dbOps->resetStmt();

					if (consoleDebugMode() && fmod(idx, 1024 * 100) == 0)
					{
						string s = to_string(idx + 1);	s += " - Inserted ";	s += to_string(inserted);
						_consoleDebugUtil.printVariablePartOfLine(s.c_str());
					}
				}

				dbOps->finalizeStmt();
			}
			if (consoleDebugMode())
			{
				string s = to_string(idx);	s += " - Inserted ";	s += to_string(inserted);
				_consoleDebugUtil.printVariablePartOfLine(s.c_str());
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

		string sql = "UPDATE GridVertex SET PosY = %s WHERE rowid = %s;";
		string sql1 = dbOps->completeSQL(sql.c_str(), to_string(posY).c_str(), to_string(vertexRowid).c_str());
		dbOps->prepareStmt(sql1.c_str());
		dbOps->acquireLock();
		int rc = sqlite3_step(dbOps->getStmt());
		dbOps->releaseLock();
		dbOps->finalizeStmt();
		if (rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to update vertex altitude in GridVertex table!", sqlite3_errmsg(dbOps->getConn()), rc));

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
			string sql = "DELETE FROM GridVertex_Mod WHERE VertexRowId = %s;";
			string sql1 = dbOps->completeSQL(sql.c_str(), to_string(iteratedModifiedVerticesMap[idx]).c_str());
			dbOps->prepareStmt(sql1.c_str());
			dbOps->acquireLock();
			int rc = sqlite3_step(dbOps->getStmt());
			dbOps->releaseLock();
			dbOps->finalizeStmt();
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to delete vertex from GridVertex_Mod table!", sqlite3_errmsg(dbOps->getConn()), rc));
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


	void DBSQLLite::getVertex(__int64 vertexRowid, GridVertex& gridVertex, int level)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT PosX, PosY, PosZ, Radius, Azimuth, Level, InitialAltitude,"
					 "NormX, NormY, NormZ, ColorR, ColorG, ColorB, ColorA, LowElevationTexAmount, HighElevationTexAmount, DirtTexAmount, RocksTexAmount, GlobalMapR, GlobalMapG, GlobalMapB "
					 "FROM GridVertex WHERE rowid = % s AND level = % s; ";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(vertexRowid).c_str(), to_string(level).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertex with rowid from GridVertex table!", sqlite3_errmsg(dbOps.getConn()), rc));
		
		gridVertex.setInternalValues((float)sqlite3_column_double(dbOps.getStmt(), 0),	// PosX
			(float)sqlite3_column_double(dbOps.getStmt(), 1),							// PosY
			(float)sqlite3_column_double(dbOps.getStmt(), 2),							// PosZ
			(float)sqlite3_column_double(dbOps.getStmt(), 3),							// Radius
			(float)sqlite3_column_double(dbOps.getStmt(), 4),							// Azimuth
			sqlite3_column_int(dbOps.getStmt(), 5),										// Level
			(float)sqlite3_column_int(dbOps.getStmt(), 6),								// InitialAltitude
			vertexRowid,																// rowid
			sqlite3_column_type(dbOps.getStmt(), 7) == SQLITE_NULL ? 0 : (float)sqlite3_column_double(dbOps.getStmt(), 7),		// NormX
			sqlite3_column_type(dbOps.getStmt(), 8) == SQLITE_NULL ? 0 : (float)sqlite3_column_double(dbOps.getStmt(), 8),		// NormY
			sqlite3_column_type(dbOps.getStmt(), 9) == SQLITE_NULL ? 0 : (float)sqlite3_column_double(dbOps.getStmt(), 9),		// NormZ
			sqlite3_column_type(dbOps.getStmt(), 10) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 10),	// ColorR
			sqlite3_column_type(dbOps.getStmt(), 11) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 11),	// ColorG
			sqlite3_column_type(dbOps.getStmt(), 12) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 12),	// ColorB
			sqlite3_column_type(dbOps.getStmt(), 13) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 13),	// ColorA
			sqlite3_column_type(dbOps.getStmt(), 14) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 14),	// LowElevationTexAmount
			sqlite3_column_type(dbOps.getStmt(), 15) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 15),	// HighElevationTexAmount
			sqlite3_column_type(dbOps.getStmt(), 16) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 16),	// DirtTexAmount
			sqlite3_column_type(dbOps.getStmt(), 17) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 17),	// RocksTexAmount
			sqlite3_column_type(dbOps.getStmt(), 18) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 18),	// GlobalMapR
			sqlite3_column_type(dbOps.getStmt(), 19) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 19),	// GlobalMapG
			sqlite3_column_type(dbOps.getStmt(), 20) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 20));	// GlobalMapB

		dbOps.finalizeStmt();
	}

	void DBSQLLite::getVertex(GridVertex& gridVertex)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT PosY, Radius, Azimuth, InitialAltitude, VertexRowId,"
			"NormX, NormY, NormZ, ColorR, ColorG, ColorB, ColorA, LowElevationTexAmount, HighElevationTexAmount, DirtTexAmount, RocksTexAmount, GlobalMapR, GlobalMapG, GlobalMapB "
			"FROM GridVertex WHERE PosX = %s AND PosZ = %s AND level = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(gridVertex.posX()).c_str(), to_string(gridVertex.posZ()).c_str(), to_string(gridVertex.level()).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertex with posX, posZ, level from GridVertex table!", sqlite3_errmsg(dbOps.getConn()), rc));

		gridVertex.setInternalValues(gridVertex.posX(),
			(float)sqlite3_column_double(dbOps.getStmt(), 0),	// PosY
			gridVertex.posZ(),
			(float)sqlite3_column_double(dbOps.getStmt(), 1),	// Radius
			(float)sqlite3_column_double(dbOps.getStmt(), 2),	// Azimuth
			gridVertex.level(),									// Level
			(float)sqlite3_column_int(dbOps.getStmt(), 3),		// InitialAltitude
			(__int64)sqlite3_column_int64(dbOps.getStmt(), 4),	// rowid
			sqlite3_column_type(dbOps.getStmt(), 5) == SQLITE_NULL ? 0 : (float)sqlite3_column_double(dbOps.getStmt(), 5),		// NormX
			sqlite3_column_type(dbOps.getStmt(), 6) == SQLITE_NULL ? 0 : (float)sqlite3_column_double(dbOps.getStmt(), 6),		// NormY
			sqlite3_column_type(dbOps.getStmt(), 7) == SQLITE_NULL ? 0 : (float)sqlite3_column_double(dbOps.getStmt(), 7),		// NormZ
			sqlite3_column_type(dbOps.getStmt(), 8) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 8),	// ColorR
			sqlite3_column_type(dbOps.getStmt(), 9) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 9),	// ColorG
			sqlite3_column_type(dbOps.getStmt(), 10) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 10),	// ColorB
			sqlite3_column_type(dbOps.getStmt(), 11) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 11),	// ColorA
			sqlite3_column_type(dbOps.getStmt(), 12) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 12),	// LowElevationTexAmount
			sqlite3_column_type(dbOps.getStmt(), 13) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 13),	// HighElevationTexAmount
			sqlite3_column_type(dbOps.getStmt(), 14) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 14),	// DirtTexAmount
			sqlite3_column_type(dbOps.getStmt(), 15) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 15),	// RocksTexAmount
			sqlite3_column_type(dbOps.getStmt(), 16) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 16),	// GlobalMapR
			sqlite3_column_type(dbOps.getStmt(), 17) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 17),	// GlobalMapG
			sqlite3_column_type(dbOps.getStmt(), 18) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 18));	// GlobalMapB
			dbOps.finalizeStmt();
	}

	size_t DBSQLLite::eraseVertices(float minX, float maxX, float minZ, float maxZ, int level)
	{
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

		string sql1 = "DELETE FROM GridVertex WHERE PosX >= %s AND PosX <= %s AND PosZ >= %s AND PosZ <= %s AND level = %s;";
		string sql = dbOps->completeSQL(sql1.c_str(), to_string(minX).c_str(), to_string(maxX).c_str(), to_string(minZ).c_str(), to_string(maxZ).c_str(), to_string(level).c_str());
		dbOps->prepareStmt(sql.c_str());
		int rc = dbOps->execStmt();
		size_t numDeleted = sqlite3_changes(dbOps->getConn());
		if (rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite error erasing vertices!", sqlite3_errmsg(dbOps->getConn()), rc));
		dbOps->finalizeStmt();

		//std::vector<GridVertex> vectGridVertex;
		//getVertices(minX, maxX, minZ, maxZ, vectGridVertex, level);
		//if (vectGridVertex.size() > 0)
		//{
		//}

				/*
		* Finalize DB operations
		*/
		if (endTransactionRequired)
		{
			dbOps->endTransaction();
			dbOps->reset();
		}

		return numDeleted;
	}
		
	void DBSQLLite::getVertices(float minX, float maxX, float minZ, float maxZ, vector<GridVertex>& vectGridVertex, int level)
	{
		vectGridVertex.clear();

		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT PosX, PosY, PosZ, Radius, Azimuth, Level, InitialAltitude, VertexRowId,"
			"NormX, NormY, NormZ, ColorR, ColorG, ColorB, ColorA, LowElevationTexAmount, HighElevationTexAmount, DirtTexAmount, RocksTexAmount, GlobalMapR, GlobalMapG, GlobalMapB "
			"FROM GridVertex WHERE PosX >= %s AND PosX <= %s AND PosZ >= %s AND PosZ <= %s AND level = %s ORDER BY PosZ, PosX;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(minX).c_str(), to_string(maxX).c_str(), to_string(minZ).c_str(), to_string(maxZ).c_str(), to_string(level).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertex from GridVertex table!", sqlite3_errmsg(dbOps.getConn()), rc));

		while (rc == SQLITE_ROW)
		{
			//Sleep(0);
			GridVertex gridVertex;
			gridVertex.setInternalValues((float)sqlite3_column_double(dbOps.getStmt(), 0),	// PosX
				(float)sqlite3_column_double(dbOps.getStmt(), 1),							// PosY
				(float)sqlite3_column_double(dbOps.getStmt(), 2),							// PosZ
				(float)sqlite3_column_double(dbOps.getStmt(), 3),							// Radius
				(float)sqlite3_column_double(dbOps.getStmt(), 4),							// Azimuth
				sqlite3_column_int(dbOps.getStmt(), 5),										// Level
				(float)sqlite3_column_int(dbOps.getStmt(), 6),								// InitialAltitude
				(__int64)sqlite3_column_int64(dbOps.getStmt(), 7),							// rowid
				sqlite3_column_type(dbOps.getStmt(), 8) == SQLITE_NULL ? 0 : (float)sqlite3_column_double(dbOps.getStmt(), 8),		// NormX
				sqlite3_column_type(dbOps.getStmt(), 9) == SQLITE_NULL ? 0 : (float)sqlite3_column_double(dbOps.getStmt(), 9),		// NormY
				sqlite3_column_type(dbOps.getStmt(), 10) == SQLITE_NULL ? 0 : (float)sqlite3_column_double(dbOps.getStmt(), 10),		// NormZ
				sqlite3_column_type(dbOps.getStmt(), 11) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 11),	// ColorR
				sqlite3_column_type(dbOps.getStmt(), 12) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 12),	// ColorG
				sqlite3_column_type(dbOps.getStmt(), 13) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 13),	// ColorB
				sqlite3_column_type(dbOps.getStmt(), 14) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 14),	// ColorA
				sqlite3_column_type(dbOps.getStmt(), 15) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 15),	// LowElevationTexAmount
				sqlite3_column_type(dbOps.getStmt(), 16) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 16),	// HighElevationTexAmount
				sqlite3_column_type(dbOps.getStmt(), 17) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 17),	// DirtTexAmount
				sqlite3_column_type(dbOps.getStmt(), 18) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 18),	// RocksTexAmount
				sqlite3_column_type(dbOps.getStmt(), 19) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 19),	// GlobalMapR
				sqlite3_column_type(dbOps.getStmt(), 20) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 20),	// GlobalMapG
				sqlite3_column_type(dbOps.getStmt(), 21) == SQLITE_NULL ? -1 : sqlite3_column_int(dbOps.getStmt(), 21));	// GlobalMapB

			vectGridVertex.push_back(gridVertex);

			dbOps.acquireLock();
			rc = sqlite3_step(dbOps.getStmt());
			dbOps.releaseLock();
			if (rc != SQLITE_ROW && rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertex from GridVertex table!", sqlite3_errmsg(dbOps.getConn()), rc));
		}
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
	
	void DBSQLLite::getWDRowIdForVertex(__int64 vertexRowid, vector<__int64>& vectWDRowId)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT WDRowId FROM GridVertex_WD WHERE VertexRowId = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(vertexRowid).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiners affecting a Vertex from GridVertex_WD table!", sqlite3_errmsg(dbOps.getConn()), rc));
		while (rc == SQLITE_ROW)
		{
			vectWDRowId.push_back(sqlite3_column_int64(dbOps.getStmt(), 0));

			dbOps.acquireLock();
			rc = sqlite3_step(dbOps.getStmt());
			dbOps.releaseLock();
			if (rc != SQLITE_ROW && rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiners affecting a Vertex from GridVertex_WD table!", sqlite3_errmsg(dbOps.getConn()), rc));
		}
		dbOps.finalizeStmt();
	}

	bool DBSQLLite::getFirstModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD)
	{
		if (!m_dbOpsIterationModifiedVertices.isInitialized())
			m_dbOpsIterationModifiedVertices.init(dbFilePath());

		if (m_dbOpsIterationModifiedVertices.getStmt() != NULL)
			m_dbOpsIterationModifiedVertices.finalizeStmt();

		m_iteratedModifiedVerticesMap.clear();
		
		string sql = "SELECT VertexRowId FROM GridVertex_Mod;";
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
			getVertex(VertexRowId, gridVertex);
			
			vector<sqlite3_int64> vectWDRowId;
			getWDRowIdForVertex(VertexRowId, vectWDRowId);
			int numAffectingWDRowid = (int)vectWDRowId.size();
			for (int idx = 0; idx < numAffectingWDRowid; idx++)
			{
				WorldDefiner wd;
				bool bFound = getWD(vectWDRowId[idx], wd);
				if (!bFound)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiner!", sqlite3_errmsg(m_dbOpsIterationModifiedVertices.getConn()), rc));
				vectWD.push_back(wd);
			}

			m_iteratedModifiedVerticesMap.push_back(VertexRowId);
			
			return true;
		}
	}
	
	bool DBSQLLite::getNextModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD)
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
			getVertex(VertexRowId, gridVertex);

			vector<sqlite3_int64> vectWDRowId;
			getWDRowIdForVertex(VertexRowId, vectWDRowId);
			int numAffectingWDRowid = (int)vectWDRowId.size();
			for (int idx = 0; idx < numAffectingWDRowid; idx++)
			{
				WorldDefiner wd;
				bool bFound = getWD(vectWDRowId[idx], wd);
				if (!bFound)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read WorldDefiner!", sqlite3_errmsg(m_dbOpsIterationModifiedVertices.getConn()), rc));
				vectWD.push_back(wd);
			}

			m_iteratedModifiedVerticesMap.push_back(VertexRowId);

			return true;
		}
	}

	std::string DBSQLLite::getQuadrantHash(float gridStep, size_t vertexPerSize, size_t level, float posX, float posZ, enum class SQLInterface::QuadrantStatus& status)
	{
		std::string h = "";
		status = SQLInterface::QuadrantStatus::NotSet;

		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT Hash, Status FROM Quadrant WHERE GridStepInWU = %s AND VertexPerSize = %s AND Level = %s AND PosXStart = %s AND PosZStart = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(gridStep).c_str(), to_string(vertexPerSize).c_str(), to_string(level).c_str(), to_string(posX).c_str(), to_string(posZ).c_str());
		dbOps.prepareStmt(sql1.c_str());
		int rc = dbOps.execStmt();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read Quadrant Hash!", sqlite3_errmsg(dbOps.getConn()), rc));
		if (rc == SQLITE_ROW)
		{
			h = std::string((char*)sqlite3_column_blob(dbOps.getStmt(), 0), sqlite3_column_bytes(dbOps.getStmt(), 0));
			std::string s = (char*)sqlite3_column_text(dbOps.getStmt(), 1);
			if (s == "C")
				status = SQLInterface::QuadrantStatus::Complete;
			else if (s == "L")
				status = SQLInterface::QuadrantStatus::Loading;
			else if (s == "E")
				status = SQLInterface::QuadrantStatus::Empty;
			else
				throw(MapManagerExceptionDBException(__FUNCTION__, std::string(std::string("DB SQLite unexpected status") + s + "!").c_str(), sqlite3_errmsg(dbOps.getConn()), rc));
		}
		dbOps.finalizeStmt();

		return h;
	}

	bool DBSQLLite::writeQuadrantToDB(TheWorld_Utils::MeshCacheBuffer& cache, TheWorld_Utils::MeshCacheBuffer::CacheQuadrantData& cacheQuadrantData, bool& stop)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("writeQuadrantToDB ") + __FUNCTION__, "ALL");

		PLOG_DEBUG << "Align CACHE <==> DB - Writing to DB quadrant " << cache.getCacheIdStr();

		std::string sql, sql1;
		int rc;

		float gridStep = cache.getGridStepInWU();
		size_t vertexPerSize = cache.getNumVerticesPerSize();
		int level = cache.getLevel();
		float quadPosX = cache.getLowerXGridVertex();
		float quadPosZ = cache.getLowerZGridVertex();
		float quadEndPosX = quadPosX + (vertexPerSize - 1) * gridStep;
		float quadEndPosZ = quadPosZ + (vertexPerSize - 1) * gridStep;

		DBSQLLiteOps* dbOps = &m_dbOpsInternalTransaction;

		if (m_dbOpsInternalTransaction.isTransactionOpened())
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite cannot procede inside caller's transaction!", ""), 0);

		enum class SQLInterface::QuadrantStatus status;
		std::string hash = getQuadrantHash(gridStep, vertexPerSize, level, quadPosX, quadPosZ, status);

		beginTransaction();

		bool eraseOldHash = false;
		size_t startIdxX = 0, startIdxZ = 0;
		
		// INSERT / UPDATE Quadrant
		if (hash.size() == 0)
		{
			eraseOldHash = true;

			//hash = cache.generateNewMeshId();
			hash = cacheQuadrantData.meshId;

			sql = "INSERT INTO Quadrant (GridStepInWU, VertexPerSize, Level, PosXStart, PosZStart, PosXEnd, PosZEnd, Status, Hash) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
			dbOps->prepareStmt(sql.c_str());

			rc = sqlite3_bind_double(dbOps->getStmt(), 1, gridStep);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.GridStepInWU failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = sqlite3_bind_int(dbOps->getStmt(), 2, (int)vertexPerSize);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.VertexPerSize failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = sqlite3_bind_int(dbOps->getStmt(), 3, level);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.Level failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = sqlite3_bind_double(dbOps->getStmt(), 4, quadPosX);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.PosXStart failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = sqlite3_bind_double(dbOps->getStmt(), 5, quadPosZ);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.PosZStart failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = sqlite3_bind_double(dbOps->getStmt(), 6, quadEndPosX);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.PosXEnd failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = sqlite3_bind_double(dbOps->getStmt(), 7, quadEndPosZ);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.PosZEnd failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			std::string str_status = cacheQuadrantData.heights32Buffer->size() > 0 ? "L" : "E";
			rc = sqlite3_bind_text(dbOps->getStmt(), 8, str_status.c_str(), -1, SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.Status failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = sqlite3_bind_blob(dbOps->getStmt(), 9, hash.c_str(), (int)hash.size(), SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = dbOps->execStmt();
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite insert/update Quadrant failed!", dbOps->errMsg(), rc));

			dbOps->finalizeStmt();
		}
		else if (cacheQuadrantData.meshId != hash)
		{
			eraseOldHash = true;

			// The quadrant was present with different hash so we have to update it and start loading
			sql1 = "UPDATE Quadrant SET Status = ?, Hash = ? WHERE GridStepInWU = %s AND VertexPerSize = %s AND Level = %s AND PosXStart = %s AND PosZStart = %s;";
			sql = dbOps->completeSQL(sql1.c_str(), to_string(gridStep).c_str(), to_string(vertexPerSize).c_str(), to_string(level).c_str(), to_string(quadPosX).c_str(), to_string(quadPosZ).c_str());
			dbOps->prepareStmt(sql.c_str());

			std::string str_status = cacheQuadrantData.heights32Buffer->size() > 0 ? "L" : "E";
			rc = sqlite3_bind_text(dbOps->getStmt(), 1, str_status.c_str(), -1, SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.Status failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = sqlite3_bind_blob(dbOps->getStmt(), 2, hash.c_str(), (int)hash.size(), SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));

			rc = dbOps->execStmt();
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite insert/update Quadrant failed!", dbOps->errMsg(), rc));

			dbOps->finalizeStmt();
		}

		if (eraseOldHash)
		{
			startIdxX = startIdxZ = 0;

			sql = "DELETE FROM QuadrantLoading WHERE Hash = ?;";
			dbOps->prepareStmt(sql.c_str());
			rc = sqlite3_bind_blob(dbOps->getStmt(), 1, hash.c_str(), (int)hash.size(), SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind QuadrantLoading.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));
			rc = dbOps->execStmt();
			int numDeleted = sqlite3_changes(dbOps->getConn());
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to delete from QuadrantLoading table!", sqlite3_errmsg(dbOps->getConn()), rc));
			dbOps->finalizeStmt();

			sql = "DELETE FROM TerrainQuadrant WHERE Hash = ?;";
			dbOps->prepareStmt(sql.c_str());
			rc = sqlite3_bind_blob(dbOps->getStmt(), 1, hash.c_str(), (int)hash.size(), SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));
			rc = dbOps->execStmt();
			numDeleted = sqlite3_changes(dbOps->getConn());
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to delete from TerrainQuadrant table!", sqlite3_errmsg(dbOps->getConn()), rc));
			dbOps->finalizeStmt();
		}

		endTransaction();

		if (!eraseOldHash)
		{
			startIdxX = startIdxZ = 0;

			sql = "SELECT LastXIdxLoaded, LastZIdxLoaded FROM QuadrantLoading WHERE Hash = ?;";
			dbOps->prepareStmt(sql.c_str());
			rc = sqlite3_bind_blob(dbOps->getStmt(), 1, hash.c_str(), (int)hash.size(), SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind QuadrantLoading.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));
			rc = dbOps->execStmt();
			if (rc != SQLITE_ROW && rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read from QuadrantLoading table!", sqlite3_errmsg(dbOps->getConn()), rc));
			if (rc == SQLITE_ROW)
			{
				size_t x = sqlite3_column_int64(dbOps->getStmt(), 0);
				startIdxX = x + 1;
				size_t z = sqlite3_column_int64(dbOps->getStmt(), 1);
				startIdxZ = z;
				if (startIdxX == vertexPerSize)
				{
					startIdxX = 0;
					startIdxZ++;
				}
				else if (startIdxX > vertexPerSize)
					throw(MapManagerExceptionDBException(__FUNCTION__, (std::string("DB SQLite value read from QuadrantLoading.LastXIdxLoaded inconsistent!") + std::to_string(x)).c_str(), sqlite3_errmsg(dbOps->getConn()), rc));
				if (startIdxZ > vertexPerSize)
					throw(MapManagerExceptionDBException(__FUNCTION__, (std::string("DB SQLite value read from QuadrantLoading.LastZIdxLoaded inconsistent!" ) + std::to_string(z)).c_str(), sqlite3_errmsg(dbOps->getConn()), rc));
			}
			dbOps->finalizeStmt();
		}
			
		if (cacheQuadrantData.heights32Buffer->size() > 0)
		{
			// read QuadrantLoading to restart: if start from beginning ==> write terrainEditValues and start loading GridVertex else continue loading gridvertex

			if (cacheQuadrantData.terrainEditValues->size() > 0)
			{
				beginTransaction();

				TheWorld_Utils::TerrainEdit terrainEdit;
				terrainEdit.deserialize(*cacheQuadrantData.terrainEditValues);

				bool insert = true;

				sql = "SELECT RowId FROM TerrainQuadrant WHERE Hash = ?;";
				dbOps->prepareStmt(sql.c_str());
				rc = sqlite3_bind_blob(dbOps->getStmt(), 1, hash.c_str(), (int)hash.size(), SQLITE_STATIC);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));
				rc = dbOps->execStmt();
				if (rc != SQLITE_ROW && rc != SQLITE_DONE)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read TerrainQuadrant Hash!", sqlite3_errmsg(dbOps->getConn()), rc));
				sqlite3_int64 rowid;
				if (rc == SQLITE_ROW)
				{
					insert = false;
					rowid = sqlite3_column_int64(dbOps->getStmt(), 0);
				}
				dbOps->finalizeStmt();


				if (insert)
				{
					sql = "INSERT INTO TerrainQuadrant ("
						"Hash,"								// 1
						"TerrainType,"						// 2
						"MinHeigth,"						// 3
						"MaxHeigth,"						// 4
						"LowElevationTexName,"				// 5
						"HighElevationTexName,"				// 6
						"DirtElevationTexName,"				// 7
						"RocksElevationTexName,"			// 8
						"EastSideXPlusNeedBlend,"			// 9
						"EastSideXPlusMinHeight,"			// 10
						"EastSideXPlusMaxHeight,"			// 11
						"WestSideXMinusNeedBlend,"			// 12
						"WestSideXMinusMinHeight,"			// 13
						"WestSideXMinusMaxHeight,"			// 14
						"SouthSideZPlusNeedBlend,"			// 15
						"SouthSideZPlusMinHeight,"			// 16
						"SouthSideZPlusMaxHeight,"			// 17
						"NorthSideZMinusNeedBlend,"			// 18
						"NorthSideZMinusMinHeight,"			// 19
						"NorthSideZMinusMaxHeight"			// 20
						") VALUES(? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ?);";
				}
				else
				{
					sql1 = "UPDATE TerrainQuadrant SET "
						"Hash=?,"							// 1
						"TerrainType=?,"					// 2
						"MinHeigth=?,"						// 3
						"MaxHeigth=?,"						// 4
						"LowElevationTexName=?,"			// 5
						"HighElevationTexName=?,"			// 6
						"DirtElevationTexName=?,"			// 7
						"RocksElevationTexName=?,"			// 8
						"EastSideXPlusNeedBlend=?,"			// 9
						"EastSideXPlusMinHeight=?,"			// 10
						"EastSideXPlusMaxHeight=?,"			// 11
						"WestSideXMinusNeedBlend=?,"		// 12
						"WestSideXMinusMinHeight=?,"		// 13
						"WestSideXMinusMaxHeight=?,"		// 14
						"SouthSideZPlusNeedBlend=?,"		// 15
						"SouthSideZPlusMinHeight=?,"		// 16
						"SouthSideZPlusMaxHeight=?,"		// 17
						"NorthSideZMinusNeedBlend=?,"		// 18
						"NorthSideZMinusMinHeight=?,"		// 19
						"NorthSideZMinusMaxHeight=?"		// 20
						" WHERE rowid = %s;";
					sql = dbOps->completeSQL(sql1.c_str(), to_string(rowid).c_str());
				}
				dbOps->prepareStmt(sql.c_str());

				rc = sqlite3_bind_blob(dbOps->getStmt(), 1, hash.c_str(), (int)hash.size(), SQLITE_STATIC);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_int(dbOps->getStmt(), 2, (int)terrainEdit.terrainType);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.TerrainType failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 3, terrainEdit.minHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.MinHeigth failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 4, terrainEdit.maxHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.MaxHeigth failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_text(dbOps->getStmt(), 5, terrainEdit.extraValues.lowElevationTexName_r, -1, SQLITE_STATIC);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.LowElevationTexName failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_text(dbOps->getStmt(), 6, terrainEdit.extraValues.highElevationTexName_g, -1, SQLITE_STATIC);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.HighElevationTexName failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_text(dbOps->getStmt(), 7, terrainEdit.extraValues.dirtTexName_b, -1, SQLITE_STATIC);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.DirtElevationTexName failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_text(dbOps->getStmt(), 8, terrainEdit.extraValues.rocksTexName_a, -1, SQLITE_STATIC);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.RocksElevationTexName failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_int(dbOps->getStmt(), 9, terrainEdit.eastSideXPlus.needBlend ? 1 : 0);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.EastSideXPlusNeedBlend failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 10, terrainEdit.eastSideXPlus.minHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.EastSideXPlusMinHeight failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 11, terrainEdit.eastSideXPlus.maxHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.EastSideXPlusMaxHeight failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_int(dbOps->getStmt(), 12, terrainEdit.westSideXMinus.needBlend ? 1 : 0);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.WestSideXPlusNeedBlend failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 13, terrainEdit.westSideXMinus.minHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.WestSideXPlusMinHeight failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 14, terrainEdit.westSideXMinus.maxHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.WestSideXPlusMaxHeight failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_int(dbOps->getStmt(), 15, terrainEdit.southSideZPlus.needBlend ? 1 : 0);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.SouthSideXPlusNeedBlend failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 16, terrainEdit.southSideZPlus.minHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.SouthSideXPlusMinHeight failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 17, terrainEdit.southSideZPlus.maxHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.SouthSideXPlusMaxHeight failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_int(dbOps->getStmt(), 18, terrainEdit.northSideZMinus.needBlend ? 1 : 0);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.NorthSideXPlusNeedBlend failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 19, terrainEdit.northSideZMinus.minHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.NorthSideXPlusMinHeight failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = sqlite3_bind_double(dbOps->getStmt(), 20, terrainEdit.northSideZMinus.maxHeight);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.NorthSideXPlusMaxHeight failed!", sqlite3_errmsg(dbOps->getConn()), rc));

				rc = dbOps->execStmt();
				if (rc != SQLITE_DONE)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite insert/update TerrainQuadrant failed!", dbOps->errMsg(), rc));

				dbOps->finalizeStmt();

				endTransaction();
			}

			// Table GridVertex
			size_t numVertices = vertexPerSize * vertexPerSize;

			//if (cacheQuadrantData.heights16Buffer->size() != numVertices * sizeof(uint16_t))
			//	throw(MapManagerExceptionWrongInput(__FUNCTION__, "heights16Buffer wrong size!"));

			if (cacheQuadrantData.heights32Buffer->size() != numVertices * sizeof(float))
				throw(MapManagerExceptionWrongInput(__FUNCTION__, "heights32Buffer wrong size!"));

			if (cacheQuadrantData.normalsBuffer->size() > 0 && !(cacheQuadrantData.normalsBuffer->size() == sizeof(struct TheWorld_Utils::_RGB) || cacheQuadrantData.normalsBuffer->size() == numVertices * sizeof(struct TheWorld_Utils::_RGB)))
				throw(MapManagerExceptionWrongInput(__FUNCTION__, "normalsBuffer wrong size!"));
			bool normalMapEmpty = false;
			if (cacheQuadrantData.normalsBuffer->size() == 0 || cacheQuadrantData.normalsBuffer->size() == sizeof(struct TheWorld_Utils::_RGB))
				normalMapEmpty = true;

			if (cacheQuadrantData.splatmapBuffer->size() > 0 && cacheQuadrantData.splatmapBuffer->size() != numVertices * sizeof(struct TheWorld_Utils::_RGBA))
				throw(MapManagerExceptionWrongInput(__FUNCTION__, "splatmapBuffer wrong size!"));
			bool splatMapEmpty = false;
			if (cacheQuadrantData.splatmapBuffer->size() == 0)
				splatMapEmpty = true;

			if (cacheQuadrantData.colormapBuffer->size() > 0 && cacheQuadrantData.colormapBuffer->size() != numVertices * sizeof(struct TheWorld_Utils::_RGBA))
				throw(MapManagerExceptionWrongInput(__FUNCTION__, "colormapBuffer wrong size!"));
			bool colorMapEmpty = false;
			if (cacheQuadrantData.colormapBuffer->size() == 0)
				colorMapEmpty = true;

			if (cacheQuadrantData.globalmapBuffer->size() > 0 && cacheQuadrantData.globalmapBuffer->size() != numVertices * sizeof(struct TheWorld_Utils::_RGB))
				throw(MapManagerExceptionWrongInput(__FUNCTION__, "globalmapBuffer wrong size!"));
			bool globalMapEmpty = false;
			if (cacheQuadrantData.globalmapBuffer->size() == 0)
				globalMapEmpty = true;

			beginTransaction();

			//size_t numDeleted = eraseVertices(quadPosX, quadEndPosX, quadPosZ, quadEndPosZ, level);

			string sql = "INSERT OR REPLACE INTO GridVertex (PosX, PosZ, Level, Radius, Azimuth, InitialAltitude, PosY, NormX, NormY, NormZ, ColorR, ColorG, ColorB, ColorA, LowElevationTexAmount, HighElevationTexAmount, DirtTexAmount, RocksTexAmount, GlobalMapR, GlobalMapG, GlobalMapB) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
			dbOps->prepareStmt(sql.c_str());

			sqlite3_stmt* stmt = nullptr;
			sql1 = "INSERT OR REPLACE INTO QuadrantLoading (Hash, LastXIdxLoaded, LastZIdxLoaded) VALUES (?, ?, ?);";
			dbOps->prepareStmt(sql1.c_str(), &stmt);

#define MAX_INSERT_BEFORE_COMMIT 1000
#define MAX_INSERT_BEFORE_LOG 100000

			size_t num = 0;
			size_t numForLog = 0;
			TheWorld_Utils::GuardProfiler* profiler1 = new TheWorld_Utils::GuardProfiler(std::string("writeQuadrantToDB 1 ") + __FUNCTION__, "GrideVertex Transaction");
			
			for (size_t z = startIdxZ; z < vertexPerSize; z++)
			{
				for (size_t x = startIdxX; x < vertexPerSize; x++)
				{
					startIdxX = startIdxZ = 0; 
					
					float vertexPosX = quadPosX + x * gridStep;
					float vertexPosY = cacheQuadrantData.heights32Buffer->at<float>(x, z, vertexPerSize);
					float vertexPosZ = quadPosZ + z * gridStep;
					float initialAltitude = vertexPosY;
					GridVertex v(vertexPosX, vertexPosZ, initialAltitude, level);
					float radius = v.radius();
					float azimuth = v.azimuth();
					float normalX = 0, normalY = 0, normalZ = 0;
					size_t lowElevationTexAmount = 0, highElevationTexAmount = 0, dirtTexAmount = 0, rocksTexAmount = 0;	// 0-255
					size_t colorR = 0, colorG = 0, colorB = 0, colorA = 0;	// 0-255
					size_t globalMapR = 0, globalMapG = 0, globalMapB = 0;	// 0-255
					if (!normalMapEmpty)
					{
						struct TheWorld_Utils::_RGB rgbNormal = cacheQuadrantData.normalsBuffer->at<TheWorld_Utils::_RGB>(x, z, vertexPerSize);									// 0-255
						Eigen::Vector3d packedNormal((const double)(double(rgbNormal.r) / 255), (const double)(double(rgbNormal.g) / 255), (const double)(double(rgbNormal.b) / 255));	// 0.0f-1.0f
						Eigen::Vector3d normal = TheWorld_Utils::unpackNormal(packedNormal);
						//float nx = (float)normal.x(), ny = (float)normal.y(), nz = (float)normal.z();
						//normal.normalize();
						normalX = (float)normal.x();	normalY = (float)normal.y();	normalZ = (float)normal.z();
					}
					if (!splatMapEmpty)
					{
						struct TheWorld_Utils::_RGBA rgba = cacheQuadrantData.splatmapBuffer->at<TheWorld_Utils::_RGBA>(x, z, vertexPerSize);
						lowElevationTexAmount = (size_t)rgba.r;
						highElevationTexAmount = (size_t)rgba.g;
						dirtTexAmount = (size_t)rgba.b;
						rocksTexAmount = (size_t)rgba.a;
					}
					if (!colorMapEmpty)
					{
						struct TheWorld_Utils::_RGBA rgba = cacheQuadrantData.colormapBuffer->at<TheWorld_Utils::_RGBA>(x, z, vertexPerSize);
						colorR = (size_t)rgba.r;
						colorG = (size_t)rgba.g;
						colorB = (size_t)rgba.b;
						colorA = (size_t)rgba.a;
					}
					if (!globalMapEmpty)
					{
						struct TheWorld_Utils::_RGB rgb = cacheQuadrantData.globalmapBuffer->at<TheWorld_Utils::_RGB>(x, z, vertexPerSize);
						globalMapR = (size_t)rgb.r;
						globalMapG = (size_t)rgb.g;
						globalMapB = (size_t)rgb.b;
					}

					rc = sqlite3_bind_double(dbOps->getStmt(), 1, vertexPosX);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.PosX failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					rc = sqlite3_bind_double(dbOps->getStmt(), 2, vertexPosZ);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.PosZ failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					rc = sqlite3_bind_int(dbOps->getStmt(), 3, level);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.Level failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					rc = sqlite3_bind_double(dbOps->getStmt(), 4, radius);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.Radius failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					rc = sqlite3_bind_double(dbOps->getStmt(), 5, azimuth);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.Azimuth failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					rc = sqlite3_bind_double(dbOps->getStmt(), 6, initialAltitude);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.InitialAltitude failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					rc = sqlite3_bind_double(dbOps->getStmt(), 7, vertexPosY);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.PosY failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					if (normalMapEmpty)
					{
						rc = sqlite3_bind_null(dbOps->getStmt(), 8);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.NormX failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 9);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.NormY failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 10);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.NormZ failed!", sqlite3_errmsg(dbOps->getConn()), rc));
					}
					else
					{
						rc = sqlite3_bind_double(dbOps->getStmt(), 8, normalX);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.NormX failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_double(dbOps->getStmt(), 9, normalY);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.NormY failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_double(dbOps->getStmt(), 10, normalZ);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.NormZ failed!", sqlite3_errmsg(dbOps->getConn()), rc));
					}

					if (colorMapEmpty)
					{
						rc = sqlite3_bind_null(dbOps->getStmt(), 11);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.ColorR failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 12);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.ColorG failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 13);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.ColorB failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 14);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.ColorA failed!", sqlite3_errmsg(dbOps->getConn()), rc));
					}
					else
					{
						rc = sqlite3_bind_int(dbOps->getStmt(), 11, (int)colorR);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.ColorR failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_int(dbOps->getStmt(), 12, (int)colorG);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.ColorG failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_int(dbOps->getStmt(), 13, (int)colorB);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.ColorB failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_int(dbOps->getStmt(), 14, (int)colorA);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.ColorA failed!", sqlite3_errmsg(dbOps->getConn()), rc));
					}

					if (splatMapEmpty)
					{
						rc = sqlite3_bind_null(dbOps->getStmt(), 15);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.LowElevationTexAmount failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 16);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.HighElevationTexAmount failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 17);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.DirtTexAmount failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 18);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.RocksTexAmount failed!", sqlite3_errmsg(dbOps->getConn()), rc));
					}
					else
					{
						rc = sqlite3_bind_int(dbOps->getStmt(), 15, (int)lowElevationTexAmount);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.LowElevationTexAmount failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_int(dbOps->getStmt(), 16, (int)highElevationTexAmount);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.HighElevationTexAmount failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_int(dbOps->getStmt(), 17, (int)dirtTexAmount);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.DirtTexAmount failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_int(dbOps->getStmt(), 18, (int)rocksTexAmount);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.RocksTexAmount failed!", sqlite3_errmsg(dbOps->getConn()), rc));
					}

					if (globalMapEmpty)
					{
						rc = sqlite3_bind_null(dbOps->getStmt(), 19);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.GlobalMapR failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 20);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.GlobalMapG failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_null(dbOps->getStmt(), 21);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.GlobalMapB failed!", sqlite3_errmsg(dbOps->getConn()), rc));
					}
					else
					{
						rc = sqlite3_bind_int(dbOps->getStmt(), 19, (int)globalMapR);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.GlobalMapR failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_int(dbOps->getStmt(), 20, (int)globalMapG);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.GlobalMapG failed!", sqlite3_errmsg(dbOps->getConn()), rc));

						rc = sqlite3_bind_int(dbOps->getStmt(), 21, (int)globalMapB);
						if (rc != SQLITE_OK)
							throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind GridVertex.GlobalMapB failed!", sqlite3_errmsg(dbOps->getConn()), rc));
					}

					rc = dbOps->execStmt();
					if (rc != SQLITE_DONE)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite insert/update GridVertex failed!", dbOps->errMsg(), rc));

					dbOps->resetStmt();

					rc = sqlite3_bind_blob(stmt, 1, hash.c_str(), (int)hash.size(), SQLITE_TRANSIENT);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind QuadrantLoading.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					rc = sqlite3_bind_int(stmt, 2, (int)x);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind QuadrantLoading.LastXIdxLoaded failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					rc = sqlite3_bind_int(stmt, 3, (int)z);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind QuadrantLoading.LastZIdxLoaded failed!", sqlite3_errmsg(dbOps->getConn()), rc));

					rc = dbOps->execStmt(stmt);
					if (rc != SQLITE_DONE)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite insert/update QuadrantLoading failed!", dbOps->errMsg(), rc));

					dbOps->resetStmt(stmt);

					num++;
					numForLog++;

					if (num >= MAX_INSERT_BEFORE_COMMIT)
					{
						dbOps->finalizeStmt();
						dbOps->finalizeStmt(stmt);
						endTransaction();
						delete profiler1;

						if (stop)
						{
							PLOG_DEBUG << "Align CACHE <==> DB - Stop requested, vertices written to DB " << std::to_string(z * vertexPerSize + x + 1);
							return false;
						}

						profiler1 = new TheWorld_Utils::GuardProfiler(std::string("writeQuadrantToDB 1 ") + __FUNCTION__, "GrideVertex Transaction");
						beginTransaction();
						dbOps->prepareStmt(sql.c_str());
						dbOps->prepareStmt(sql1.c_str(), &stmt);
						num = 0;
					}

					if (numForLog >= MAX_INSERT_BEFORE_LOG)
					{
						//PLOG_DEBUG << "DBSQLLite::writeQuadrantToDB - " << cache.getCacheFilePath() << ": " << std::to_string(z * vertexPerSize + x + 1) << " vertices written to DB";
						PLOG_DEBUG << "Align CACHE <==> DB - Vertices written to DB " << std::to_string(z * vertexPerSize + x + 1);
						numForLog = 0;
					}
				}
			}

			PLOG_DEBUG << "Align CACHE <==> DB - Vertices written to DB " << std::to_string(vertexPerSize * vertexPerSize);

			delete profiler1;
			
			dbOps->finalizeStmt(stmt);

			dbOps->finalizeStmt();

			sql = "DELETE FROM QuadrantLoading WHERE Hash = ?;";
			dbOps->prepareStmt(sql.c_str());
			rc = sqlite3_bind_blob(dbOps->getStmt(), 1, hash.c_str(), (int)hash.size(), SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind QuadrantLoading.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));
			rc = dbOps->execStmt();
			int numDeleted = sqlite3_changes(dbOps->getConn());
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to delete from QuadrantLoading table!", sqlite3_errmsg(dbOps->getConn()), rc));
			dbOps->finalizeStmt();

			sql = "UPDATE Quadrant SET Status = ? WHERE Hash = ?;";
			dbOps->prepareStmt(sql.c_str());
			std::string str_status = "C";
			rc = sqlite3_bind_text(dbOps->getStmt(), 1, str_status.c_str(), -1, SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind Quadrant.Status failed!", sqlite3_errmsg(dbOps->getConn()), rc));
			rc = sqlite3_bind_blob(dbOps->getStmt(), 2, hash.c_str(), (int)hash.size(), SQLITE_TRANSIENT);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind QuadrantLoading.Hash failed!", sqlite3_errmsg(dbOps->getConn()), rc));
			rc = dbOps->execStmt();
			int numUpdated = sqlite3_changes(dbOps->getConn());
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to delete from QuadrantLoading table!", sqlite3_errmsg(dbOps->getConn()), rc));
			dbOps->finalizeStmt();

			endTransaction();
		}

		return true;
	}

	void DBSQLLite::readQuadrantFromDB(TheWorld_Utils::MeshCacheBuffer& cache, std::string& meshId, enum class SQLInterface::QuadrantStatus& status, TheWorld_Utils::TerrainEdit& terrainEdit)
	{
		TheWorld_Utils::GuardProfiler profiler(std::string("readQuadrantFromDB ") + __FUNCTION__, "ALL");
		
		PLOG_DEBUG << "Align CACHE <==> DB - Reading quadrant from DB " << cache.getCacheIdStr();

		meshId.clear();
		
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();

		float gridStep = cache.getGridStepInWU();
		size_t vertxePerSize = cache.getNumVerticesPerSize();
		int level = cache.getLevel();
		float quadPosX = cache.getLowerXGridVertex();
		float quadPosZ = cache.getLowerZGridVertex();
		float quadEndPosX = quadPosX + (vertxePerSize - 1) * gridStep;
		float quadEndPosZ = quadPosZ + (vertxePerSize - 1) * gridStep;

		string sql1 = "SELECT Hash FROM Quadrant WHERE GridStepInWU = %s, VertexPerSize = %s AND Level = %s AND PosXStart = %s AND PosZStart = %s;";
		string sql = dbOps.completeSQL(sql1.c_str(), to_string(gridStep).c_str(), to_string(vertxePerSize).c_str(), to_string(level).c_str(), to_string(quadPosX).c_str(), to_string(quadPosZ).c_str());
		dbOps.prepareStmt(sql.c_str());

		int rc = dbOps.execStmt();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertex from GridVertex table!", sqlite3_errmsg(dbOps.getConn()), rc));

		if (rc == SQLITE_DONE)
			return;

		meshId = std::string((char*)sqlite3_column_blob(dbOps.getStmt(), 0), sqlite3_column_bytes(dbOps.getStmt(), 0));
		
		sql = "SELECT "
			"TerrainType,"						// 0
			"MinHeigth,"						// 1
			"MaxHeigth,"						// 2
			"LowElevationTexName,"				// 3
			"HighElevationTexName,"				// 4
			"DirtElevationTexName,"				// 5
			"RocksElevationTexName,"			// 6
			"EastSideXPlusNeedBlend,"			// 7
			"EastSideXPlusMinHeight,"			// 8
			"EastSideXPlusMaxHeight,"			// 9
			"WestSideXMinusNeedBlend,"			// 10
			"WestSideXMinusMinHeight,"			// 11
			"WestSideXMinusMaxHeight,"			// 12
			"SouthSideZPlusNeedBlend,"			// 13
			"SouthSideZPlusMinHeight,"			// 14
			"SouthSideZPlusMaxHeight,"			// 15
			"NorthSideZMinusNeedBlend,"			// 16
			"NorthSideZMinusMinHeight,"			// 17
			"NorthSideZMinusMaxHeight "			// 18
			"FROM TerrainQuadrant WHERE Hash = ?;";
		dbOps.prepareStmt(sql.c_str());
		rc = sqlite3_bind_blob(dbOps.getStmt(), 1, meshId.c_str(), (int)meshId.size(), SQLITE_STATIC);
		if (rc != SQLITE_OK)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB bind TerrainQuadrant.Hash failed!", sqlite3_errmsg(dbOps.getConn()), rc));

		rc = dbOps.execStmt();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read TerrainQuadrant Hash!", sqlite3_errmsg(dbOps.getConn()), rc));

		if (rc == SQLITE_DONE)
		{
			terrainEdit.size = 0;
			return;
		}

		terrainEdit.size = sizeof(TheWorld_Utils::TerrainEdit);
		terrainEdit.needUploadToServer = false;
		terrainEdit.normalsNeedRegen = false;
		terrainEdit.terrainType = (enum TheWorld_Utils::TerrainEdit::TerrainType)sqlite3_column_int(dbOps.getStmt(), 0);
		terrainEdit.minHeight = (float)sqlite3_column_double(dbOps.getStmt(), 1);
		terrainEdit.maxHeight = (float)sqlite3_column_double(dbOps.getStmt(), 2);
		strcpy_s(terrainEdit.extraValues.lowElevationTexName_r, sizeof(terrainEdit.extraValues.lowElevationTexName_r), (const char*)sqlite3_column_text(dbOps.getStmt(), 3));
		strcpy_s(terrainEdit.extraValues.highElevationTexName_g, sizeof(terrainEdit.extraValues.highElevationTexName_g), (const char*)sqlite3_column_text(dbOps.getStmt(), 4));
		strcpy_s(terrainEdit.extraValues.dirtTexName_b, sizeof(terrainEdit.extraValues.dirtTexName_b), (const char*)sqlite3_column_text(dbOps.getStmt(), 5));
		strcpy_s(terrainEdit.extraValues.rocksTexName_a, sizeof(terrainEdit.extraValues.rocksTexName_a), (const char*)sqlite3_column_text(dbOps.getStmt(), 6));
		terrainEdit.extraValues.texturesNeedRegen = false;
		terrainEdit.extraValues.emptyColormap = false;
		terrainEdit.extraValues.emptyGlobalmap = false;
		terrainEdit.eastSideXPlus.needBlend = sqlite3_column_int(dbOps.getStmt(), 7) == 1 ? true : false;
		terrainEdit.eastSideXPlus.minHeight = (float)sqlite3_column_double(dbOps.getStmt(), 8);
		terrainEdit.eastSideXPlus.maxHeight = (float)sqlite3_column_double(dbOps.getStmt(), 9);
		terrainEdit.westSideXMinus.needBlend = sqlite3_column_int(dbOps.getStmt(), 10) == 1 ? true : false;
		terrainEdit.westSideXMinus.minHeight = (float)sqlite3_column_double(dbOps.getStmt(), 11);
		terrainEdit.westSideXMinus.maxHeight = (float)sqlite3_column_double(dbOps.getStmt(), 12);
		terrainEdit.southSideZPlus.needBlend = sqlite3_column_int(dbOps.getStmt(), 13) == 1 ? true : false;
		terrainEdit.southSideZPlus.minHeight = (float)sqlite3_column_double(dbOps.getStmt(), 14);
		terrainEdit.southSideZPlus.maxHeight = (float)sqlite3_column_double(dbOps.getStmt(), 15);
		terrainEdit.northSideZMinus.needBlend = sqlite3_column_int(dbOps.getStmt(), 16) == 1 ? true : false;
		terrainEdit.northSideZMinus.minHeight = (float)sqlite3_column_double(dbOps.getStmt(), 17);
		terrainEdit.northSideZMinus.maxHeight = (float)sqlite3_column_double(dbOps.getStmt(), 18);
	}

	void DBSQLLite::finalizeDB(void)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		dbOps.finalizeDB();
	}
}