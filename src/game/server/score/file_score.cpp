/* copyright (c) 2008 rajh and gregwar. Score stuff */

#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <game/teerace.h>

#include "../gamecontext.h"
#include "file_score.h"

// TODO: remove score ip or use ip hash

static LOCK gs_ScoreLock = 0;

CFileScore::CPlayerScore::CPlayerScore(const char *pName, int Time, const char *pIP, int *pCpTime)
{
	m_Time = Time;
	mem_copy(m_aCpTime, pCpTime, sizeof(m_aCpTime));
	str_copy(m_aName, pName, sizeof(m_aName));
	str_copy(m_aIP, pIP, sizeof(m_aIP));
}

CFileScore::CFileScore(CGameContext *pGameServer) : m_pGameServer(pGameServer), m_pServer(pGameServer->Server())
{
	if(gs_ScoreLock == 0)
		gs_ScoreLock = lock_create();
		
	Init();
}

CFileScore::~CFileScore()
{
	lock_wait(gs_ScoreLock);
	
	// clear list
	m_Top.clear();
	
	lock_unlock(gs_ScoreLock);
}

void CFileScore::WriteEntry(IOHANDLE File, const CPlayerScore *pEntry) const
{
#if defined(CONF_FAMILY_WINDOWS)
	const char* pNewLine = "\r\n";
#else
	const char* pNewLine = "\n";
#endif

	char aBuf[1024] = { 0 };
	char aBuf2[128];
	str_append(aBuf, pEntry->m_aName, sizeof(aBuf));
	str_append(aBuf, pNewLine, sizeof(aBuf));
	str_format(aBuf2, sizeof(aBuf2), "%d", pEntry->m_Time);
	str_append(aBuf, aBuf2, sizeof(aBuf));
	str_append(aBuf, pNewLine, sizeof(aBuf));
	str_append(aBuf, pEntry->m_aIP, sizeof(aBuf));
	str_append(aBuf, pNewLine, sizeof(aBuf));
	if(g_Config.m_SvCheckpointSave)
	{
		for(int c = 0; c < NUM_CHECKPOINTS; c++)
		{
			str_format(aBuf2, sizeof(aBuf2), "%d ", pEntry->m_aCpTime[c]);
			str_append(aBuf, aBuf2, sizeof(aBuf));
		}
		str_append(aBuf, pNewLine, sizeof(aBuf));
	}
	io_write(File, aBuf, str_length(aBuf));
}

IOHANDLE CFileScore::OpenFile(int Flags) const
{
	char aFilename[256];
	str_format(aFilename, sizeof(aFilename), "records/%s_record.dtb", m_pServer->GetMapName());
	return m_pGameServer->Storage()->OpenFile(aFilename, Flags, IStorage::TYPE_SAVE);
}

void CFileScore::SaveScoreThread(void *pUser)
{
	CFileScore *pSelf = (CFileScore *)pUser;
	lock_wait(gs_ScoreLock);
	IOHANDLE File = pSelf->OpenFile(IOFLAG_WRITE);
	if(File)
	{
		int t = 0;
		for(sorted_array<CPlayerScore>::range r = pSelf->m_Top.all(); !r.empty(); r.pop_front())
		{
			pSelf->WriteEntry(File, &r.front());
			t++;
			if(t%50 == 0)
				thread_sleep(1);
		}
		io_close(File);
	}
	lock_unlock(gs_ScoreLock);
}

void CFileScore::Save()
{
	void *pSaveThread = thread_init(SaveScoreThread, this);
	thread_detach(pSaveThread);
}

void CFileScore::Init()
{
	lock_wait(gs_ScoreLock);
	IOHANDLE File = OpenFile(IOFLAG_READ);
	if(File)
	{
		CLineReader LineReader;
		LineReader.Init(File);
		CPlayerScore Tmp;
		int LinesPerItem = g_Config.m_SvCheckpointSave ? 4 : 3;
		char *pLine;
		for(int LineCount = 0; (pLine = LineReader.Get()); LineCount++)
		{
			if(str_length(pLine) == 0)
				break;

			int Type = LineCount % LinesPerItem;
			if(Type == 0)
			{
				mem_zero(&Tmp, sizeof(Tmp));
				str_copy(Tmp.m_aName, pLine, sizeof(Tmp.m_aName));
			}
			else if(Type == 1)
			{
				Tmp.m_Time = str_toint(pLine);
			}
			else if(Type == 2)
			{
				str_copy(Tmp.m_aIP, pLine, sizeof(Tmp.m_aIP));
				if(!g_Config.m_SvCheckpointSave)
					m_Top.add(Tmp);
			}
			else if(Type == 3)
			{
				const char *pTime = pLine;
				for(int i = 0; pTime && i < NUM_CHECKPOINTS; i++)
				{
					Tmp.m_aCpTime[i] = str_toint(pTime);
					pTime = str_find(pTime, " ");
					if(pTime) pTime++;
				}
				m_Top.add(Tmp);
			}
		}
		io_close(File);
	}
	lock_unlock(gs_ScoreLock);

	// save the current best score
	if(m_Top.size())
		GetRecord()->Set(m_Top[0].m_Time, m_Top[0].m_aCpTime);
}

CFileScore::CPlayerScore *CFileScore::SearchScoreByID(int ID, int *pPosition)
{
	if(g_Config.m_SvScoreIP)
	{
		char aIP[16];
		Server()->GetClientAddr(ID, aIP, sizeof(aIP));
	
		int Pos = 1;
		for(sorted_array<CPlayerScore>::range r = m_Top.all(); !r.empty(); r.pop_front())
		{
			if(str_comp(r.front().m_aIP, aIP) == 0)
			{
				if(pPosition)
					*pPosition = Pos;
				return &r.front();
			}
			Pos++;
		}
	}
	
	return SearchScoreByName(Server()->ClientName(ID), pPosition, true);
}

CFileScore::CPlayerScore *CFileScore::SearchScoreByName(const char *pName, int *pPosition, bool ExactMatch)
{
	CPlayerScore *pPlayer = 0;
	int Pos = 1;
	int Found = 0;
	for(sorted_array<CPlayerScore>::range r = m_Top.all(); !r.empty(); r.pop_front())
	{
		if(str_comp(r.front().m_aName, pName) == 0)
		{
			if(pPosition)
				*pPosition = Pos;
			return &r.front();
		}
		if(!ExactMatch && str_find_nocase(r.front().m_aName, pName))
		{
			if(pPosition)
				*pPosition = Pos;
			Found++;
			pPlayer = &r.front();
		}
		Pos++;
	}
	if(Found > 1)
	{
		if(pPosition)
			*pPosition = -1;
		return 0;
	}
	return pPlayer;
}

void CFileScore::LoadScore(int ClientID, bool PrintRank)
{
	char aIP[16];
	Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
	CPlayerScore *pPlayer = SearchScoreByID(ClientID);
	if(pPlayer && str_comp(pPlayer->m_aIP, aIP) != 0)
	{
		lock_wait(gs_ScoreLock);
		str_copy(pPlayer->m_aIP, aIP, sizeof(pPlayer->m_aIP));
		lock_unlock(gs_ScoreLock);
		Save();
	}
	
	// set score
	if(pPlayer)
	{
		PlayerData(ClientID)->Set(pPlayer->m_Time, pPlayer->m_aCpTime);
		if(g_Config.m_SvShowBest)
		{
			CPlayer *pPl = GameServer()->m_apPlayers[ClientID];
			PlayerData(ClientID)->m_CurTime = pPlayer->m_Time;
			pPl->m_Score = max(-(pPlayer->m_Time / 1000), pPl->m_Score);

			if(g_Config.m_SvShowTimes)
				GameServer()->SendPlayerTime(-1, pPlayer->m_Time, ClientID);
			else
				GameServer()->SendPlayerTime(ClientID, pPlayer->m_Time, ClientID);
		}
	}
}

void CFileScore::SaveScore(int ClientID, int Time, int *pCpTime, bool NewRecord)
{
	if(!NewRecord)
		return;

	const char *pName = Server()->ClientName(ClientID);
	char aIP[16];
	Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));

	lock_wait(gs_ScoreLock);
	CPlayerScore *pPlayer = SearchScoreByID(ClientID);

	if(pPlayer)
	{
		pPlayer->m_Time = PlayerData(ClientID)->m_Time;
		mem_copy(pPlayer->m_aCpTime, PlayerData(ClientID)->m_aCpTime, sizeof(pPlayer->m_aCpTime));
		str_copy(pPlayer->m_aName, pName, sizeof(pPlayer->m_aName));

		sort(m_Top.all());
	}
	else
		m_Top.add(CPlayerScore(pName, PlayerData(ClientID)->m_Time, aIP, PlayerData(ClientID)->m_aCpTime));

	lock_unlock(gs_ScoreLock);
	Save();
}

void CFileScore::ShowTop5(int ClientID, int Debut)
{
	char aBuf[512];
	char aTime[64];
	GameServer()->SendChatTarget(ClientID, "----------- Top 5 -----------");
	for(int i = 0; i < 5 && i + Debut - 1 < m_Top.size(); i++)
	{
		const CPlayerScore *r = &m_Top[i+Debut-1];
		IRace::FormatTimeLong(aTime, sizeof(aTime), r->m_Time);
		str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s",
			i + Debut, r->m_aName, aTime);
		GameServer()->SendChatTarget(ClientID, aBuf);
	}
	GameServer()->SendChatTarget(ClientID, "------------------------------");
}

void CFileScore::ShowRank(int ClientID, const char *pName, bool Search)
{
	const CPlayerScore *pScore = 0;
	int Pos = 0;
	char aBuf[512];

	if(!Search)
		pScore = SearchScoreByID(ClientID, &Pos);
	else
		pScore = SearchScoreByName(pName, &Pos, false);

	bool Public = false;

	if(pScore && Pos > -1)
	{
		Public = g_Config.m_SvShowTimes;
		char aTime[64];
		IRace::FormatTimeLong(aTime, sizeof(aTime), pScore->m_Time);
		if(!Public)
			str_format(aBuf, sizeof(aBuf), "Your time: %s", aTime);
		else
			str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", Pos, pScore->m_aName, aTime);
		if(Search)
		{
			char aClientName[128];
			str_format(aClientName, sizeof(aClientName), " (%s)", Server()->ClientName(ClientID));
			str_append(aBuf, aClientName, sizeof(aBuf));
		}
	}
	else if(Pos == -1)
		str_format(aBuf, sizeof(aBuf), "Several players were found.");
	else
		str_format(aBuf, sizeof(aBuf), "%s is not ranked", pName);

	if(Public)
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	else
		GameServer()->SendChatTarget(ClientID, aBuf);
}
