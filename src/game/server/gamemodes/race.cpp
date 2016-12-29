/* copyright (c) 2007 rajh, race mod stuff */
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/score.h>
#if defined(CONF_TEERACE)
#include <game/server/webapp.h>
#include <game/ghost.h>
#endif
#include <game/teerace.h>
#include "race.h"

CGameControllerRACE::CGameControllerRACE(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "Race";
	
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aRace[i].Reset();
#if defined(CONF_TEERACE)
		m_aStopRecordTick[i] = -1;
#endif
	}
}

CGameControllerRACE::~CGameControllerRACE()
{
}

int CGameControllerRACE::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	int ClientID = pVictim->GetPlayer()->GetCID();
	m_aRace[ClientID].Reset();
	
#if defined(CONF_TEERACE)
	if(Server()->RaceRecorder_IsRecording(ClientID))
		Server()->RaceRecorder_Stop(ClientID);
	
	if(Server()->GhostRecorder_IsRecording(ClientID))
		Server()->GhostRecorder_Stop(ClientID, 0);
#endif

	return 0;
}

void CGameControllerRACE::DoWincheck()
{
	if(m_GameOverTick == -1 && !m_Warmup)
	{
		if((g_Config.m_SvTimelimit > 0 && (Server()->Tick()-m_RoundStartTick) >= g_Config.m_SvTimelimit*Server()->TickSpeed()*60))
			EndRound();
	}
}

void CGameControllerRACE::Tick()
{
	IGameController::Tick();
	DoWincheck();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CRaceData *p = &m_aRace[i];

		if(p->m_RaceState == RACE_STARTED && GameServer()->m_apPlayers[i]->m_RaceClient == 1 &&
			Server()->Tick() - p->m_RefreshTick >= Server()->TickSpeed())
		{
			bool Checkpoint = p->m_CpTick != -1 && p->m_CpTick > Server()->Tick();
			GameServer()->SendRaceTime(i, GetTime(i), Checkpoint ? p->m_CpDiff : 0);
			p->m_RefreshTick = Server()->Tick();
		}
		
#if defined(CONF_TEERACE)
		// stop recording at the finish
		CPlayerData *pBest = GameServer()->Score()->PlayerData(i);
		if(Server()->RaceRecorder_IsRecording(i))
		{
			if(Server()->Tick() == m_aStopRecordTick[i])
			{
				m_aStopRecordTick[i] = -1;
				Server()->RaceRecorder_Stop(i);
				continue;
			}
			
			if(m_aRace[i].m_RaceState == RACE_STARTED && pBest->m_Time > 0 && pBest->m_Time < GetTime(i))
				Server()->RaceRecorder_Stop(i);
		}
		
		// stop ghost if time is bigger then best time
		if(Server()->GhostRecorder_IsRecording(i) && m_aRace[i].m_RaceState == RACE_STARTED && pBest->m_Time > 0 && pBest->m_Time < GetTime(i))
			Server()->GhostRecorder_Stop(i, 0);
#endif
	}
}

void CGameControllerRACE::Snap(int SnappingClient)
{
	int TmpRoundStartTick = m_RoundStartTick;
	if(SnappingClient >= 0)
	{
		CRaceData *p = &m_aRace[SnappingClient];
		m_RoundStartTick = p->m_RaceState == RACE_STARTED ? p->m_StartTick : Server()->Tick();
	}

	IGameController::Snap(SnappingClient);
	m_RoundStartTick = TmpRoundStartTick;
}

bool CGameControllerRACE::OnCheckpoint(int ID, int z)
{
	CRaceData *p = &m_aRace[ID];
	CPlayerData *pBest = GameServer()->Score()->PlayerData(ID);
	if(p->m_RaceState != RACE_STARTED)
		return false;

	p->m_aCpCurrent[z] = GetTime(ID);

	if(pBest->m_Time && pBest->m_aCpTime[z] != 0)
	{
		int Diff = p->m_aCpCurrent[z] - pBest->m_aCpTime[z];
		if(GameServer()->m_apPlayers[ID]->m_RaceClient == 1)
		{
			p->m_CpDiff = Diff;
			p->m_CpTick = Server()->Tick() + Server()->TickSpeed() * 2;
		}
		else if(Server()->Tick() - p->m_RefreshTick >= Server()->TickSpeed() / 2)
		{
			int Time = GameServer()->m_apPlayers[ID]->m_DDNetClient ? GetTime(ID) : 0;
			GameServer()->SendRaceTime(ID, Time, Diff);
			p->m_RefreshTick = Server()->Tick();
		}
	}

	return true;
}

bool CGameControllerRACE::OnRaceStart(int ID, int StartAddTime, bool Check)
{
	CRaceData *p = &m_aRace[ID];
	CCharacter *pChr = GameServer()->GetPlayerChar(ID);
	if(Check && (pChr->HasWeapon(WEAPON_GRENADE) || pChr->Armor()) && (p->m_RaceState == RACE_FINISHED || p->m_RaceState == RACE_STARTED))
		return false;
	
	p->m_RaceState = RACE_STARTED;
	p->m_StartTick = Server()->Tick();
	p->m_RefreshTick = Server()->Tick();
	p->m_StartAddTime = StartAddTime;

	if(p->m_RaceState != RACE_NONE)
	{
		// reset pickups
		if(!pChr->HasWeapon(WEAPON_GRENADE))
			GameServer()->m_apPlayers[ID]->m_ResetPickups = true;
	}

#if defined(CONF_TEERACE)
	if(g_Config.m_WaAutoRecord && GameServer()->Webapp() && Server()->GetUserID(ID) > 0 && GameServer()->Webapp()->CurrentMap()->m_ID > -1 && !Server()->GhostRecorder_IsRecording(ID))
	{
		Server()->GhostRecorder_Start(ID);
		
		CGhostSkin Skin;
		StrToInts(&Skin.m_Skin0, 6, pChr->GetPlayer()->m_TeeInfos.m_SkinName);
		Skin.m_UseCustomColor = pChr->GetPlayer()->m_TeeInfos.m_UseCustomColor;
		Skin.m_ColorBody = pChr->GetPlayer()->m_TeeInfos.m_ColorBody;
		Skin.m_ColorFeet = pChr->GetPlayer()->m_TeeInfos.m_ColorFeet;
		Server()->GhostRecorder_WriteData(ID, GHOSTDATA_TYPE_SKIN, (const char*)&Skin, sizeof(Skin));
	}
#endif

	return true;
}

bool CGameControllerRACE::OnRaceEnd(int ID, int FinishTime)
{
	CRaceData *p = &m_aRace[ID];
	CPlayerData *pBest = GameServer()->Score()->PlayerData(ID);
	if(p->m_RaceState != RACE_STARTED)
		return false;

	p->m_RaceState = RACE_FINISHED;

	// add the time from the start
	FinishTime += p->m_StartAddTime;
	
	GameServer()->m_apPlayers[ID]->m_Score = max(-(FinishTime / 1000), GameServer()->m_apPlayers[ID]->m_Score);

	int Improved = FinishTime - pBest->m_Time;
	bool NewRecord = pBest->Check(FinishTime, p->m_aCpCurrent);

	// save the score
	GameServer()->Score()->SaveScore(ID, FinishTime, p->m_aCpCurrent, NewRecord);
	if(NewRecord && GameServer()->Score()->CheckRecord(ID) && g_Config.m_SvShowTimes)
		GameServer()->SendRecord(-1);

	char aBuf[128];
	char aTime[64];
	IRace::FormatTimeLong(aTime, sizeof(aTime), FinishTime, true);
	str_format(aBuf, sizeof(aBuf), "%s finished in: %s", Server()->ClientName(ID), aTime);
	if(!g_Config.m_SvShowTimes)
		GameServer()->SendChatTarget(ID, aBuf);
	else
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

	if(Improved < 0)
	{
		str_format(aBuf, sizeof(aBuf), "New record: -%d.%03d second(s) better", abs(Improved) / 1000, abs(Improved) % 1000);
		if(!g_Config.m_SvShowTimes)
			GameServer()->SendChatTarget(ID, aBuf);
		else
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

#if defined(CONF_TEERACE)
	if(Server()->RaceRecorder_IsRecording(ID))
		m_aStopRecordTick[ID] = Server()->Tick()+Server()->TickSpeed();
#endif

	return true;
}

bool CGameControllerRACE::IsStart(int TilePos, vec2 Pos, int Team)
{
	return GameServer()->Collision()->GetIndex(TilePos) == TILE_BEGIN
		|| GameServer()->Collision()->GetIndex(Pos) == TILE_BEGIN;
}

bool CGameControllerRACE::IsEnd(int TilePos, vec2 Pos, int Team)
{
	return GameServer()->Collision()->GetIndex(TilePos) == TILE_END
		|| GameServer()->Collision()->GetIndex(Pos) == TILE_END;
}

void CGameControllerRACE::ProcessRaceTile(int ID, int TilePos, vec2 PrevPos, vec2 Pos)
{
	int Cp = GameServer()->Collision()->CheckCheckpoint(TilePos);
	if (Cp != -1)
		OnCheckpoint(ID, Cp);

	int Team = GameServer()->m_apPlayers[ID]->GetTeam();
	if(IsStart(TilePos, Pos, Team))
		OnRaceStart(ID, CalculateStartAddTime(PrevPos, Pos, Team));
	else if(IsEnd(TilePos, Pos, Team))
		OnRaceEnd(ID, CalculateFinishTime(GetTime(ID), PrevPos, Pos, Team));
}

int CGameControllerRACE::GetTime(int ID)
{
	return (Server()->Tick() - m_aRace[ID].m_StartTick) * 1000 / Server()->TickSpeed();
}

int CGameControllerRACE::CalculateStartAddTime(vec2 PrevPos, vec2 Pos, int Team)
{
	int Num = 1000 / Server()->TickSpeed();
	for (int i = 0; i <= Num; i++)
	{
		float a = i / (float)Num;
		vec2 TmpPos = mix(Pos, PrevPos, a);
		if (IsStart(-1, TmpPos, Team))
			return i;
	}
	return Num;
}

int CGameControllerRACE::CalculateFinishTime(int Time, vec2 PrevPos, vec2 Pos, int Team)
{
	int Num = 1000 / Server()->TickSpeed();
	for(int i = 0; i <= Num; i++)
	{
		float a = i / (float)Num;
		vec2 TmpPos = mix(PrevPos, Pos, a);
		if(IsEnd(-1, TmpPos, Team))
			return Time - Num + i;
	}
	return Time;
}
