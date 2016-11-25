/* copyright (c) 2007 rajh and gregwar. Score stuff */
#ifndef GAME_SERVER_GAMEMODES_RACE_H
#define GAME_SERVER_GAMEMODES_RACE_H

#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/score.h>

class CGameControllerRACE : public IGameController
{
#if defined(CONF_TEERACE)
	int m_aStopRecordTick[MAX_CLIENTS];
#endif

	virtual bool OnCheckpoint(int ID, int z);

	int GetTime(int ID);
	int CalculateStartAddTime(vec2 PrevPos, vec2 Pos, int Team);
	int CalculateFinishTime(int Time, vec2 PrevPos, vec2 Pos, int Team);

protected:
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
			m_StartAddTime = 0;
		}
	} m_aRace[MAX_CLIENTS];

	virtual bool IsStart(int TilePos, vec2 Pos, int Team);
	virtual bool IsEnd(int TilePos, vec2 Pos, int Team);

	virtual bool OnRaceStart(int ID, int StartAddTime, bool Check = true);
	virtual bool OnRaceEnd(int ID, int FinishTime);

public:
	enum
	{
		RACE_NONE = 0,
		RACE_STARTED,
		RACE_FINISHED,
	};
	
	CGameControllerRACE(class CGameContext *pGameServer);
	~CGameControllerRACE();

	virtual void DoWincheck();
	virtual void Tick();
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);

	virtual void ProcessRaceTile(int ID, int TilePos, vec2 PrevPos, vec2 Pos);

	int GetRaceState(int ID) const { return m_aRace[ID].m_RaceState; }
	void SetRaceState(int ID, int State) { m_aRace[ID].m_RaceState = State; }
};

#endif
