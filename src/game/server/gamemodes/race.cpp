/* copyright (c) 2007 rajh, race mod stuff */
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/score.h>
#if defined(CONF_TEERACE)
#include <game/server/webapp.h>
#endif
#include <string.h>
#include "race.h"

CGameControllerRACE::CGameControllerRACE(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "Race";
	m_pTeleporter = 0;
	
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
	delete[] m_pTeleporter;
}

void CGameControllerRACE::InitTeleporter()
{
	int ArraySize = 0;
	if(GameServer()->Collision()->Layers()->TeleLayer())
	{
		for(int i = 0; i < GameServer()->Collision()->Layers()->TeleLayer()->m_Width*GameServer()->Collision()->Layers()->TeleLayer()->m_Height; i++)
		{
			// get the array size
			if(GameServer()->Collision()->m_pTele[i].m_Number > ArraySize)
				ArraySize = GameServer()->Collision()->m_pTele[i].m_Number;
		}
	}
	
	if(!ArraySize)
	{
		m_pTeleporter = 0x0;
		return;
	}
	
	m_pTeleporter = new vec2[ArraySize];
	mem_zero(m_pTeleporter, ArraySize*sizeof(vec2));
	
	// assign the values
	for(int i = 0; i < GameServer()->Collision()->Layers()->TeleLayer()->m_Width*GameServer()->Collision()->Layers()->TeleLayer()->m_Height; i++)
	{
		if(GameServer()->Collision()->m_pTele[i].m_Number > 0 && GameServer()->Collision()->m_pTele[i].m_Type == TILE_TELEOUT)
			m_pTeleporter[GameServer()->Collision()->m_pTele[i].m_Number-1] = vec2(i%GameServer()->Collision()->Layers()->TeleLayer()->m_Width*32+16, i/GameServer()->Collision()->Layers()->TeleLayer()->m_Width*32+16);
	}
}

int CGameControllerRACE::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	int ClientID = pVictim->GetPlayer()->GetCID();
	m_aRace[ClientID].Reset();
	
#if defined(CONF_TEERACE)
	if(Server()->IsRecording(ClientID))
		Server()->StopRecord(ClientID);
	
	if(Server()->IsGhostRecording(ClientID))
		Server()->StopGhostRecord(ClientID);
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

		if(p->m_RaceState == RACE_STARTED && Server()->Tick()-p->m_RefreshTime >= Server()->TickSpeed())
		{
			int Time = GetTime(i);

			char aBuftime[128];
			char aTmp[128];

			CNetMsg_Sv_RaceTime Msg;
			Msg.m_Time = Time / 1000;
			Msg.m_Check = 0;
			str_format(aBuftime, sizeof(aBuftime), "Current Time: %d min %d sec",
				Time / (60 * 1000), (Time / 1000) % 60);

			if(p->m_CpTick != -1 && p->m_CpTick > Server()->Tick())
			{
				Msg.m_Check = (p->m_CpDiff % 1000) / 10;
				int CpDiff = abs(p->m_CpDiff);
				str_format(aTmp, sizeof(aTmp), "\nCheckpoint | Diff : %s%3d.%02d",
					p->m_CpDiff > 0 ? "+" : "-", CpDiff / 1000, (CpDiff % 1000) / 10);
				strcat(aBuftime, aTmp);
			}

			if(GameServer()->m_apPlayers[i]->m_IsUsingRaceClient)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
			else
				GameServer()->SendBroadcast(aBuftime, i);

			p->m_RefreshTime = Server()->Tick();
		}
		
#if defined(CONF_TEERACE)
		// stop recording at the finish
		CPlayerData *pBest = GameServer()->Score()->PlayerData(i);
		if(Server()->IsRecording(i))
		{
			if(Server()->Tick() == m_aStopRecordTick[i])
			{
				m_aStopRecordTick[i] = -1;
				Server()->StopRecord(i);
				continue;
			}
			
			if(m_aRace[i].m_RaceState == RACE_STARTED && pBest->m_Time > 0 && pBest->m_Time < GetTime(i))
				Server()->StopRecord(i);
		}
		
		// stop ghost if time is bigger then best time
		if(Server()->IsGhostRecording(i) && m_aRace[i].m_RaceState == RACE_STARTED && pBest->m_Time > 0 && pBest->m_Time < GetTime(i))
			Server()->StopGhostRecord(i);
#endif
	}
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
		p->m_CpDiff = p->m_aCpCurrent[z] - pBest->m_aCpTime[z];
		p->m_CpTick = Server()->Tick() + Server()->TickSpeed()*2;
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
	p->m_StartTime = Server()->Tick();
	p->m_RefreshTime = Server()->Tick();
	p->m_StartAddTime = StartAddTime;

	if(p->m_RaceState != RACE_NONE)
	{
		// reset pickups
		if(!pChr->HasWeapon(WEAPON_GRENADE))
			GameServer()->m_apPlayers[ID]->m_ResetPickups = true;
	}

#if defined(CONF_TEERACE)
	if(g_Config.m_WaAutoRecord && GameServer()->Webapp() && Server()->GetUserID(ID) > 0 && GameServer()->Webapp()->CurrentMap()->m_ID > -1 && !Server()->IsGhostRecording(ID))
		Server()->StartGhostRecord(ID, pChr->GetPlayer()->m_TeeInfos.m_SkinName, pChr->GetPlayer()->m_TeeInfos.m_UseCustomColor, pChr->GetPlayer()->m_TeeInfos.m_ColorBody, pChr->GetPlayer()->m_TeeInfos.m_ColorFeet);
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
	str_format(aBuf, sizeof(aBuf), "%s finished in: %d minute(s) %d.%03d second(s)",
		Server()->ClientName(ID), FinishTime / (60 * 1000), (FinishTime / 1000) % 60, FinishTime % 1000);
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
	if(Server()->IsRecording(ID))
		m_aStopRecordTick[ID] = Server()->Tick()+Server()->TickSpeed();
#endif

	return true;
}

int CGameControllerRACE::GetTime(int ID)
{
	return (Server()->Tick() - m_aRace[ID].m_StartTime) * 1000 / Server()->TickSpeed();
}
