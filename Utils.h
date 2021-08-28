#pragma once


#include "framework.h"
#include <string>
#include <iostream>
#include <chrono>
#include "gsl\assert"


namespace TheWorld_MapManager
{
	template <class TimeT = std::chrono::milliseconds, class ClockT = std::chrono::steady_clock> class Timer
	{
		using timep_t = typename ClockT::time_point;
		timep_t _start = ClockT::now(), _end = {};

	public:
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
			std::cout << message << " duration = " << duration().count() << " ms\n";
		}
	};

#define TimerMs Timer<std::chrono::milliseconds, std::chrono::steady_clock>

	class debugUtils
	{
	public:
		debugUtils()
		{
			m_console = GetStdHandle(STD_OUTPUT_HANDLE);
			m_startCursorPosition = { 0, 0 };
			m_cursorPosition = { 0, 0 };
		}

		void printFixedPartOfLine(const char*classname, const char *functionname, const char* m, debugUtils *pPreviousPos = NULL)
		{
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
			SetConsoleCursorPosition(m_console, m_cursorPosition);
			std::cout << m << "\n";
		}

		void printVariablePartOfLine(int i)
		{
			SetConsoleCursorPosition(m_console, m_cursorPosition);
			std::cout << i << "\n";
		}

		void printNewLine(void)
		{
			std::cout << "\n";
		}

		COORD getStartPosition(void) { return m_startCursorPosition; }

	private:
		void* m_console;
		COORD m_startCursorPosition;
		COORD m_cursorPosition;
	};

	std::string getModuleLoadPath(void);

}