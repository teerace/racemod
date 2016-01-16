/* copyright (c) 2007 rajh and gregwar. Score stuff */
#ifndef GAME_SERVER_GAMEMODES_RACE_H
#define GAME_SERVER_GAMEMODES_RACE_H

#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/score.h>

class CGameControllerRACE : public IGameController
{
public:
	enum
	{
		RACE_NONE = 0,
		RACE_STARTED,
		RACE_FINISHED,
	};

	struct CRaceData
	{
		int m_RaceState;
		int m_StartTime;
		int m_RefreshTime;

		int m_aCpCurrent[NUM_CHECKPOINTS];
		int m_CpTick;
		int m_CpDiff;
		
		int m_StartAddTime;

		void Reset()
		{
			m_RaceState = RACE_NONE;
			m_StartTime = -1;
			m_RefreshTime = -1;
			mem_zero(m_aCpCurrent, sizeof(m_aCpCurrent));
			m_CpTick = -1;
			m_CpDiff = 0;
			m_StartAddTime = 0.0f;
		}
	} m_aRace[MAX_CLIENTS];
	
	CGameControllerRACE(class CGameContext *pGameServer);
	~CGameControllerRACE();
	
	vec2 *m_pTeleporter;
	
#if defined(CONF_TEERACE)
	int m_aStopRecordTick[MAX_CLIENTS];
#endif
	
	void InitTeleporter();

	virtual void DoWincheck();
	virtual void Tick();
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);

	virtual bool OnCheckpoint(int ID, int z);
	virtual bool OnRaceStart(int ID, int StartAddTime, bool Check=true);
	virtual bool OnRaceEnd(int ID, int FinishTime);

	int GetTime(int ID);
};

#endif
