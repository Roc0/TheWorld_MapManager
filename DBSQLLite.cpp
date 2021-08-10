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

		DBSQLLiteOps dbOps(this);
		dbOps.init();
		dbOps.reset();
	}

	DBSQLLite::~DBSQLLite()
	{
	}

	void DBSQLLite::addWD(WorldDefiner& WD, std::vector<addWD_mapVertex>& mapVertex)
	{
		sqlite3_int64 WDRowID = -1;

		DBSQLLiteOps dbOps(this);
		
		debugUtils debugUtil;

		dbOps.init();
		dbOps.beginTransaction();

		/*
		* INSERT in table WorldDefiner
		*/
		
		string insert_WD_STMT = "INSERT INTO WorldDefiner (PosX, PosZ, radius, azimuth, azimuthDegree, Level, Type, Strength, AOE) VALUES ("
			+ std::to_string(WD.getPosX())
			+ "," + std::to_string(WD.getPosZ())
			+ "," + std::to_string(WD.getRadius())
			+ "," + std::to_string(WD.getAzimuth())
			+ "," + std::to_string(WD.getAzimuthDegree())
			+ "," + std::to_string(WD.getLevel())
			+ "," + std::to_string((int)WD.getType())
			+ "," + std::to_string(WD.getStrength())
			+ "," + std::to_string(WD.getAOE())
			+ ");";

		dbOps.acquireLock();
		int rc = sqlite3_exec(dbOps.getConn(), insert_WD_STMT.c_str(), NULL, NULL, NULL);
		WDRowID = sqlite3_last_insert_rowid(dbOps.getConn());
		dbOps.releaseLock();
		if (rc != SQLITE_OK && rc != SQLITE_CONSTRAINT)
			throw(MapManagerExceptionDBException("DB SQLite insert World Definer failed!", dbOps.errMsg(), rc));

		if (rc == SQLITE_CONSTRAINT)
			throw(MapManagerExceptionDuplicate("DB SQLite World Definer duplicate!"));

		/*
		* INSERT in table MapVertex
		*/

		int numVertices = (int)mapVertex.size();

		int idx = 0;
		int affected = 0;
		int notAffected = 0;
		int inserted = 0;
		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing vertices to MapVertex Table: ");
		if (numVertices > 0)
		{
			string insert_Vertex_STMT = "INSERT INTO MapVertex (PosX, PosZ, radius, azimuth, Level, PosY) VALUES (?, ?, ?, ?, ?, ?);";
			dbOps.prepareStmt(insert_Vertex_STMT.c_str());

			for (idx = 0; idx < numVertices; idx++)
			{
				if (mapVertex[idx].affected)
					affected++;
				else
					notAffected++;

				rc = sqlite3_bind_double(dbOps.getStmt(), 1, mapVertex[idx].posX);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException("DB SQLite DB bind PosX failed!", sqlite3_errmsg(dbOps.getConn()), rc));
				rc = sqlite3_bind_double(dbOps.getStmt(), 2, mapVertex[idx].posZ);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException("DB SQLite DB bind PosZ failed!", sqlite3_errmsg(dbOps.getConn()), rc));
				rc = sqlite3_bind_double(dbOps.getStmt(), 3, mapVertex[idx].radius);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException("DB SQLite DB bind radius failed!", sqlite3_errmsg(dbOps.getConn()), rc));
				rc = sqlite3_bind_double(dbOps.getStmt(), 4, mapVertex[idx].azimuth);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException("DB SQLite DB bind azimuth failed!", sqlite3_errmsg(dbOps.getConn()), rc));
				rc = sqlite3_bind_int(dbOps.getStmt(), 5, mapVertex[idx].level);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException("DB SQLite DB bind level failed!", sqlite3_errmsg(dbOps.getConn()), rc));
				rc = sqlite3_bind_double(dbOps.getStmt(), 6, 0.0);	// Not affected vertices have 0.0 altitude
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException("DB SQLite DB bind PosY failed!", sqlite3_errmsg(dbOps.getConn()), rc));

				dbOps.acquireLock();
				rc = sqlite3_step(dbOps.getStmt());
				sqlite3_int64 MapVertexRowId = sqlite3_last_insert_rowid(dbOps.getConn());
				dbOps.releaseLock();
				if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT)
					throw(MapManagerExceptionDBException("DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps.getConn()), rc));
				if (rc == SQLITE_DONE)
					inserted++;
				
				dbOps.resetStmt();

				if (mapVertex[idx].affected)
				{
					if (rc == SQLITE_CONSTRAINT)
					{
						// The Vertex was present in MapVertex table so we have to acquire his rowid
						DBSQLLiteOps dbOps1(this);
						dbOps1.init();
						string select_rowid__STMT = "SELECT rowid FROM MapVertex WHERE PosX = %s AND PosZ = %s AND Level = %s;";
						string s = dbOps1.completeSQL(select_rowid__STMT.c_str(), to_string(mapVertex[idx].posX).c_str(), to_string(mapVertex[idx].posZ).c_str(), to_string(mapVertex[idx].level).c_str());
						dbOps1.prepareStmt(s.c_str());
						dbOps1.acquireLock();
						rc = sqlite3_step(dbOps1.getStmt());
						dbOps1.releaseLock();
						if (rc != SQLITE_ROW)
							throw(MapManagerExceptionDBException("DB SQLite unable to read rowid vertex from MapVertx table!", sqlite3_errmsg(dbOps1.getConn()), rc));
						dbOps1.finalizeStmt();
					}
				}

				//if (debugMode() && fmod(idx, 1024 * 100) == 0) debugUtil.printVariablePartOfLine(idx + 1);
				if (debugMode() && fmod(idx, 1024 * 100) == 0)
				{
					string s = to_string(idx + 1);	s += " - Affected ";	s += to_string(affected);	s += " - Not affected ";	s += to_string(notAffected);	s += " - Inserted ";	s += to_string(inserted);
					debugUtil.printVariablePartOfLine(s.c_str());
				}
			}

			dbOps.finalizeStmt();
		}
		//if (debugMode()) debugUtil.printVariablePartOfLine(idx);
		if (debugMode())
		{
			string s = to_string(idx);	s += " - Affected ";	s += to_string(affected);	s += " - Not affected ";	s += to_string(notAffected);	s += " - Inserted ";	s += to_string(inserted);
			debugUtil.printVariablePartOfLine(s.c_str());
		}

		/*
		* INSERT in table MapVertex_Mod
		*/

		idx = 0;
		affected = 0;
		inserted = 0;
		if (debugMode()) debugUtil.printFixedPartOfLine(classname(), __FUNCTION__, "Writing affected vertices to MapVertex_Mod Table: ");
		if (numVertices > 0)
		{
			string insert_Vertex_STMT = "INSERT INTO MapVertex_Mod (PosX, PosZ, Level) VALUES (?, ?, ?);";
			dbOps.prepareStmt(insert_Vertex_STMT.c_str());

			for (idx = 0; idx < numVertices; idx++)
			{
				// insert vertices not affected by WD (to complete the squres)
				// affected vertices will be inserted in MapVertex_Mod table to be computed later
				if (mapVertex[idx].affected)
				{
					affected++;

					rc = sqlite3_bind_double(dbOps.getStmt(), 1, mapVertex[idx].posX);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException("DB SQLite DB bind PosX failed!", sqlite3_errmsg(dbOps.getConn()), rc));
					rc = sqlite3_bind_double(dbOps.getStmt(), 2, mapVertex[idx].posZ);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException("DB SQLite DB bind PosZ failed!", sqlite3_errmsg(dbOps.getConn()), rc));
					rc = sqlite3_bind_int(dbOps.getStmt(), 3, mapVertex[idx].level);
					if (rc != SQLITE_OK)
						throw(MapManagerExceptionDBException("DB SQLite DB bind level failed!", sqlite3_errmsg(dbOps.getConn()), rc));

					dbOps.acquireLock();
					rc = sqlite3_step(dbOps.getStmt());
					dbOps.releaseLock();
					if (rc == SQLITE_DONE)
						inserted++;
					if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT)
						throw(MapManagerExceptionDBException("DB SQLite DB insert vertex failed!", sqlite3_errmsg(dbOps.getConn()), rc));

					dbOps.resetStmt();
				}

				//if (debugMode() && fmod(idx, 1024 * 100) == 0) debugUtil.printVariablePartOfLine(idx + 1);
				if (debugMode() && fmod(idx, 1024 * 100) == 0)
				{
					string s = to_string(idx + 1);	s += " - Affected ";	s += to_string(affected);	s += " - Inserted ";	s += to_string(inserted);
					debugUtil.printVariablePartOfLine(s.c_str());
				}
			}

			dbOps.finalizeStmt();
		}
		//if (debugMode()) debugUtil.printVariablePartOfLine(idx);
		if (debugMode())
		{
			string s = to_string(idx);	s += " - Affected ";	s += to_string(affected);	s += " - Inserted ";	s += to_string(inserted);
			debugUtil.printVariablePartOfLine(s.c_str());
		}

		dbOps.endTransaction();
		dbOps.reset();
	}

	void DBSQLLite::finalizeDB(void)
	{
		DBSQLLiteOps dbOps(this);
		dbOps.finalizeDB();
	}
}