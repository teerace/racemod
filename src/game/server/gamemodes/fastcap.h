#ifndef GAME_SERVER_GAMEMODES_CTF_H
#define GAME_SERVER_GAMEMODES_CTF_H
#include "race.h"

class CGameControllerFC : public CGameControllerRACE
{
	class CFlag *m_apFlags[2];
	class CFlag *m_apPlFlags[MAX_CLIENTS];

	bool IsOwnFlagStand(vec2 Pos, int Team);
	bool IsEnemyFlagStand(vec2 Pos, int Team);

protected:
	virtual bool IsStart(int TilePos, vec2 Pos, int Team) { return IsEnemyFlagStand(Pos, Team); }
	virtual bool IsEnd(int TilePos, vec2 Pos, int Team) { return IsOwnFlagStand(Pos, Team); }

public:
	CGameControllerFC(class CGameContext *pGameServer);
	
	virtual bool CanBeMovedOnBalance(int Cid);
	
	virtual bool OnEntity(int Index, vec2 Pos);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual bool CanSpawn(int Team, vec2 *pOutPos);
	
	virtual bool IsFastCap() const { return true; }

	virtual bool OnRaceStart(int ID, int StartAddTime, bool Check);
	virtual bool OnRaceEnd(int ID, int FinishTime);
	
	virtual void Snap(int SnappingClient);
};

#endif
