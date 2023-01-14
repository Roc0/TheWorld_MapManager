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
		MaxEffectOnWD = 0,
		MinEffectOnWD = 1
	};

	class WorldDefiner
	{
	public:
		_declspec(dllexport) WorldDefiner();
		//WorldDefiner(const WorldDefiner& wd)
		//{
		//	*this = wd;
		//}

		_declspec(dllexport) WorldDefiner(float posX, float posZ, enum class WDType type, enum class WDFunctionType functionType, float strength, float AOE, int level = 0, void* fp = nullptr);
		_declspec(dllexport) ~WorldDefiner();

		_declspec(dllexport) void setInternalValues(float posX, float posZ, int level, enum class WDType type, float radius, float azimuth, float azimuthDegree, float strength, float AOE, enum class WDFunctionType functionType, __int64 rowid, void* fp = nullptr);

		_declspec(dllexport) float getPosX(void) const
		{
			return m_posX;
		};
		_declspec(dllexport) float getPosZ(void) const
		{
			return m_posZ; 
		};
		_declspec(dllexport) enum class WDType getType(void) const
		{
			return m_type; 
		};
		_declspec(dllexport) float getStrength(void) const
		{
			return m_strength; 
		};
		_declspec(dllexport) float getAOE(void) const
		{
			return m_AOE; 
		};
		_declspec(dllexport) int getLevel(void) const
		{
			return m_level; 
		};
		_declspec(dllexport) float getRadius(void) const
		{
			return m_radius; 
		};
		_declspec(dllexport) float getAzimuth(void) const
		{
			return m_azimuth; 
		};
		_declspec(dllexport) float getAzimuthDegree(void) const
		{
			return m_azimuthDegree; 
		};
		_declspec(dllexport) enum class WDFunctionType getFunctionType(void) const
		{
			return m_functionType; 
		};

		_declspec(dllexport) __int64 getRowid(void) const
		{
			return m_rowid; 
		};

		// needed to use an istance of GridVertex as a key in a map (to keep the map sorted by m_posZ and by m_posX for equal m_posZ)
		bool operator<(const WorldDefiner& p) const
		{
			if (m_level < p.m_level)
				return true;
			else if (m_level > p.m_level)
				return false;
			else
			{
				if (m_posZ < p.m_posZ)
					return true;
				else if (m_posZ > p.m_posZ)
					return false;
				else
				{
					if (m_posX < p.m_posX)
						return true;
					else if (m_posX > p.m_posX)
						return false;
					else
						return m_type < p.m_type;
				}
			}
		}
		
		//WorldDefiner operator=(const WorldDefiner& wd)
		//{
		//	TODO
		//	return *this;
		//}


	private:
		float m_posX;
		float m_posZ;
		enum class WDType m_type;
		float m_strength;
		float m_AOE;
		int m_level;
		float m_radius;
		float m_azimuth;
		float m_azimuthDegree;
		enum class WDFunctionType m_functionType;
		__int64 m_rowid;
	};
}


