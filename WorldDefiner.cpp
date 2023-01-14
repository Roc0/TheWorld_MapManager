#include "pch.h"

#define _USE_MATH_DEFINES // for C++

#include "WorldDefiner.h"
#include "MapManagerException.h"

namespace TheWorld_MapManager
{
	WorldDefiner::WorldDefiner()
	{
		m_posX = 0.0;
		m_posZ = 0.0;
		m_level = 0;
		m_type = (WDType)-1;
		m_strength = 0.0;
		m_AOE = 0.0;
		m_radius = 0.0;
		m_azimuth = 0.0;
		m_azimuthDegree = 0.0;
		m_functionType = (WDFunctionType)-1;
		m_rowid = -1;
	}

	WorldDefiner::WorldDefiner(float posX, float posZ, enum class WDType type, enum class WDFunctionType functionType, float strength, float AOE, int level, void* fp)
	{
		//if (strength < 0.0 || strength > 1.0)
		//	throw(MapManagerExceptionWrongInput("Strength of a WD must be in range 0.0 / 1.0"));
		
		m_posX = posX;
		m_posZ = posZ;
		m_level = level;
		m_type = type;
		m_functionType = functionType;
		m_strength = strength;
		m_AOE = AOE;

		m_rowid = -1;

		m_radius = sqrtf(powf(posX, 2.0) + powf(posZ, 2.0));
		if ((posX == 0 && posZ == 0) || m_radius == 0)
		{
			m_radius = 0;
			m_azimuth = 0;
			m_azimuthDegree = 0;
		}
		else
		{
			//angle of radius with x-axis (complementar of 2PI if Z < 0)
			m_azimuth = acosf(posX / m_radius);
			if (posZ < 0)
				m_azimuth = float(M_PI) * (float)2.0 - m_azimuth;
			m_azimuthDegree = (m_azimuth * 180) / float(M_PI);
		}
	}

	void WorldDefiner::setInternalValues(float posX, float posZ, int level, enum class WDType type, float radius, float azimuth, float azimuthDegree, float strength, float AOE, enum class WDFunctionType functionType, __int64 rowid, void* fp)
	{
		m_posX = posX;
		m_posZ = posZ;
		m_level = level;
		m_type = type;
		m_strength = strength;
		m_AOE = AOE;
		m_radius = radius;
		m_azimuth = azimuth;
		m_azimuthDegree = azimuthDegree;
		m_functionType = functionType;

		m_rowid = rowid;
	}
	
	WorldDefiner::~WorldDefiner()
	{
	}
}