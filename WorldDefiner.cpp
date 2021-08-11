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
	}

	WorldDefiner::WorldDefiner(float posX, float posZ, WDType type, float strength, float AOE, int level, float radius, float azimuth, float azimuthDegree, void* fp)
	{
		if (strength < 0.0 || strength > 1.0)
			throw(MapManagerExceptionWrongInput("Strength of a WD must be in range 0.0 / 1.0"));
		
		m_posX = posX;
		m_posZ = posZ;
		m_level = level;
		m_type = type;
		m_strength = strength;
		m_AOE = AOE;
		if (radius != 0.0 && azimuth != 0.0 && azimuthDegree != 0.0)
		{
			m_radius = radius;
			m_azimuth = azimuth;
			m_azimuthDegree = azimuthDegree;
		}
		else
		{
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
	}

	void WorldDefiner::init(float posX, float posZ, int level, WDType type, float radius, float azimuth, float azimuthDegree, float strength, float AOE, void* fp)
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
	}
	
	WorldDefiner::~WorldDefiner()
	{
	}
}