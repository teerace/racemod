#ifndef GAME_SERVER_WEBAPP_TOP_H
#define GAME_SERVER_WEBAPP_TOP_H

#include "data.h"

class CWebTop
{
public:
	class CUserRank
	{
	public:
		CUserRank(const char* pName, float Time)
		{
			m_Time = Time;
			str_copy(m_aName, pName, sizeof(m_aName));
		}
		CUserRank() {}
		
		float m_Time;
		char m_aName[32];
		
		bool operator<(const CUserRank& Other) { return (this->m_Time < Other.m_Time); }
	};
	
	class CParam : public IDataIn
	{
	public:
		int m_Start; // TODO: actually use this
		int m_ClientID;
	};
	
	class COut : public IDataOut
	{
	public:
		COut(int Type) { m_Type = Type; }
		int m_ClientID;
		sorted_array<CUserRank> m_lUserRanks;
	};
	
	static int GetTop5(void *pUserData);
};

#endif
