#pragma once

#include <iostream>
#include <exception>

using namespace std;

namespace TheWorld_MapManager
{
	class MapManagerException : public exception
	{
	public:
		_declspec(dllexport) MapManagerException(const char* message = NULL)
		{ 
			m_exceptionName = "MapManagerException";
			if (strlen(message) == 0)
				m_message = "MapManager Generic Exception - C++ Exception";
			else
				m_message = message;
		};
		_declspec(dllexport) ~MapManagerException() {};

		const char* what() const throw ()
		{
			return m_message.c_str();
		}

		const char* exceptionName()
		{
			return m_exceptionName.c_str();
		}
	private:
		string m_message;
	
	protected:
		string m_exceptionName;
	};

	class MapManagerExceptionWrongInput : public MapManagerException
	{
	public:
		_declspec(dllexport) MapManagerExceptionWrongInput(const char* message = NULL) : MapManagerException(message) { m_exceptionName = "MapManagerExceptionWrongInput"; };
	};
}
