/* CSqlScore class by Sushi */
#if defined(CONF_SQL)

#include <engine/server/mysql.h>
#include <engine/shared/config.h>

#include <game/teerace.h>

#include "../gamecontext.h"
#include "sql_score.h"

// TODO: remove ip scoring?
// TODO: maybe change table structure (checkpoint storage, ...)

static LOCK gs_SqlLock = 0;

CSqlScore::CSqlScore(CGameContext *pGameServer)
	: m_pGameServer(pGameServer), m_pServer(pGameServer->Server()), m_DbExists(false), m_pSqlConfig(new CSqlConfig())
{
	str_copy(m_pSqlConfig->m_aDatabase, g_Config.m_SvSqlDatabase, sizeof(m_pSqlConfig->m_aDatabase));
	str_copy(m_pSqlConfig->m_aUser, g_Config.m_SvSqlUser, sizeof(m_pSqlConfig->m_aUser));
	str_copy(m_pSqlConfig->m_aPass, g_Config.m_SvSqlPw, sizeof(m_pSqlConfig->m_aPass));
	str_copy(m_pSqlConfig->m_aIp, g_Config.m_SvSqlIp, sizeof(m_pSqlConfig->m_aIp));
	m_pSqlConfig->m_Port = g_Config.m_SvSqlPort;

	str_copy(m_aPrefix, g_Config.m_SvSqlPrefix, sizeof(m_aPrefix));
	
	if(gs_SqlLock == 0)
		gs_SqlLock = lock_create();
	
	Init();
}

CSqlScore::~CSqlScore()
{
	lock_wait(gs_SqlLock);
	lock_unlock(gs_SqlLock);

	delete m_pSqlConfig;
}

void CSqlScore::Init()
{
	CSqlConnection Con;
	if(!Con.Connect(m_pSqlConfig, false))
		return;

	char aBuf[768];
	str_format(aBuf, sizeof(aBuf), "CREATE DATABASE IF NOT EXISTS %s", m_pSqlConfig->m_aDatabase);
	if(Con.Query(aBuf))
		m_DbExists = true;
}

void CSqlScore::OnMapLoad()
{
	IScore::OnMapLoad();

	if(!m_DbExists)
		return;

	lock_wait(gs_SqlLock);

	CSqlConnection Con;
	if(!Con.Connect(m_pSqlConfig))
		return;

	char aMap[64];
	str_copy(aMap, m_pServer->GetMapName(), sizeof(aMap));
	ClearString(aMap, sizeof(aMap));

	char aBuf[768];
	str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_%s_race (Name VARCHAR(31) NOT NULL, Time INTEGER DEFAULT 0, IP VARCHAR(16) DEFAULT '0.0.0.0', cp1 INTEGER DEFAULT 0, cp2 INTEGER DEFAULT 0, cp3 INTEGER DEFAULT 0, cp4 INTEGER DEFAULT 0, cp5 INTEGER DEFAULT 0, cp6 INTEGER DEFAULT 0, cp7 INTEGER DEFAULT 0, cp8 INTEGER DEFAULT 0, cp9 INTEGER DEFAULT 0, cp10 INTEGER DEFAULT 0, cp11 INTEGER DEFAULT 0, cp12 INTEGER DEFAULT 0, cp13 INTEGER DEFAULT 0, cp14 INTEGER DEFAULT 0, cp15 INTEGER DEFAULT 0, cp16 INTEGER DEFAULT 0, cp17 INTEGER DEFAULT 0, cp18 INTEGER DEFAULT 0, cp19 INTEGER DEFAULT 0, cp20 INTEGER DEFAULT 0, cp21 INTEGER DEFAULT 0, cp22 INTEGER DEFAULT 0, cp23 INTEGER DEFAULT 0, cp24 INTEGER DEFAULT 0, cp25 INTEGER DEFAULT 0);", m_aPrefix, aMap);
	if(!Con.Query(aBuf))
		return;

	dbg_msg("SQL", "Tables were created successfully");

	str_format(aBuf, sizeof(aBuf), "SELECT Time FROM %s_%s_race ORDER BY `Time` ASC LIMIT 0, 1;", m_aPrefix, aMap);
	CSqlResultSet *pResult = Con.QueryWithResult(aBuf);
	if(!pResult)
		return;

	if(pResult->Next())
	{
		UpdateRecord(pResult->GetInteger("Time"));
		dbg_msg("SQL", "Getting best time on server done");
	}

	delete pResult;
	lock_unlock(gs_SqlLock);
}

void CSqlScore::ExecSqlFunc(void *pUser)
{
	lock_wait(gs_SqlLock);

	CSqlExecData *pData = (CSqlExecData *)pUser;
	CSqlScore *pScore = pData->m_pScore;

	CSqlConnection Con;

	// Connect to database
	if(Con.Connect(pScore->m_pSqlConfig))
		pData->m_pFunc(&Con, false, pData->m_pUserData);
	else
		pData->m_pFunc(&Con, true, pData->m_pUserData);

	delete pData;
	lock_unlock(gs_SqlLock);
}

void CSqlScore::LoadScoreThread(CSqlConnection *pCon, bool Error, void *pUser)
{
	CScoreData *pData = (CScoreData *)pUser;
	CSqlScore *pScore = pData->m_pScore;

	if(Error)
	{
		delete pData;
		return;
	}

	bool Found = false;

	int Time = 0;
	int aCpTime[NUM_CHECKPOINTS] = { 0 };

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE IP='%s' OR Name='%s';",
		pScore->m_aPrefix, pData->m_aMap, pData->m_aIP, pData->m_aName);
	CSqlResultSet *pResult = pCon->QueryWithResult(aBuf);
	if(pResult == NULL)
	{
		delete pData;
		return;
	}

	// if ip found...
	if(g_Config.m_SvScoreIP)
	{
		while(pResult->Next())
		{
			if(str_comp(pResult->GetString("IP"), pData->m_aIP) == 0)
			{
				Found = true;
				Time = pResult->GetInteger("Time");
				if(g_Config.m_SvCheckpointSave)
				{
					int Start = pResult->GetColumnIndex("cp1");
					for(int i = 0; i < NUM_CHECKPOINTS; i++)
						aCpTime[i] = pResult->GetInteger(Start + i);
				}
				break;
			}
		}
	}

	if(!Found)
	{
		pResult->SetRowIndex(0);
		while(pResult->Next())
		{
			if(str_comp(pResult->GetString("Name"), pData->m_aName) == 0)
			{
				Found = true;
				Time = pResult->GetInteger("Time");
				if(g_Config.m_SvCheckpointSave)
				{
					int Start = pResult->GetColumnIndex("cp1");
					for(int i = 0; i < NUM_CHECKPOINTS; i++)
						aCpTime[i] = pResult->GetInteger(Start + i);
				}

				if(str_comp(pResult->GetString("IP"), pData->m_aIP) != 0)
				{
					// set the new ip
					str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET IP='%s' WHERE Name='%s';", pScore->m_aPrefix, pData->m_aMap, pData->m_aIP, pData->m_aName);
					pCon->Query(aBuf);
				}
				break;
			}
		}
	}

	delete pResult;

	if(Found)
	{
		pScore->m_aPlayerData[pData->m_ClientID].SetTime(Time, aCpTime);
		if(g_Config.m_SvShowBest)
		{
			pScore->m_aPlayerData[pData->m_ClientID].UpdateCurTime(Time);
			int SendTo = g_Config.m_SvShowTimes ? -1 : pData->m_ClientID;
			pScore->GameServer()->SendPlayerTime(SendTo, Time, pData->m_ClientID);
		}
	}
			
	dbg_msg("SQL", "Getting best time done");

	delete pData;
}

void CSqlScore::OnPlayerInit(int ClientID, bool PrintRank)
{
	m_aPlayerData[ClientID].Reset();

	if(!m_DbExists)
		return;

	CScoreData *Tmp = new CScoreData(this);
	str_copy(Tmp->m_aMap, m_pServer->GetMapName(), sizeof(Tmp->m_aMap));
	ClearString(Tmp->m_aMap, sizeof(Tmp->m_aMap));
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), sizeof(Tmp->m_aName));
	ClearString(Tmp->m_aName, sizeof(Tmp->m_aName));
	Server()->GetClientAddr(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));
	
	void *LoadThread = thread_init(ExecSqlFunc, new CSqlExecData(this, LoadScoreThread, Tmp));
	thread_detach(LoadThread);
}

void CSqlScore::SaveScoreThread(CSqlConnection *pCon, bool Error, void *pUser)
{
	CScoreData *pData = (CScoreData *)pUser;
	CSqlScore *pScore = pData->m_pScore;
	
	if(Error)
	{
		delete pData;
		return;
	}

	bool Found = false;
	char aName[32] = { 0 };

	char aBuf[768];
	str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE IP='%s' OR Name='%s';",
		pScore->m_aPrefix, pData->m_aMap, pData->m_aIP, pData->m_aName);
	CSqlResultSet *pResult = pCon->QueryWithResult(aBuf);
	if(pResult == NULL)
	{
		delete pData;
		return;
	}

	if(g_Config.m_SvScoreIP)
	{
		while(pResult->Next())
		{
			if(str_comp(pResult->GetString("IP"), pData->m_aIP) == 0)
			{
				Found = true;
				str_copy(aName, pResult->GetString("Name"), sizeof(aName));
				break;
			}
		}
	}

	if(!Found)
	{
		pResult->SetRowIndex(0);
		while(pResult->Next())
		{
			if(str_comp(pResult->GetString("Name"), pData->m_aName) == 0)
			{
				Found = true;
				str_copy(aName, pResult->GetString("Name"), sizeof(aName));
				break;
			}
		}
	}

	delete pResult;

	if(Found)
	{
		if(g_Config.m_SvCheckpointSave)
			str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET Name='%s', Time='%d', cp1='%d', cp2='%d', cp3='%d', cp4='%d', cp5='%d', cp6='%d', cp7='%d', cp8='%d', cp9='%d', cp10='%d', cp11='%d', cp12='%d', cp13='%d', cp14='%d', cp15='%d', cp16='%d', cp17='%d', cp18='%d', cp19='%d', cp20='%d', cp21='%d', cp22='%d', cp23='%d', cp24='%d', cp25='%d' WHERE IP='%s';", pScore->m_aPrefix, pData->m_aMap, pData->m_aName, pData->m_Time, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24], pData->m_aIP);
		else
			str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET Name='%s', Time='%d' WHERE Name='%s';", pScore->m_aPrefix, pData->m_aMap, pData->m_aName, pData->m_Time, aName);
		pCon->Query(aBuf);
	}
	else
	{
		if(g_Config.m_SvCheckpointSave)
			str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_%s_race(Name, IP, Time, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25) VALUES ('%s', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d');", pScore->m_aPrefix, pData->m_aMap, pData->m_aName, pData->m_aIP, pData->m_Time, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24]);
		else
			str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_%s_race(Name, IP, Time) VALUES ('%s', '%s', '%d');", pScore->m_aPrefix, pData->m_aMap, pData->m_aName, pData->m_aIP, pData->m_Time);
		pCon->Query(aBuf);
	}
			
	dbg_msg("SQL", "Updating time done");

	delete pData;
}

void CSqlScore::OnPlayerFinish(int ClientID, int Time, int *pCpTime)
{
	bool NewPlayerRecord = m_aPlayerData[ClientID].UpdateTime(Time, pCpTime);
	if(UpdateRecord(Time) && g_Config.m_SvShowTimes)
		GameServer()->SendRecord(-1);

	if(!m_DbExists || !NewPlayerRecord)
		return;

	CScoreData *Tmp = new CScoreData(this);
	str_copy(Tmp->m_aMap, m_pServer->GetMapName(), sizeof(Tmp->m_aMap));
	ClearString(Tmp->m_aMap, sizeof(Tmp->m_aMap));
	Tmp->m_ClientID = ClientID;
	Tmp->m_Time = Time;
	mem_copy(Tmp->m_aCpCurrent, pCpTime, sizeof(Tmp->m_aCpCurrent));
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), sizeof(Tmp->m_aName));
	ClearString(Tmp->m_aName, sizeof(Tmp->m_aName));
	Server()->GetClientAddr(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));
	
	void *SaveThread = thread_init(ExecSqlFunc, new CSqlExecData(this, SaveScoreThread, Tmp));
	thread_detach(SaveThread);
}

void CSqlScore::ShowRankThread(CSqlConnection *pCon, bool Error, void *pUser)
{
	CScoreData *pData = (CScoreData *)pUser;
	CSqlScore *pScore = pData->m_pScore;
	
	if(Error)
	{
		delete pData;
		return;
	}

	// TODO: search function is incomplete

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "SELECT Name, IP, Time FROM %s_%s_race ORDER BY `Time` ASC;", pScore->m_aPrefix, pData->m_aMap);
	CSqlResultSet *pResult = pCon->QueryWithResult(aBuf);
	if(pResult == NULL)
	{
		delete pData;
		return;
	}

	int RowCount = 0;
	bool Found = false;

	if(!pData->m_Search && g_Config.m_SvScoreIP)
	{
		while(pResult->Next())
		{
			RowCount++;
			if(str_comp(pResult->GetString("IP"), pData->m_aIP) == 0)
			{
				Found = true;
				break;
			}
		}
	}

	if(!Found)
	{
		pResult->SetRowIndex(0);
		while(pResult->Next())
		{
			RowCount++;
			//if(str_find_nocase(ppRow[0], pData->m_aName))
			if(str_comp(pResult->GetString("Name"), pData->m_aName) == 0)
			{
				Found = true;
				break;
			}
		}
	}

	bool Public = false;

	if(Found)
	{
		Public = g_Config.m_SvShowTimes;
		char aTime[64];
		IRace::FormatTimeLong(aTime, sizeof(aTime), pResult->GetInteger("Time"));
		if(!Public)
			str_format(aBuf, sizeof(aBuf), "Your time: %s", aTime);
		else
			str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", RowCount, pResult->GetString("Name"), aTime);
		if(pData->m_Search)
			str_append(aBuf, pData->m_aRequestingPlayer, sizeof(aBuf));
	}
	else
		str_format(aBuf, sizeof(aBuf), "%s is not ranked", pData->m_aName);

	if(Public)
		pScore->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	else
		pScore->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
			
	dbg_msg("SQL", "Showing rank done");
	
	delete pResult;
	delete pData;
}

void CSqlScore::ShowRank(int ClientID, const char *pName, bool Search)
{
	if(!m_DbExists)
		return;

	CScoreData *Tmp = new CScoreData(this);
	str_copy(Tmp->m_aMap, m_pServer->GetMapName(), sizeof(Tmp->m_aMap));
	ClearString(Tmp->m_aMap, sizeof(Tmp->m_aMap));
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, pName, sizeof(Tmp->m_aName));
	ClearString(Tmp->m_aName, sizeof(Tmp->m_aName));
	Server()->GetClientAddr(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));
	Tmp->m_Search = Search;
	str_format(Tmp->m_aRequestingPlayer, sizeof(Tmp->m_aRequestingPlayer), " (%s)", Server()->ClientName(ClientID));
	
	void *RankThread = thread_init(ExecSqlFunc, new CSqlExecData(this, ShowRankThread, Tmp));
	thread_detach(RankThread);
}

void CSqlScore::ShowTop5Thread(CSqlConnection *pCon, bool Error, void *pUser)
{
	CScoreData *pData = (CScoreData *)pUser;
	CSqlScore *pScore = pData->m_pScore;
	
	if(Error)
	{
		delete pData;
		return;
	}

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "SELECT Name, Time FROM %s_%s_race ORDER BY `Time` ASC LIMIT %d, 5;", pScore->m_aPrefix, pData->m_aMap, pData->m_Num - 1);
	CSqlResultSet *pResult = pCon->QueryWithResult(aBuf);
	if(pResult == NULL)
	{
		delete pData;
		return;
	}

	int Rank = pData->m_Num;

	pScore->GameServer()->SendChatTarget(pData->m_ClientID, "----------- Top 5 -----------");
	while(pResult->Next())
	{
		char aTime[64];
		IRace::FormatTimeLong(aTime, sizeof(aTime), pResult->GetInteger("Time"));
		str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", Rank, pResult->GetString("Name"), aTime);
		pScore->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
		Rank++;
	}
	pScore->GameServer()->SendChatTarget(pData->m_ClientID, "------------------------------");

	dbg_msg("SQL", "Showing top5 done");

	delete pResult;
	delete pData;
}

void CSqlScore::ShowTop5(int ClientID, int Debut)
{
	if(!m_DbExists)
		return;

	CScoreData *Tmp = new CScoreData(this);
	str_copy(Tmp->m_aMap, m_pServer->GetMapName(), sizeof(Tmp->m_aMap));
	ClearString(Tmp->m_aMap, sizeof(Tmp->m_aMap));
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;
	
	void *Top5Thread = thread_init(ExecSqlFunc, new CSqlExecData(this, ShowTop5Thread, Tmp));
	thread_detach(Top5Thread);
}

// anti SQL injection
void CSqlScore::ClearString(char *pString, int Size)
{
	// check if the string is long enough to escape something
	if(Size <= 2)
	{
		if(pString[0] == '\'' || pString[0] == '\\' || pString[0] == ';')
			pString[0] = '_';
		return;
	}
	
	// replace ' ' ' with ' \' ' and remove '\'
	for(int i = 0; i < str_length(pString); i++)
	{
		// replace '-' with '_'
		if(pString[i] == '-')
		{
			pString[i] = '_';
			continue;
		}
		
		// escape ', \ and ;
		if(pString[i] == '\'' || pString[i] == '\\' || pString[i] == ';')
		{
			for(int j = Size-2; j > i; j--)
				pString[j] = pString[j-1];
			pString[i] = '\\';
			i++; // so we dont double escape
			continue;
		}
	}

	// aaand remove spaces and \ at the end xD
	for(int i = str_length(pString)-1; i >= 0; i--)
	{
		if(pString[i] == ' ' || pString[i] == '\\')
			pString[i] = 0;
		else
			break;
	}
}

#endif
