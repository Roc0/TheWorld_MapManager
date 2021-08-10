#pragma once
namespace TheWorld_MapManager
{
	enum class WDType
	{
		elevator = 0,
		depressor = 1,
		flattener = 2
	};
	
	class WorldDefiner
	{
	public:
		_declspec(dllexport) WorldDefiner(float posX, float posZ, WDType type, float strength, float AOE, int level = 0, void* fp = NULL);
		_declspec(dllexport) ~WorldDefiner();

		_declspec(dllexport) float getPosX(void) { return m_posX; };
		_declspec(dllexport) float getPosZ(void) { return m_posZ; };
		_declspec(dllexport) WDType getType(void) { return m_type; };
		_declspec(dllexport) float getStrength(void) { return m_strength; };
		_declspec(dllexport) float getAOE(void) { return m_AOE; };
		_declspec(dllexport) int getLevel(void) { return m_level; };
		_declspec(dllexport) float getRadius(void) { return m_radius; };
		_declspec(dllexport) float getAzimuth(void) { return m_azimuth; };
		_declspec(dllexport) float getAzimuthDegree(void) { return m_azimuthDegree; };

	private:
		float m_posX;
		float m_posZ;
		int m_level;
		WDType m_type;
		float m_strength;
		float m_AOE;
		float m_radius;
		float m_azimuth;
		float m_azimuthDegree;
	};
}


