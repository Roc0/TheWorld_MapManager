#pragma once
namespace TheWorld_MapManager
{
	enum class WDType
	{
		elevator = 0,
		depressor = 1,
		flattener = 2
	};
	
	enum class WDFunctionType
	{
		cosin = 0
	};

	class WorldDefiner
	{
	public:
		_declspec(dllexport) WorldDefiner();
		_declspec(dllexport) WorldDefiner(float posX, float posZ, WDType type, WDFunctionType functionType, float strength, float AOE, int level = 0, void* fp = nullptr);
		_declspec(dllexport) ~WorldDefiner();

		_declspec(dllexport) void setInternalValues(float posX, float posZ, int level, WDType type, float radius, float azimuth, float azimuthDegree, float strength, float AOE, WDFunctionType functionType, __int64 rowid, void* fp = nullptr);

		_declspec(dllexport) float getPosX(void) { return m_posX; };
		_declspec(dllexport) float getPosZ(void) { return m_posZ; };
		_declspec(dllexport) WDType getType(void) { return m_type; };
		_declspec(dllexport) float getStrength(void) { return m_strength; };
		_declspec(dllexport) float getAOE(void) { return m_AOE; };
		_declspec(dllexport) int getLevel(void) { return m_level; };
		_declspec(dllexport) float getRadius(void) { return m_radius; };
		_declspec(dllexport) float getAzimuth(void) { return m_azimuth; };
		_declspec(dllexport) float getAzimuthDegree(void) { return m_azimuthDegree; };
		_declspec(dllexport) WDFunctionType getFunctionType(void) { return m_functionType; };

		_declspec(dllexport) __int64 getRowid(void) { return m_rowid; };

	private:
		float m_posX;
		float m_posZ;
		WDType m_type;
		float m_strength;
		float m_AOE;
		int m_level;
		float m_radius;
		float m_azimuth;
		float m_azimuthDegree;
		WDFunctionType m_functionType;
		__int64 m_rowid;
	};
}


