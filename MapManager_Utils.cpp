#include "pch.h"
#include <plog/Initializers/RollingFileInitializer.h>

#include "MapManager_Utils.h"
#include "MapManagerException.h"

namespace TheWorld_MapManager
{
    size_t limiter::s_concurrentExecutions = 0;
    std::recursive_mutex limiter::s_mtx;
    std::queue<limiter*> limiter::s_waiting;

    limiter::limiter(size_t maxConcurrentExecutions)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(s_mtx);
            if (s_concurrentExecutions < maxConcurrentExecutions)
            {
                s_concurrentExecutions++;
                return;
            }
            else
                s_waiting.push(this);
        }

        while (true)
        {
            s_mtx.lock();
            while (s_concurrentExecutions >= maxConcurrentExecutions)
            {
                s_mtx.unlock();
                Sleep(1);
                s_mtx.lock();
            }
            // s_concurrentExecutions is less than maxConcurrentExecutions and the mutex is locked
            if (s_waiting.front() == this)
            {
                // it's my turn
                s_waiting.pop();
                s_concurrentExecutions++;
                s_mtx.unlock();
                break;
            }
            s_mtx.unlock();
            Sleep(1);
        }
    }

    limiter::~limiter()
    {
        //std::lock_guard<std::recursive_mutex> lock(s_mtx);
        //if (s_concurrentExecutions > 0)
            s_concurrentExecutions--;
    }
    
    std::string ToString(GUID* guid)
    {
        char guid_string[37]; // 32 hex chars + 4 hyphens + null terminator
        snprintf(
            guid_string, sizeof(guid_string),
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid->Data1, guid->Data2, guid->Data3,
            guid->Data4[0], guid->Data4[1], guid->Data4[2],
            guid->Data4[3], guid->Data4[4], guid->Data4[5],
            guid->Data4[6], guid->Data4[7]);
        return guid_string;
    }

    std::string getModuleLoadPath(void)
	{
        char path[MAX_PATH];
        HMODULE hm = NULL;

        if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)getModuleLoadPath, &hm) == 0)
        {
            int ret = GetLastError();
            sprintf_s(path, sizeof(path), "GetModuleHandle failed trying to get DLL Load path, error = %d\n", ret);
            throw(MapManagerExceptionUnexpectedError(path));
        }
        if (GetModuleFileNameA(hm, path, sizeof(path)) == 0)
        {
            int ret = GetLastError();
            sprintf_s(path, sizeof(path), "GetModuleFileName failed trying to get DLL Load path, error = %d\n", ret);
            throw(MapManagerExceptionUnexpectedError(path));
        }

        int l = (int)strlen(path);
        for (int i = l; i >= 0; i--)
            if (path[i] == '\\')
            {
                path[i] = '\0';
                break;
            }
        
        std::string s = path;

        return s;
	}

    void MapManagerUtils::staticInit(const char* logPath, plog::Severity sev, plog::IAppender* appender)
    {
        if (appender == nullptr)
            plog::init(sev, logPath, 1000000, 3);
        else
            plog::init(sev, appender);

        PLOG(plog::get()->getMaxSeverity()) << "***************";
        PLOG(plog::get()->getMaxSeverity()) << "Log initilized!";
        PLOG(plog::get()->getMaxSeverity()) << "***************";
    }

    void MapManagerUtils::staticDeinit(void)
    {
        PLOG(plog::get()->getMaxSeverity()) << "*****************";
        PLOG(plog::get()->getMaxSeverity()) << "Log Terminated!";
        PLOG(plog::get()->getMaxSeverity()) << "*****************";
    }
        
    MapManagerUtils::~MapManagerUtils(void)
    {
        //PLOG_INFO << "*****************";
        //PLOG_INFO << "Log Terminated!";
        //PLOG_INFO << "*****************";
    }
}