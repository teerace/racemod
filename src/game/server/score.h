#ifndef GAME_SERVER_INTERFACE_SCORE_H
#define GAME_SERVER_INTERFACE_SCORE_H

#include "entities/character.h"
#include "gamecontext.h"

#define NUM_CHECKPOINTS 25

class CPlayerData
{
public:
	CPlayerData()
	{
		Reset();
	}
	
	void Reset()
	{
		m_Time = 0;
		m_CurTime = 0;
		mem_zero(m_aCpTime, sizeof(m_aCpTime));
	}
	
	void Set(float Time, float *pCpTime)
	{
		m_Time = Time;
		mem_copy(m_aCpTime, pCpTime, sizeof(m_aCpTime));
	}
	
	bool Check(float Time, float *pCpTime)
	{
		if(!m_CurTime || Time < m_CurTime)
			m_CurTime = Time;

		if(!m_Time || Time < m_Time)
		{
			Set(Time, pCpTime);
			return true;
		}
		return false;
	}

	void SetCur(float Time)
	{
		m_CurTime = Time;
	}

	float m_Time;
	float m_CurTime;
	float m_aCpTime[NUM_CHECKPOINTS];
};

class IScore
{
	CPlayerData m_aPlayerData[MAX_CLIENTS];
	CPlayerData m_CurrentRecord;
	
public:
	IScore() { m_CurrentRecord.Reset(); }
	virtual ~IScore() {}
	
	CPlayerData *PlayerData(int ID) { return &m_aPlayerData[ID]; }
	CPlayerData *GetRecord() { return &m_CurrentRecord; }

	bool CheckRecord(int ClientID)
	{
		bool NewRecord = (!m_CurrentRecord.m_Time || m_aPlayerData[ClientID].m_Time < m_CurrentRecord.m_Time);
		if(NewRecord)
			m_CurrentRecord = m_aPlayerData[ClientID];
		return NewRecord;
	}
	
	virtual void LoadScore(int ClientID) = 0;
	virtual void SaveScore(int ClientID) = 0;
	
	virtual void ShowTop5(int ClientID, int Debut=1) = 0;
	virtual void ShowRank(int ClientID, const char* pName, bool Search=false) = 0;
};

#endif
