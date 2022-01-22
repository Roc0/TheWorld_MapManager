#include "pch.h"
#include <plog/Initializers/RollingFileInitializer.h>

#include "Utils.h"
#include "MapManagerException.h"

namespace TheWorld_MapManager
{
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

    void utils::init(const char* logPath, plog::Severity sev)
    {
        plog::init(sev, logPath, 1000000, 3);
        PLOG_INFO << endl;
        PLOG_INFO << "***************";
        PLOG_INFO << "Log initilized!";
        PLOG_INFO << "***************";
    }

    utils::~utils(void)
    {
        PLOG_INFO << "*****************";
        PLOG_INFO << "Log Deinitilized!";
        PLOG_INFO << "*****************";
    }
}