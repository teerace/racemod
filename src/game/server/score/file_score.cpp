/* copyright (c) 2008 rajh and gregwar. Score stuff */

#include <engine/shared/config.h>
#include <sstream>
#include <fstream>

#include "../gamecontext.h"
#include "file_score.h"

static LOCK gs_ScoreLock = 0;

CFileScore::CPlayerScore::CPlayerScore(const char *pName, float Time, const char *pIP, float *pCpTime)
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

std::string SaveFile()
{
	std::ostringstream oss;
	if(g_Config.m_SvScoreFolder[0])
		oss << g_Config.m_SvScoreFolder << "/" << g_Config.m_SvMap << "_record.dtb";
	else
		oss << g_Config.m_SvMap << "_record.dtb";
	return oss.str();
}

void CFileScore::SaveScoreThread(void *pUser)
{
	CFileScore *pSelf = (CFileScore *)pUser;
	lock_wait(gs_ScoreLock);
	std::fstream f;
	f.open(SaveFile().c_str(), std::ios::out);
	if(!f.fail())
	{
		int t = 0;
		for(sorted_array<CPlayerScore>::range r = pSelf->m_Top.all(); !r.empty(); r.pop_front())
		{
			f << r.front().m_aName << std::endl << r.front().m_Time << std::endl  << r.front().m_aIP << std::endl;
			if(g_Config.m_SvCheckpointSave)
			{
				for(int c = 0; c < NUM_CHECKPOINTS; c++)
					f << r.front().m_aCpTime[c] << " ";
				f << std::endl;
			}
			t++;
			if(t%50 == 0)
				thread_sleep(1);
		}
	}
	f.close();
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
	
	// create folder if not exist
	if(g_Config.m_SvScoreFolder[0])
		fs_makedir(g_Config.m_SvScoreFolder);
	
	std::fstream f;
	f.open(SaveFile().c_str(), std::ios::in);
	
	while(!f.eof() && !f.fail())
	{
		std::string TmpName, TmpScore, TmpIP, TmpCpLine;
		std::getline(f, TmpName);
		if(!f.eof() && TmpName != "")
		{
			std::getline(f, TmpScore);
			std::getline(f, TmpIP);
			float aTmpCpTime[NUM_CHECKPOINTS] = {0};
			if(g_Config.m_SvCheckpointSave)
			{
				std::getline(f, TmpCpLine);
				char *pTime = strtok((char*)TmpCpLine.c_str(), " ");
				int i = 0;
				while(pTime != NULL && i < NUM_CHECKPOINTS)
				{
					aTmpCpTime[i] = atof(pTime);
					pTime = strtok(NULL, " ");
					i++;
				}
			}
			m_Top.add(*new CPlayerScore(TmpName.c_str(), atof(TmpScore.c_str()), TmpIP.c_str(), aTmpCpTime));
		}
	}
	f.close();
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
		if(!strcmp(r.front().m_aIP, aIP) && g_Config.m_SvScoreIP && ScoreIP)
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
			if(!strcmp(r.front().m_aName, pName))
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

void CFileScore::LoadScore(int ClientID)
{
	char aIP[16];
	Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
	CPlayerScore *pPlayer = SearchScore(ClientID, 0, 0);
	if(pPlayer && strcmp(pPlayer->m_aIP, aIP) != 0)
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

void CFileScore::SaveScore(int ClientID, float Time, float *pCpTime, bool NewRecord)
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
		m_Top.add(*new CPlayerScore(pName, PlayerData(ClientID)->m_Time, aIP, PlayerData(ClientID)->m_aCpTime));

	lock_release(gs_ScoreLock);
	Save();
}

void CFileScore::ShowTop5(int ClientID, int Debut)
{
	char aBuf[512];
	GameServer()->SendChatTarget(ClientID, "----------- Top 5 -----------");
	for(int i = 0; i < 5; i++)
	{
		if(i+Debut > m_Top.size())
			break;
		CPlayerScore *r = &m_Top[i+Debut-1];
		str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %.3f second(s)",
			i+Debut, r->m_aName, (int)r->m_Time/60, fmod(r->m_Time,60));
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
		float Time = pScore->m_Time;
		char aClientName[128];
		str_format(aClientName, sizeof(aClientName), " (%s)", Server()->ClientName(ClientID));
		if(!g_Config.m_SvShowTimes)
			str_format(aBuf, sizeof(aBuf), "Your time: %d minute(s) %.3f second(s)", (int)Time/60, fmod(Time,60));
		else
			str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %.3f second(s)", Pos, pScore->m_aName, (int)Time/60, fmod(Time,60));
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
