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
	StopRace(ClientID);
	m_aRace[ClientID].Reset();
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

	bool PureTuning = GameServer()->IsPureTuning();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CRaceData *p = &m_aRace[i];

		if(p->m_RaceState == RACE_STARTED && !PureTuning)
			StopRace(i);

		if(p->m_RaceState == RACE_STARTED && !GameServer()->m_apPlayers[i]->m_RaceCfg.m_TimerWarmup &&
			Server()->Tick() - p->m_RefreshTick >= Server()->TickSpeed())
		{
			bool Checkpoint = p->m_CpTick != -1 && p->m_CpTick > Server()->Tick();
			GameServer()->SendRaceTime(i, GetTime(i), Checkpoint ? p->m_CpDiff : 0);
			p->m_RefreshTick = Server()->Tick();
		}
		
#if defined(CONF_TEERACE)
		// stop recording at the finish
		const CPlayerData *pBest = GameServer()->Score()->PlayerData(i);
		bool NewBest = pBest->CheckTime(GetTime(i));

		if(Server()->RaceRecorder_IsRecording(i) && 
			(Server()->Tick() == m_aStopRecordTick[i] || (m_aRace[i].m_RaceState == RACE_STARTED && !NewBest) || !PureTuning))
		{
			m_aStopRecordTick[i] = -1;
			Server()->RaceRecorder_Stop(i);
		}
		
		// stop ghost if time is bigger then best time
		if(Server()->GhostRecorder_IsRecording(i) && m_aRace[i].m_RaceState == RACE_STARTED && !NewBest)
			Server()->GhostRecorder_Stop(i, 0);
#endif
	}
}

void CGameControllerRACE::Snap(int SnappingClient)
{
	CNetObj_GameInfo *pGameInfoObj = (CNetObj_GameInfo *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	if(m_GameOverTick != -1)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_GAMEOVER;
	if(m_SuddenDeath)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;
	if(GameServer()->m_World.m_Paused)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_PAUSED;
	pGameInfoObj->m_RoundStartTick = m_RoundStartTick;
	pGameInfoObj->m_WarmupTimer = GameServer()->m_World.m_Paused ? m_UnpauseTimer : m_Warmup;

	pGameInfoObj->m_ScoreLimit = g_Config.m_SvScorelimit;
	pGameInfoObj->m_TimeLimit = g_Config.m_SvTimelimit;

	pGameInfoObj->m_RoundNum = (str_length(g_Config.m_SvMaprotation) && g_Config.m_SvRoundsPerMap) ? g_Config.m_SvRoundsPerMap : 0;
	pGameInfoObj->m_RoundCurrent = m_RoundCount+1;

	CPlayer *pPlayer = SnappingClient >= 0 ? GameServer()->m_apPlayers[SnappingClient] : 0;
	if(pPlayer && pPlayer->m_RaceCfg.m_TimerWarmup && m_aRace[SnappingClient].m_RaceState == RACE_STARTED)
	{
		pGameInfoObj->m_WarmupTimer = -m_aRace[SnappingClient].m_StartTick;
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_RACETIME;
	}
}

bool CGameControllerRACE::OnCheckpoint(int ID, int z)
{
	CRaceData *p = &m_aRace[ID];
	const CPlayerData *pBest = GameServer()->Score()->PlayerData(ID);
	if(p->m_RaceState != RACE_STARTED)
		return false;

	p->m_aCpCurrent[z] = GetTime(ID);

	if(pBest->m_Time && pBest->m_aCpTime[z] != 0)
	{
		int Diff = p->m_aCpCurrent[z] - pBest->m_aCpTime[z];
		const CPlayer::CRaceCfg *pConfig = &GameServer()->m_apPlayers[ID]->m_RaceCfg;
		bool TimeBroadcast = !pConfig->m_TimerWarmup && !pConfig->m_TimerNetMsg;
		if(pConfig->m_TimerNetMsg || (!pConfig->m_CheckpointNetMsg && TimeBroadcast))
		{
			p->m_CpDiff = Diff;
			p->m_CpTick = Server()->Tick() + Server()->TickSpeed() * 2;
		}
		else if(Server()->Tick() - p->m_RefreshTick >= Server()->TickSpeed() / 2)
		{
			int Time = GameServer()->m_apPlayers[ID]->CheckClient(CCustomClient::CLIENT_DDNET) ? GetTime(ID) : 0;
			GameServer()->SendCheckpoint(ID, Time, Diff);
			p->m_RefreshTick = Server()->Tick();
		}
	}

	return true;
}

void CGameControllerRACE::OnRaceStart(int ID, int StartAddTime)
{
	CRaceData *p = &m_aRace[ID];
	CCharacter *pChr = GameServer()->GetPlayerChar(ID);

	if(p->m_RaceState != RACE_NONE)
	{
		// reset pickups
		if(!pChr->HasWeapon(WEAPON_GRENADE))
			GameServer()->m_apPlayers[ID]->m_ResetPickups = true;
	}
	
	p->m_RaceState = RACE_STARTED;
	p->m_StartTick = Server()->Tick();
	p->m_RefreshTick = Server()->Tick();
	p->m_StartAddTime = StartAddTime;

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
}

void CGameControllerRACE::OnRaceEnd(int ID, int FinishTime)
{
	CRaceData *p = &m_aRace[ID];
	p->m_RaceState = RACE_FINISHED;

	if(!FinishTime)
	{
#if defined(CONF_TEERACE)
		if(Server()->RaceRecorder_IsRecording(ID))
		{
			m_aStopRecordTick[ID] = -1;
			Server()->RaceRecorder_Stop(ID);
		}
		if(Server()->GhostRecorder_IsRecording(ID))
			Server()->GhostRecorder_Stop(ID, FinishTime);
#endif
		return;
	}

	// TODO:
	// move all this into the scoring classes so the selected
	// scoring backend can decide how to handle the situation

	// add the time from the start
	FinishTime += p->m_StartAddTime;
	int Improved = FinishTime - GameServer()->Score()->PlayerData(ID)->m_Time;

	// save the score
	GameServer()->Score()->OnPlayerFinish(ID, FinishTime, p->m_aCpCurrent);

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
		m_aStopRecordTick[ID] = Server()->Tick() + Server()->TickSpeed();
	if(Server()->GhostRecorder_IsRecording(ID))
		Server()->GhostRecorder_Stop(ID, FinishTime);
#endif
}

bool CGameControllerRACE::IsStart(int TilePos, vec2 Pos, int Team) const
{
	return GameServer()->Collision()->GetIndex(TilePos) == TILE_BEGIN
		|| GameServer()->Collision()->GetIndex(Pos) == TILE_BEGIN;
}

bool CGameControllerRACE::IsEnd(int TilePos, vec2 Pos, int Team) const
{
	return GameServer()->Collision()->GetIndex(TilePos) == TILE_END
		|| GameServer()->Collision()->GetIndex(Pos) == TILE_END;
}

void CGameControllerRACE::ProcessRaceTile(int ID, int TilePos, vec2 PrevPos, vec2 Pos)
{
	int Cp = GameServer()->Collision()->CheckCheckpoint(TilePos);
	if(Cp != -1)
		OnCheckpoint(ID, Cp);

	int Team = GameServer()->m_apPlayers[ID]->GetTeam();
	if(CanStartRace(ID) && IsStart(TilePos, Pos, Team))
		OnRaceStart(ID, CalculateStartAddTime(PrevPos, Pos, Team));
	else if(CanEndRace(ID) && IsEnd(TilePos, Pos, Team))
		OnRaceEnd(ID, CalculateFinishTime(GetTime(ID), PrevPos, Pos, Team));
}

bool CGameControllerRACE::CanStartRace(int ID) const
{
	CCharacter *pChr = GameServer()->GetPlayerChar(ID);
	return (m_aRace[ID].m_RaceState == RACE_NONE || (!pChr->HasWeapon(WEAPON_GRENADE) && !pChr->Armor())) && GameServer()->IsPureTuning();
}

bool CGameControllerRACE::CanEndRace(int ID) const
{
	return m_aRace[ID].m_RaceState == RACE_STARTED;
}

int CGameControllerRACE::GetTime(int ID) const
{
	return (Server()->Tick() - m_aRace[ID].m_StartTick) * 1000 / Server()->TickSpeed();
}

int CGameControllerRACE::CalculateStartAddTime(vec2 PrevPos, vec2 Pos, int Team) const
{
	int Num = 1000 / Server()->TickSpeed();
	for(int i = 0; i <= Num; i++)
	{
		float a = i / (float)Num;
		vec2 TmpPos = mix(Pos, PrevPos, a);
		if(IsStart(-1, TmpPos, Team))
			return i;
	}
	return Num;
}

int CGameControllerRACE::CalculateFinishTime(int Time, vec2 PrevPos, vec2 Pos, int Team) const
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
