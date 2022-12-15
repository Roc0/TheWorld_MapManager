#pragma once


#include "framework.h"
#include <string>
#include <iostream>
#include <chrono>
#include <mutex>
#include <queue>

#include "gsl\assert"
#include <plog/Log.h>

namespace TheWorld_MapManager
{
	std::string ToString(GUID* guid);

	class limiter
	{
	public:
		limiter(size_t maxConcurrentExecutions);
		~limiter(void);
	private:
		static size_t s_concurrentExecutions;
		static std::recursive_mutex s_mtx;
		static std::queue<limiter*> s_waiting;
	};
	
	class utils
	{
	public:
		utils(void) {}
		void init(const char* logPath, plog::Severity, plog::IAppender* appender = NULL);
		~utils(void);
	};

	template <class TimeT = std::chrono::milliseconds, class ClockT = std::chrono::steady_clock> class Timer
	{
		using timep_t = typename ClockT::time_point;
		timep_t _start = ClockT::now(), _end = {};

	public:
		Timer(bool plogOutputEnabled, bool consoleOutputEnabled)
		{
			m_plogOutputEnabled = plogOutputEnabled;
			m_consoleOutputEnabled = consoleOutputEnabled;
		}
		
		void tick() {
			_end = timep_t{};
			_start = ClockT::now();
		}

		void tock() { _end = ClockT::now(); }

		template <class TT = TimeT>
		TT duration() const {
			Expects(_end != timep_t{} && "toc before reporting");
			//assert(_end != timep_t{} && "toc before reporting");
			return std::chrono::duration_cast<TT>(_end - _start);
		}

		void printDuration(const char* message)
		{
			tock();
			if (m_consoleOutputEnabled)
				std::cout << message << " duration = " << duration().count() << " ms\n";
			if (m_plogOutputEnabled)
				PLOG_INFO << message << " duration = " << duration().count() << " ms";
		}
	private:
		bool m_plogOutputEnabled;
		bool m_consoleOutputEnabled;
	};

#define TimerMs Timer<std::chrono::milliseconds, std::chrono::steady_clock>

	class consoleDebugUtil
	{
	public:
		consoleDebugUtil(bool debugMode)
		{
			m_console = GetStdHandle(STD_OUTPUT_HANDLE);
			m_startCursorPosition = { 0, 0 };
			m_cursorPosition = { 0, 0 };
			m_debugMode = debugMode;
		}

		void printFixedPartOfLine(const char*classname, const char *functionname, const char* m, consoleDebugUtil*pPreviousPos = NULL)
		{
			if (!m_debugMode)
				return;

			if (pPreviousPos)
			{
				SetConsoleCursorPosition(m_console, pPreviousPos->getStartPosition());
				printNewLine();
			}
			
			CONSOLE_SCREEN_BUFFER_INFO cbsi;
			GetConsoleScreenBufferInfo(m_console, &cbsi);
			m_startCursorPosition = cbsi.dwCursorPosition;
			std::cout << functionname << " - " << m;
			GetConsoleScreenBufferInfo(m_console, &cbsi);
			m_cursorPosition = cbsi.dwCursorPosition;
		}

		void printVariablePartOfLine(const char* m)
		{
			if (!m_debugMode)
				return;

			SetConsoleCursorPosition(m_console, m_cursorPosition);
			std::cout << m << "\n";
		}

		void printVariablePartOfLine(int i)
		{
			if (!m_debugMode)
				return;

			SetConsoleCursorPosition(m_console, m_cursorPosition);
			std::cout << i << "\n";
		}

		void printNewLine(void)
		{
			if (!m_debugMode)
				return;

			std::cout << "\n";
		}

		COORD getStartPosition(void) { return m_startCursorPosition; }

	private:
		void* m_console;
		COORD m_startCursorPosition;
		COORD m_cursorPosition;
		bool m_debugMode;
	};

	std::string getModuleLoadPath(void);

	//void FloatToByteStream(float f, BYTE* stream, size_t& size)
	//{
	//	float* pf = &f;
	//	BYTE* pc = reinterpret_cast<BYTE*>(pf);
	//	for (size_t i = 0; i < sizeof(float); i++)
	//	{
	//		stream[i] = *pc;
	//		pc++;
	//	}
	//}
	
	//template <typename T>
	//void serializeToByteStream(T in, BYTE* stream, size_t& size)
	//{
	//	T* pIn = &in;
	//	BYTE* pc = reinterpret_cast<BYTE*>(pIn);
	//	for (size_t i = 0; i < sizeof(T); i++)
	//	{
	//		stream[i] = *pc;
	//		pc++;
	//	}
	//	size = sizeof(T);
	//}

	//template <typename T>
	//T deserializeFromByteStream(BYTE* stream, size_t& size)
	//{
	//	T* pOut = reinterpret_cast<T*>(stream);
	//	size = sizeof(T);
	//	return *pOut;
	//}
}