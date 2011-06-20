#ifndef GAME_SERVER_WEBAPP_RUN_H
#define GAME_SERVER_WEBAPP_RUN_H

#include <game/server/score.h>

#include <game/data.h>

class CWebRun
{
public:
	class CParam : public IDataIn
	{
	public:
		CParam() { m_Tick = -1; }
		int m_ClientID;
		int m_UserID;
		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		float m_Time;
		float m_aCpTime[NUM_CHECKPOINTS];
		int m_Tick;
	};
	
	class COut : public IDataOut
	{
	public:
		COut(int Type) { m_Type = Type; }
		int m_UserID;
		int m_ClientID;
		int m_Tick;
	};
	
	static int Post(void *pUserData);
};

#endif
