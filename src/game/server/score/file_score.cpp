/* copyright (c) 2008 rajh and gregwar. Score stuff */

#include <string.h>

#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

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
	
	lock_release(gs_ScoreLock);
}

void CFileScore::WriteLine(IOHANDLE File, const char *pLine)
{
	io_write(File, pLine, str_length(pLine));
	io_write_newline(File);
}

IOHANDLE CFileScore::OpenFile(int Flags)
{
	char aFilename[256];
	str_format(aFilename, sizeof(aFilename), "records/%s_record.dtb", g_Config.m_SvMap);
	return m_pServer->Storage()->OpenFile(aFilename, Flags, IStorage::TYPE_SAVE);
}

void CFileScore::SaveScoreThread(void *pUser)
{
	CFileScore *pSelf = (CFileScore *)pUser;
	lock_wait(gs_ScoreLock);
	IOHANDLE File = pSelf->OpenFile(IOFLAG_WRITE);
	if(File)
	{
		int t = 0;
		char aBuf[128];
		for(sorted_array<CPlayerScore>::range r = pSelf->m_Top.all(); !r.empty(); r.pop_front())
		{
			pSelf->WriteLine(File, r.front().m_aName);
			str_format(aBuf, sizeof(aBuf), "%d", r.front().m_Time);
			pSelf->WriteLine(File, aBuf);
			pSelf->WriteLine(File, r.front().m_aIP);
			if(g_Config.m_SvCheckpointSave)
			{
				for(int c = 0; c < NUM_CHECKPOINTS; c++)
				{
					str_format(aBuf, sizeof(aBuf), "%d ", r.front().m_aCpTime[c]);
					io_write(File, aBuf, str_length(aBuf));
				}
				io_write_newline(File);
			}
			t++;
			if(t%50 == 0)
				thread_sleep(1);
		}
		io_close(File);
	}
	lock_release(gs_ScoreLock);
}

void CFileScore::Save()
{
	void *pSaveThread = thread_create(SaveScoreThread, this);
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
		int LineCount = 0;
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
				char aBuf[256];
				str_copy(aBuf, pLine, sizeof(aBuf));
				char *pTime = strtok(aBuf, " ");
				for(int i = 0; pTime != NULL && i < NUM_CHECKPOINTS; i++)
				{
					Tmp.m_aCpTime[i] = str_toint(pTime);
					pTime = strtok(NULL, " ");
				}
				m_Top.add(Tmp);
			}
		}
		io_close(File);
	}
	lock_release(gs_ScoreLock);

	// save the current best score
	if(m_Top.size())
		GetRecord()->Set(m_Top[0].m_Time, m_Top[0].m_aCpTime);
}

CFileScore::CPlayerScore *CFileScore::SearchScore(int ID, bool ScoreIP, int *pPosition)
{
	char aIP[16];
	Server()->GetClientAddr(ID, aIP, sizeof(aIP));
	
	int Pos = 1;
	for(sorted_array<CPlayerScore>::range r = m_Top.all(); !r.empty(); r.pop_front())
	{
		if(!str_comp(r.front().m_aIP, aIP) && g_Config.m_SvScoreIP && ScoreIP)
		{
			if(pPosition)
				*pPosition = Pos;
			return &r.front();
		}
		Pos++;
	}
	
	return SearchName(Server()->ClientName(ID), pPosition, 0);
}

CFileScore::CPlayerScore *CFileScore::SearchName(const char *pName, int *pPosition, bool NoCase)
{
	CPlayerScore *pPlayer = 0;
	int Pos = 1;
	int Found = 0;
	for(sorted_array<CPlayerScore>::range r = m_Top.all(); !r.empty(); r.pop_front())
	{
		if(str_find_nocase(r.front().m_aName, pName))
		{
			if(pPosition)
				*pPosition = Pos;
			if(NoCase)
			{
				Found++;
				pPlayer = &r.front();
			}
			if(!str_comp(r.front().m_aName, pName))
				return &r.front();
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
	CPlayerScore *pPlayer = SearchScore(ClientID, 0, 0);
	if(pPlayer && str_comp(pPlayer->m_aIP, aIP) != 0)
	{
		lock_wait(gs_ScoreLock);
		str_copy(pPlayer->m_aIP, aIP, sizeof(pPlayer->m_aIP));
		lock_release(gs_ScoreLock);
		Save();
	}
	
	// set score
	if(pPlayer)
		PlayerData(ClientID)->Set(pPlayer->m_Time, pPlayer->m_aCpTime);
}

void CFileScore::SaveScore(int ClientID, int Time, int *pCpTime, bool NewRecord)
{
	if(!NewRecord)
		return;

	const char *pName = Server()->ClientName(ClientID);
	char aIP[16];
	Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));

	lock_wait(gs_ScoreLock);
	CPlayerScore *pPlayer = SearchScore(ClientID, 1, 0);

	if(pPlayer)
	{
		pPlayer->m_Time = PlayerData(ClientID)->m_Time;
		mem_copy(pPlayer->m_aCpTime, PlayerData(ClientID)->m_aCpTime, sizeof(pPlayer->m_aCpTime));
		str_copy(pPlayer->m_aName, pName, sizeof(pPlayer->m_aName));

		sort(m_Top.all());
	}
	else
		m_Top.add(CPlayerScore(pName, PlayerData(ClientID)->m_Time, aIP, PlayerData(ClientID)->m_aCpTime));

	lock_release(gs_ScoreLock);
	Save();
}

void CFileScore::ShowTop5(int ClientID, int Debut)
{
	char aBuf[512];
	GameServer()->SendChatTarget(ClientID, "----------- Top 5 -----------");
	for(int i = 0; i < 5 && i + Debut - 1 < m_Top.size(); i++)
	{
		CPlayerScore *r = &m_Top[i+Debut-1];
		str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %d.%03d second(s)",
			i + Debut, r->m_aName, r->m_Time / (60 * 1000), (r->m_Time / 1000) % 60, r->m_Time % 1000);
		GameServer()->SendChatTarget(ClientID, aBuf);
	}
	GameServer()->SendChatTarget(ClientID, "------------------------------");
}

void CFileScore::ShowRank(int ClientID, const char *pName, bool Search)
{
	CPlayerScore *pScore;
	int Pos;
	char aBuf[512];
	
	if(!Search)
		pScore = SearchScore(ClientID, 1, &Pos);
	else
		pScore = SearchName(pName, &Pos, 1);
	
	if(pScore && Pos > -1)
	{
		int Time = pScore->m_Time;
		char aClientName[128];
		str_format(aClientName, sizeof(aClientName), " (%s)", Server()->ClientName(ClientID));
		if(!g_Config.m_SvShowTimes)
			str_format(aBuf, sizeof(aBuf), "Your time: %d minute(s) %d.%03d second(s)",
				Time / (60 * 1000), (Time / 1000) % 60, Time % 1000);
		else
			str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %d.%03d second(s)",
				Pos, pScore->m_aName, Time / (60 * 1000), (Time / 1000) % 60, Time % 1000);
		if(Search)
			strcat(aBuf, aClientName);
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		return;
	}
	else if(Pos == -1)
		str_format(aBuf, sizeof(aBuf), "Several players were found.");
	else
		str_format(aBuf, sizeof(aBuf), "%s is not ranked", Search?pName:Server()->ClientName(ClientID));
	
	GameServer()->SendChatTarget(ClientID, aBuf);
}
