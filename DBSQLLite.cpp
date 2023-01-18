#include "pch.h"

#include "json/json.h"
#include <iostream>
#include <fstream>
#include <string>

#include "MapManager_Utils.h"
#include "DBSQLLite.h"

using namespace std;

namespace TheWorld_MapManager
{
	DBSQLLiteConn DBSQLLiteOps::s_conn;
	DBThreadContextPool DBSQLLiteOps::s_connPool;
	enum class DBSQLLiteOps::ConnectionType DBSQLLiteOps::s_connType = DBSQLLiteOps::ConnectionType::SingleConn;
	std::recursive_mutex DBSQLLiteOps::s_DBAccessMtx;
		
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

	string DBSQLLite::readParam(std::string paranName)
	{
		string paramValue = "";
		
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT ParamValue FROM Params WHERE ParamName = '" + paranName + "';";
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
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to update vertex altitude in GridVertex table!", sqlite3_errmsg(dbOps->getConn()), rc));
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
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to update vertex altitude in GridVertex table!", sqlite3_errmsg(dbOps->getConn()), rc));
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
		string sql = "SELECT PosX, PosY, PosZ, Radius, Azimuth, Level, InitialAltitude FROM GridVertex WHERE rowid = %s AND level = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(vertexRowid).c_str(), to_string(level).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertex with rowid from GridVertex table!", sqlite3_errmsg(dbOps.getConn()), rc));
		
		gridVertex.setInternalValues((float)sqlite3_column_double(dbOps.getStmt(), 0),	// PosX
									(float)sqlite3_column_double(dbOps.getStmt(), 1),	// PosY
									(float)sqlite3_column_double(dbOps.getStmt(), 2),	// PosZ
									(float)sqlite3_column_double(dbOps.getStmt(), 3),	// Radius
									(float)sqlite3_column_double(dbOps.getStmt(), 4),	// Azimuth
									sqlite3_column_int(dbOps.getStmt(), 5),				// Level
									(float)sqlite3_column_int(dbOps.getStmt(), 6),		// InitialAltitude
									vertexRowid);										// rowid
		dbOps.finalizeStmt();
	}

	void DBSQLLite::getVertex(GridVertex& gridVertex)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT PosY, Radius, Azimuth, InitialAltitude, VertexRowId FROM GridVertex WHERE PosX = %s AND PosZ = %s AND level = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(gridVertex.posX()).c_str(), to_string(gridVertex.posZ()).c_str(), to_string(gridVertex.level()).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertex with posX, posZ, level from GridVertex table!", sqlite3_errmsg(dbOps.getConn()), rc));

		gridVertex.setInternalValues((float)sqlite3_column_double(dbOps.getStmt(), 0),	// PosX
			(float)sqlite3_column_double(dbOps.getStmt(), 1),	// PosY
			(float)sqlite3_column_double(dbOps.getStmt(), 2),	// PosZ
			(float)sqlite3_column_double(dbOps.getStmt(), 3),	// Radius
			(float)sqlite3_column_double(dbOps.getStmt(), 4),	// Azimuth
			sqlite3_column_int(dbOps.getStmt(), 5),				// Level
			(float)sqlite3_column_int(dbOps.getStmt(), 6),		// InitialAltitude
			(__int64)sqlite3_column_int64(dbOps.getStmt(), 7));	// rowid
		dbOps.finalizeStmt();
	}

	void DBSQLLite::getVertices(float minX, float maxX, float minZ, float maxZ, vector<GridVertex>& vectGridVertex, int level)
	{
		vectGridVertex.clear();

		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT PosX, PosY, PosZ, Radius, Azimuth, Level, InitialAltitude, VertexRowId FROM GridVertex WHERE PosX >= %s AND PosX <= %s AND PosZ >= %s AND PosZ <= %s AND level = %s ORDER BY PosZ, PosX;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(minX).c_str(), to_string(maxX).c_str(), to_string(minZ).c_str(), to_string(maxZ).c_str(), to_string(level).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read vertex from GridVertex table!", sqlite3_errmsg(dbOps.getConn()), rc));

		while (rc == SQLITE_ROW)
		{
			GridVertex gridVertex;
			gridVertex.setInternalValues((float)sqlite3_column_double(dbOps.getStmt(), 0),	// PosX
				(float)sqlite3_column_double(dbOps.getStmt(), 1),							// PosY
				(float)sqlite3_column_double(dbOps.getStmt(), 2),							// PosZ
				(float)sqlite3_column_double(dbOps.getStmt(), 3),							// Radius
				(float)sqlite3_column_double(dbOps.getStmt(), 4),							// Azimuth
				sqlite3_column_int(dbOps.getStmt(), 5),										// Level
				(float)sqlite3_column_int(dbOps.getStmt(), 6),								// InitialAltitude
				(__int64)sqlite3_column_int64(dbOps.getStmt(), 7));							// rowid

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

	std::string DBSQLLite::getQuadrantHash(float gridStep, size_t vertxePerSize, size_t level, float posX, float posZ)
	{
		std::string h = "";

		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		string sql = "SELECT Hash FROM Quadrant WHERE GridStep = %s AND VertexPerSize = %s AND Level = %s AND PosXStart = %s AND PosZStart = %s;";
		string sql1 = dbOps.completeSQL(sql.c_str(), to_string(gridStep).c_str(), to_string(vertxePerSize).c_str(), to_string(level).c_str(), to_string(posX).c_str(), to_string(posZ).c_str());
		dbOps.prepareStmt(sql1.c_str());
		dbOps.acquireLock();
		int rc = sqlite3_step(dbOps.getStmt());
		dbOps.releaseLock();
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to read Quadrant Hash!", sqlite3_errmsg(dbOps.getConn()), rc));
		if (rc == SQLITE_ROW)
		{
			h = (char*)sqlite3_column_text(dbOps.getStmt(), 0);
		}
		dbOps.finalizeStmt();

		return h;
	}

	void DBSQLLite::setQuadrantHash(float gridStep, size_t vertxePerSize, size_t level, float posX, float posZ, std::string hash)
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

		float posXEnd = posX + (vertxePerSize - 1) * gridStep;
		float posZEnd = posZ + (vertxePerSize - 1) * gridStep;

		string sql = "INSERT INTO Quadrant (GridStep, VertexPerSize, Level, PosXStart, PosZStart, PosXEnd, PosZEnd, Hash) VALUES ("
			+ std::to_string(gridStep)								// GridStep
			+ "," + std::to_string(vertxePerSize)					// VertexPerSize
			+ "," + std::to_string(level)							// Level
			+ "," + std::to_string(posX)							// PosXStart
			+ "," + std::to_string(posZ)							// PosZStart
			+ "," + std::to_string(posXEnd)							// PosXEnd
			+ "," + std::to_string(posZEnd)							// PosZEnd
			+ ",\"" + hash											// Hash
			+ "\");";
		dbOps->acquireLock();
		int rc = sqlite3_exec(dbOps->getConn(), sql.c_str(), NULL, NULL, NULL);
		dbOps->releaseLock();
		if (rc != SQLITE_OK && rc != SQLITE_CONSTRAINT)
			throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite insert quadrant hash failed!", dbOps->errMsg(), rc));
		if (rc == SQLITE_CONSTRAINT)
		{
			// The quadrant was present so we have to update it
			string sql = "UPDATE Quadrant SET Hash = \"%s\" WHERE GridStep = %s AND VertexPerSize = %s AND Level = %s AND PosXStart = %s AND PosZStart = %s;";
			string sql1 = dbOps->completeSQL(sql.c_str(), hash, to_string(gridStep).c_str(), to_string(vertxePerSize).c_str(), to_string(level).c_str(), to_string(posX).c_str(), to_string(posZ).c_str());
			dbOps->acquireLock();
			dbOps->prepareStmt(sql1.c_str());
			int rc = sqlite3_step(dbOps->getStmt());
			dbOps->finalizeStmt();
			dbOps->releaseLock();
			if (rc != SQLITE_DONE)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite unable to update vertex altitude in GridVertex table!", sqlite3_errmsg(dbOps->getConn()), rc));
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
		
	void DBSQLLite::finalizeDB(void)
	{
		DBSQLLiteOps dbOps(dbFilePath());
		dbOps.init();
		dbOps.finalizeDB();
	}
}