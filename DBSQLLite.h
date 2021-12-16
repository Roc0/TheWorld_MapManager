#pragma once

#include <string>

#include "SQLInterface.h"
#include "sqlite3.h"
#include "MapManagerException.h"

namespace TheWorld_MapManager
{
	class DBSQLLiteConn
	{
	public:
		DBSQLLiteConn()
		{
			m_pDB = NULL;
		}
		
		~DBSQLLiteConn()
		{
			if (m_pDB)
				close();
		}
		virtual const char* classname() { return "DBSQLLiteConn"; }

		void open(const char* dbFilePath)
		{
			m_dbFilePath = dbFilePath;

			if (strlen(m_dbFilePath.c_str()) == 0)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB path is NULL!"));
			if (m_pDB != NULL)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB already opened!"));

			int rc = sqlite3_open_v2(m_dbFilePath.c_str(), &m_pDB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB open failed!", sqlite3_errmsg(m_pDB), rc));

			sqlite3_mutex* m = sqlite3_db_mutex(m_pDB);
		}

		void close()
		{
			if (m_pDB == NULL)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB not opened!"));

			int rc = sqlite3_close_v2(m_pDB);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB close failed!", sqlite3_errmsg(m_pDB), rc));

			m_pDB = NULL;
		}

		bool isOpened(void) { return (m_pDB != NULL); }

		sqlite3* getConn(void) { return m_pDB; }

	private:
		sqlite3* m_pDB;
		string m_dbFilePath;
	};

	class DBSQLLiteOps
	{
	
		// Example of insert with bind : https://renenyffenegger.ch/notes/development/databases/SQLite/c-interface/basic/index
		// SQLite3 and threading : https://stackoverflow.com/questions/10079552/what-do-the-mutex-and-cache-sqlite3-open-v2-flags-mean , https://dev.yorhel.nl/doc/sqlaccess

	public:
		DBSQLLiteOps(const char* dbFilePath)
		{
			m_initialized = false;
			m_lockAcquired = false;
			m_stmt = NULL;
			m_transactionOpened = false;
			m_dbFilePath = dbFilePath;
		}
		DBSQLLiteOps()
		{
			m_initialized = false;
			m_lockAcquired = false;
			m_stmt = NULL;
			m_transactionOpened = false;
		}
		~DBSQLLiteOps()
		{
			if (m_lockAcquired)
				releaseLock();
			
			if (m_stmt)
			{
				sqlite3_finalize(m_stmt);
				m_stmt = NULL;
			}

			if (m_transactionOpened)
				endTransaction(false);
			
			if (m_initialized)
				reset();
		}
		virtual const char* classname() { return "DBSQLLiteOps"; }

		bool isInitialized(void) { return m_initialized; }

		void init(const char* _dbFilePath = NULL)
		{
			if (m_initialized)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DBSQLLiteOps already intialized!"));

			if (_dbFilePath)
				m_dbFilePath = _dbFilePath;
			
			if (m_dbFilePath.empty())
				throw(MapManagerExceptionDBException(__FUNCTION__, "DBSQLLiteOps  cannot initialize instance : file path empty!"));

			if (!s_conn.isOpened())
			{
				if (m_dbFilePath.length() == 0)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB path is NULL!"));

				s_conn.open(m_dbFilePath.c_str());
			}

			m_initialized = true;
		}

		void reset()
		{
			if (!m_initialized)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DBSQLLiteOps trying to reset an unitialized instance!"));

			m_initialized = false;
		}

		void beginTransaction(void)
		{
			if (!m_initialized)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DBSQLLiteOps instance not initialized!"));

			sqlite3* db = getConn();
			if (db == NULL)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB not opened!"));

			acquireLock();
			int rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
			releaseLock();
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB Begin Transaction failed!", sqlite3_errmsg(getConn()), rc));

			m_transactionOpened = true;
		}

		void endTransaction(bool commit = true)
		{
			if (!m_initialized)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DBSQLLiteOps instance not initialized!"));

			if (!m_transactionOpened )
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite there is not an oened transaction!"));

			if (commit)
			{
				/*int rc = sqlite3_exec(getConn(), "COMMIT TRANSACTION;", NULL, NULL, NULL);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException("DB SQLite DB Commit Transaction failed!", sqlite3_errmsg(getConn()), rc)); */

#define COMMIT_MAX_TIME_RETRY_ON_DB_BUSY	1000
#define COMMIT_SLEEP_TIME_ON_DB_BUSY		10
				prepareStmt("COMMIT TRANSACTION;");

				int numRetry = 0;
				int rc = SQLITE_BUSY;
				while (rc == SQLITE_BUSY && numRetry < COMMIT_MAX_TIME_RETRY_ON_DB_BUSY / COMMIT_SLEEP_TIME_ON_DB_BUSY)
				{
					acquireLock();
					rc = sqlite3_step(m_stmt);		//executing the statement
					releaseLock();
					if (rc == SQLITE_BUSY)
						Sleep(COMMIT_SLEEP_TIME_ON_DB_BUSY);
					numRetry++;
				}
				if (rc != SQLITE_DONE)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB Commit Transaction failed!", sqlite3_errmsg(getConn()), rc));

				finalizeStmt();
				
			}
			else
			{
				/*int rc = sqlite3_exec(getConn(), "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
				if (rc != SQLITE_OK)
					throw(MapManagerExceptionDBException("DB SQLite DB Rollback Transaction failed!", sqlite3_errmsg(getConn()), rc));*/

#define COMMIT_MAX_TIME_RETRY_ON_DB_BUSY	1000
#define COMMIT_SLEEP_TIME_ON_DB_BUSY		10
				prepareStmt("ROLLBACK TRANSACTION;");

				int numRetry = 0;
				int rc = SQLITE_BUSY;
				while (rc == SQLITE_BUSY && numRetry < COMMIT_MAX_TIME_RETRY_ON_DB_BUSY / COMMIT_SLEEP_TIME_ON_DB_BUSY)
				{
					acquireLock();
					rc = sqlite3_step(m_stmt);		//executing the statement
					releaseLock();
					if (rc == SQLITE_BUSY)
						Sleep(COMMIT_SLEEP_TIME_ON_DB_BUSY);
					numRetry++;
				}
				if (rc != SQLITE_DONE)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB Rollback Transaction failed!", sqlite3_errmsg(getConn()), rc));

				finalizeStmt();
			}

			m_transactionOpened = false;
		}

		string quote(const string& s) {
			return string("'") + s + string("'");
		}

		sqlite3* getConn(void) { return s_conn.getConn(); }

		const char* errMsg() { return sqlite3_errmsg(getConn()); }

		void prepareStmt(const char* szSql)
		{
			if (!m_initialized)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DBSQLLiteOps instance not initialized!"));

			sqlite3* db = getConn();
			if (db == NULL)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB not opened!"));

			if (m_stmt != NULL)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite trying to prepare a statement not finalized!"));

			const char* pzTail;
			acquireLock();
			int rc = sqlite3_prepare_v2(db, szSql, -1, &m_stmt, &pzTail);
			releaseLock();
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite prepare of a statement failed!", sqlite3_errmsg(getConn()), rc));

			m_preparedStmtSQL = szSql;
		}

		void finalizeStmt(void)
		{
			if (!m_initialized)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DBSQLLiteOps instance not initialized!"));
			if (m_stmt == NULL)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite trying to finalize a statement not prepared!"));

			acquireLock();
			int rc = sqlite3_finalize(m_stmt);
			releaseLock();
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite finalize of a statement failed!", sqlite3_errmsg(getConn()), rc));

			m_stmt = NULL;
			m_preparedStmtSQL.clear();
		}

		sqlite3_stmt* getStmt() { return m_stmt; }
		bool isTransactionOpened() { return m_transactionOpened; }

		void resetStmt(void)
		{
			int rc = sqlite3_reset(m_stmt);
			if (rc != SQLITE_OK && rc != SQLITE_CONSTRAINT)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite reset of a statement failed!", sqlite3_errmsg(getConn()), rc));
		}

		void acquireLock(void)
		{
			if (m_lockAcquired)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite trying to acquire a lock recursively!"));
			
			sqlite3* db = getConn();
			if (db)
				sqlite3_mutex_enter(sqlite3_db_mutex(db));

			m_lockAcquired = true;
		}
		
		void releaseLock(void)
		{
			if (!m_lockAcquired)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite trying to release a lock not acquired!"));

			sqlite3* db = getConn();
			if (db)
				sqlite3_mutex_leave(sqlite3_db_mutex(db));

			m_lockAcquired = false;
		}

		void finalizeDB(void)
		{
			if (getConn())
				s_conn.close();
		}

		string completeSQL(const char* sql, ...)
		{
			char buffer[512];
			va_list params;
			va_start(params, sql);
			vsnprintf(buffer, sizeof(buffer), sql, params);
			va_end(params);
			return buffer;
		}

	private:
		static DBSQLLiteConn s_conn;
		bool m_initialized;
		bool m_lockAcquired;
		sqlite3_stmt* m_stmt;
		string m_preparedStmtSQL;
		bool m_transactionOpened;
		string m_dbFilePath;
	};

	class DBSQLLite : public SQLInterface
	{
	public:
		_declspec(dllexport) DBSQLLite(DBType dbt, const char* dataPath, bool debugMode = false);
		_declspec(dllexport) ~DBSQLLite();
		virtual const char* classname() { return "DBSQLLite"; }

		const char* dbFilePath(void) { return m_dbFilePath.c_str(); }

		_declspec(dllexport) std::string readParam(std::string paranName);
		_declspec(dllexport) void beginTransaction(void);
		_declspec(dllexport) void endTransaction(bool commit = true);
		_declspec(dllexport) __int64 addWDAndVertices(WorldDefiner* pWD, std::vector<GridVertex>& vectGridVertices);
		_declspec(dllexport) bool eraseWD(__int64 wdRowid);
		_declspec(dllexport) void updateAltitudeOfVertex(__int64 vertexRowid, float altitude);
		_declspec(dllexport) void clearVerticesMarkedForUpdate(void);
		_declspec(dllexport) void getVertex(__int64 vertexRowid, GridVertex& gridVertex, int level = 0);
		_declspec(dllexport) void getVertices(float minX, float maxX, float minZ, float maxZ, vector<GridVertex>& vectGridVertices, int level = 0);
		_declspec(dllexport) bool getWD(float posX, float posZ, int level, WDType type, WorldDefiner& WD);
		_declspec(dllexport) bool getWD(__int64 wdRowid, WorldDefiner& WD);
		_declspec(dllexport) void getWDRowIdForVertex(__int64 vertexRowid, vector<__int64>& vectWDRowId);
		_declspec(dllexport) bool getFirstModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD);
		_declspec(dllexport) bool getNextModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD);
		_declspec(dllexport) void finalizeDB(void);

	private:
		float getDistance(float x1, float y1, float x2, float y2)
		{
			return sqrtf((powf((x2 - x1), 2.0) + powf((y2 - y1), 2.0)));
		}

	private:
		std::string m_dbFilePath;
		DBSQLLiteOps m_dbOpsIterationModifiedVertices;
		std::vector<__int64> m_iteratedModifiedVerticesMap;
		DBSQLLiteOps m_dbOpsInternalTransaction;
	};
}
