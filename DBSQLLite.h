#pragma once

#include "SQLInterface.h"
#include "sqlite3.h"
#include "MapManagerException.h"

#include <string>
#include <memory>
#include <thread>

namespace TheWorld_MapManager
{
	class DBSQLLiteConn
	{
	public:
		DBSQLLiteConn(void)
		{
			m_pDB = NULL;
		}
		
		~DBSQLLiteConn(void)
		{
			if (isOpened())
				close();
		}
		virtual const char* classname(void)
		{
			return "DBSQLLiteConn"; 
		}

		void open(const char* dbFilePath)
		{
			m_dbFilePath = dbFilePath;

			if (strlen(m_dbFilePath.c_str()) == 0)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB path is NULL!"));
			if (m_pDB != NULL)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB already opened!"));

			int rc = sqlite3_open_v2(m_dbFilePath.c_str(), &m_pDB, SQLITE_OPEN_READWRITE /*| SQLITE_OPEN_FULLMUTEX*/, NULL);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB open failed!", sqlite3_errmsg(m_pDB), rc));
			//PLOG_DEBUG << "DB Opened " << m_dbFilePath;

			//sqlite3_mutex* m = sqlite3_db_mutex(m_pDB);	// to debug reason only: in multithread mode cannot be used 
															// SQLITE is in threading mode Multi-thread executing sqlite3_config(SQLITE_CONFIG_MULTITHREAD) set at intilization time of Map Manager for ConnectionType::MultiConn or if 
															// not specified SQLITE_OPEN_FULLMUTEX in sqlite3_open_v2
		}

		void close(void)
		{
			if (m_pDB == NULL)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB not opened!"));

			int rc = sqlite3_close_v2(m_pDB);
			if (rc != SQLITE_OK)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB close failed!", sqlite3_errmsg(m_pDB), rc));

			m_pDB = NULL;
		}

		bool isOpened(void)
		{
			return (m_pDB != NULL); 
		}

		sqlite3* getConn(void) 
		{
			return m_pDB; 
		}

	private:
		sqlite3* m_pDB;
		string m_dbFilePath;
	};

	class DBThreadContext
	{
	public:
		DBThreadContext(void)
		{
		}

		~DBThreadContext(void)
		{
			deinit();
		}

		void deinit(void)
		{
			m_mapConnMtx.lock();
			for (auto& item : m_mapConn)
			{
				if (item.second->isOpened())
					item.second->close();
			}
			m_mapConn.clear();
			m_mapConnMtx.unlock();
		}

		DBSQLLiteConn* getConn(std::string& dbFilePath)
		{
			std::string key = dbFilePath;
			m_mapConnMtx.lock();
			if (!m_mapConn.contains(key))
				m_mapConn[key] = make_unique<DBSQLLiteConn>();
			DBSQLLiteConn* conn = m_mapConn[key].get();
			m_mapConnMtx.unlock();
			return conn;
		}

	private:
		map<std::string, std::unique_ptr<DBSQLLiteConn>> m_mapConn;
		std::recursive_mutex m_mapConnMtx;
	};
	
	class DBThreadContextPool
	{
	public:
		DBThreadContextPool(void)
		{
		}

		~DBThreadContextPool(void)
		{
			m_mapCtxMtx.lock();
			for (auto& item : m_mapCtx)
			{
			}
			m_mapCtxMtx.unlock();
		}

		DBThreadContext* getCurrentCtx(void)
		{
			size_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
			m_mapCtxMtx.lock();
			if (!m_mapCtx.contains(tid))
				m_mapCtx[tid] = make_unique<DBThreadContext>();
			DBThreadContext* ctx = m_mapCtx[tid].get();
			m_mapCtxMtx.unlock();
			return ctx;
		}

		void resetForCurrentThread(void)
		{
			size_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
			m_mapCtxMtx.lock();
			if (m_mapCtx.contains(tid))
			{
				m_mapCtx[tid]->deinit();
				m_mapCtx.erase(tid);
			}
			m_mapCtxMtx.unlock();
		}

	private:
		map<size_t, std::unique_ptr<DBThreadContext>> m_mapCtx;
		std::recursive_mutex m_mapCtxMtx;
	};

	class DBSQLLiteOps
	{
	
		// Example of insert with bind : https://renenyffenegger.ch/notes/development/databases/SQLite/c-interface/basic/index
		// SQLite3 and threading : https://stackoverflow.com/questions/10079552/what-do-the-mutex-and-cache-sqlite3-open-v2-flags-mean , https://dev.yorhel.nl/doc/sqlaccess

	public:
		DBSQLLiteOps(const char* dbFilePath)
		{
			m_initialized = false;
			m_conn = nullptr;
			m_connType = s_connType;
			m_lockAcquired = false;
			m_stmt = NULL;
			m_transactionOpened = false;
			m_dbFilePath = dbFilePath;
		}
		DBSQLLiteOps()
		{
			m_initialized = false;
			m_conn = nullptr;
			m_connType = s_connType;
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
		virtual const char* classname() 
		{
			return "DBSQLLiteOps";
		}

		bool isInitialized(void) 
		{ 
			return m_initialized; 
		}

		enum class ConnectionType
		{
			SingleConn = 0,
			MultiConn = 1
		};

		static void setConnectionType(enum class ConnectionType type)
		{
			if (type == ConnectionType::MultiConn)
			{
				int rc = sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
				if (rc == SQLITE_OK)
					s_connType = type;
			}
			else
				s_connType = type;
		}
		
		void init(const char* _dbFilePath = nullptr)
		{
			if (m_initialized)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DBSQLLiteOps already intialized!"));

			if (_dbFilePath)
				m_dbFilePath = _dbFilePath;
			
			if (m_dbFilePath.empty())
				throw(MapManagerExceptionDBException(__FUNCTION__, "DBSQLLiteOps cannot initialize instance : file path empty!"));

			if (m_connType == ConnectionType::SingleConn)
				m_conn = &s_conn;
			else
				m_conn = s_connPool.getCurrentCtx()->getConn(m_dbFilePath);
			
			if (!m_conn->isOpened())
			{
				m_conn->open(m_dbFilePath.c_str());
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

#define BEGINTTRANS_MAX_TIME_RETRY_ON_DB_BUSY	5000
#define BEGINTTRANS_SLEEP_TIME_ON_DB_BUSY		1

			TimerMs clock(false, false);
			clock.tick();
			while (true)
			{
				acquireLock();
				int rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
				releaseLock();
				if (rc != SQLITE_OK && rc != SQLITE_BUSY)
					throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB Begin Transaction failed!", sqlite3_errmsg(getConn()), rc));
				
				if (rc == SQLITE_OK)
					break;
				else
				{
					auto msDuration = clock.partialDuration().count();
					if (msDuration > BEGINTTRANS_MAX_TIME_RETRY_ON_DB_BUSY)
						throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite DB Begin Transaction failed! SQLITE_BUSY after timeout", sqlite3_errmsg(getConn()), rc));
					else
						Sleep(BEGINTTRANS_SLEEP_TIME_ON_DB_BUSY);
				}
			}

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

		string quote(const string& s)
		{
			return string("'") + s + string("'");
		}

		sqlite3* getConn(void)
		{
			return m_conn->getConn(); 
		}

		const char* errMsg()
		{
			return sqlite3_errmsg(getConn()); 
		}

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

		sqlite3_stmt* getStmt()
		{
			return m_stmt; 
		}
		bool isTransactionOpened() 
		{
			return m_transactionOpened; 
		}

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

			if (m_connType == ConnectionType::MultiConn)
			{
				s_DBAccessMtx.lock();
			}
			else
			{
				sqlite3* db = getConn();
				if (db)
					sqlite3_mutex_enter(sqlite3_db_mutex(db));
			}

			m_lockAcquired = true;
		}
		
		void releaseLock(void)
		{
			if (!m_lockAcquired)
				throw(MapManagerExceptionDBException(__FUNCTION__, "DB SQLite trying to release a lock not acquired!"));

			if (m_connType == ConnectionType::MultiConn)
			{
				s_DBAccessMtx.unlock();
			}
			else
			{
				sqlite3* db = getConn();
				if (db)
					sqlite3_mutex_leave(sqlite3_db_mutex(db));
			}

			m_lockAcquired = false;
		}

		void finalizeDB(void)
		{
			if (getConn() != nullptr)
				m_conn->close();

			s_connPool.resetForCurrentThread();
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
		static DBThreadContextPool s_connPool;
		static enum class ConnectionType s_connType;
		static std::recursive_mutex s_DBAccessMtx;

		DBSQLLiteConn* m_conn;
		enum class ConnectionType m_connType;
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
		DBSQLLite(DBType dbt, const char* dataPath, bool consoleDebugMode = false);
		~DBSQLLite();
		
		static void setConnectionType(enum class DBSQLLiteOps::ConnectionType type)
		{
			DBSQLLiteOps::setConnectionType(type);
		}
		
		const char* dbFilePath(void)
		{
			return m_dbFilePath.c_str(); 
		}

		virtual const char* classname()
		{
			return "DBSQLLite"; 
		}
		std::string readParam(std::string paranName);
		void beginTransaction(void);
		void endTransaction(bool commit = true);
		__int64 addWDAndVertices(WorldDefiner* pWD, std::vector<GridVertex>& vectGridVertices);
		bool eraseWD(__int64 wdRowid);
		void updateAltitudeOfVertex(__int64 vertexRowid, float altitude);
		void clearVerticesMarkedForUpdate(void);
		void getVertex(__int64 vertexRowid, GridVertex& gridVertex, int level = 0);
		void getVertex(GridVertex& gridVertex);
		void getVertices(float minX, float maxX, float minZ, float maxZ, vector<GridVertex>& vectGridVertices, int level = 0);
		bool getWD(float posX, float posZ, int level, WDType type, WorldDefiner& WD);
		bool getWD(__int64 wdRowid, WorldDefiner& WD);
		void getWDRowIdForVertex(__int64 vertexRowid, vector<__int64>& vectWDRowId);
		bool getFirstModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD);
		bool getNextModfiedVertex(GridVertex& gridVertex, std::vector<WorldDefiner>& vectWD);
		virtual std::string getQuadrantHash(float gridStep, size_t vertxePerSize, size_t level, float posX, float posZ);
		virtual void setQuadrantHash(float gridStep, size_t vertxePerSize, size_t level, float posX, float posZ, std::string hash);
		void finalizeDB(void);

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
