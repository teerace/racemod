/* CSqlScore class by Sushi */
#if defined(CONF_SQL)

#include <base/tl/pointer.h>

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
	lock_destroy(gs_SqlLock);

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

void CSqlScore::StartSqlThread(CSqlExecData::FQueryFunc pFunc, void *pUserData)
{
	void *Thread = thread_init(ExecSqlFunc, new CSqlExecData(this, pFunc, pUserData));
	thread_detach(Thread);
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
	smart_ptr<CSqlResultSet> Result(Con.QueryWithResult(aBuf));
	if(!Result.get())
		return;

	if(Result->Next())
	{
		UpdateRecord(Result->GetInteger("Time"));
		dbg_msg("SQL", "Getting best time on server done");
	}

	lock_unlock(gs_SqlLock);
}

void CSqlScore::ExecSqlFunc(void *pUser)
{
	lock_wait(gs_SqlLock);

	smart_ptr<CSqlExecData> Data((CSqlExecData *)pUser);
	CSqlScore *pScore = Data->m_pScore;

	CSqlConnection Con;

	// Connect to database
	if(Con.Connect(pScore->m_pSqlConfig))
		Data->m_pFunc(&Con, false, Data->m_pUserData);
	else
		Data->m_pFunc(&Con, true, Data->m_pUserData);

	lock_unlock(gs_SqlLock);
}

void CSqlScore::LoadScoreThread(CSqlConnection *pCon, bool Error, void *pUser)
{
	smart_ptr<CScoreData> Data((CScoreData *)pUser);
	CSqlScore *pScore = Data->m_pScore;

	if(Error)
		return;

	bool Found = false;

	int Time = 0;
	int aCpTime[NUM_CHECKPOINTS] = { 0 };

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE IP='%s' OR Name='%s';",
		pScore->m_aPrefix, Data->m_aMap, Data->m_aIP, Data->m_aName);
	smart_ptr<CSqlResultSet> Result(pCon->QueryWithResult(aBuf));
	if(!Result.get())
		return;

	// if ip found...
	if(g_Config.m_SvScoreIP)
	{
		while(Result->Next())
		{
			if(str_comp(Result->GetString("IP"), Data->m_aIP) == 0)
			{
				Found = true;
				Time = Result->GetInteger("Time");
				if(g_Config.m_SvCheckpointSave)
				{
					int Start = Result->GetColumnIndex("cp1");
					for(int i = 0; i < NUM_CHECKPOINTS; i++)
						aCpTime[i] = Result->GetInteger(Start + i);
				}
				break;
			}
		}
	}

	if(!Found)
	{
		Result->SetRowIndex(0);
		while(Result->Next())
		{
			if(str_comp(Result->GetString("Name"), Data->m_aName) == 0)
			{
				Found = true;
				Time = Result->GetInteger("Time");
				if(g_Config.m_SvCheckpointSave)
				{
					int Start = Result->GetColumnIndex("cp1");
					for(int i = 0; i < NUM_CHECKPOINTS; i++)
						aCpTime[i] = Result->GetInteger(Start + i);
				}

				if(str_comp(Result->GetString("IP"), Data->m_aIP) != 0)
				{
					// set the new ip
					str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET IP='%s' WHERE Name='%s';",
						pScore->m_aPrefix, Data->m_aMap, Data->m_aIP, Data->m_aName);
					pCon->Query(aBuf);
				}
				break;
			}
		}
	}

	if(Found)
	{
		pScore->m_aPlayerData[Data->m_ClientID].SetTime(Time, aCpTime);
		if(g_Config.m_SvShowBest)
		{
			pScore->m_aPlayerData[Data->m_ClientID].UpdateCurTime(Time);
			int SendTo = g_Config.m_SvShowTimes ? -1 : Data->m_ClientID;
			pScore->GameServer()->SendPlayerTime(SendTo, Time, Data->m_ClientID);
		}
	}
			
	dbg_msg("SQL", "Getting best time done");
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
	
	StartSqlThread(LoadScoreThread, Tmp);
}

void CSqlScore::SaveScoreThread(CSqlConnection *pCon, bool Error, void *pUser)
{
	smart_ptr<CScoreData> Data((CScoreData *)pUser);
	CSqlScore *pScore = Data->m_pScore;
	
	if(Error)
		return;

	bool Found = false;
	char aName[32] = { 0 };

	char aBuf[768];
	str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE IP='%s' OR Name='%s';",
		pScore->m_aPrefix, Data->m_aMap, Data->m_aIP, Data->m_aName);
	smart_ptr<CSqlResultSet> Result(pCon->QueryWithResult(aBuf));
	if(!Result.get())
		return;

	if(g_Config.m_SvScoreIP)
	{
		while(Result->Next())
		{
			if(str_comp(Result->GetString("IP"), Data->m_aIP) == 0)
			{
				Found = true;
				str_copy(aName, Result->GetString("Name"), sizeof(aName));
				break;
			}
		}
	}

	if(!Found)
	{
		Result->SetRowIndex(0);
		while(Result->Next())
		{
			if(str_comp(Result->GetString("Name"), Data->m_aName) == 0)
			{
				Found = true;
				str_copy(aName, Result->GetString("Name"), sizeof(aName));
				break;
			}
		}
	}

	if(Found)
	{
		if(g_Config.m_SvCheckpointSave)
			str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET Name='%s', Time='%d', cp1='%d', cp2='%d', cp3='%d', cp4='%d', cp5='%d', cp6='%d', cp7='%d', cp8='%d', cp9='%d', cp10='%d', cp11='%d', cp12='%d', cp13='%d', cp14='%d', cp15='%d', cp16='%d', cp17='%d', cp18='%d', cp19='%d', cp20='%d', cp21='%d', cp22='%d', cp23='%d', cp24='%d', cp25='%d' WHERE IP='%s';", pScore->m_aPrefix, Data->m_aMap, Data->m_aName, Data->m_Time, Data->m_aCpCurrent[0], Data->m_aCpCurrent[1], Data->m_aCpCurrent[2], Data->m_aCpCurrent[3], Data->m_aCpCurrent[4], Data->m_aCpCurrent[5], Data->m_aCpCurrent[6], Data->m_aCpCurrent[7], Data->m_aCpCurrent[8], Data->m_aCpCurrent[9], Data->m_aCpCurrent[10], Data->m_aCpCurrent[11], Data->m_aCpCurrent[12], Data->m_aCpCurrent[13], Data->m_aCpCurrent[14], Data->m_aCpCurrent[15], Data->m_aCpCurrent[16], Data->m_aCpCurrent[17], Data->m_aCpCurrent[18], Data->m_aCpCurrent[19], Data->m_aCpCurrent[20], Data->m_aCpCurrent[21], Data->m_aCpCurrent[22], Data->m_aCpCurrent[23], Data->m_aCpCurrent[24], Data->m_aIP);
		else
			str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET Name='%s', Time='%d' WHERE Name='%s';", pScore->m_aPrefix, Data->m_aMap, Data->m_aName, Data->m_Time, aName);
		pCon->Query(aBuf);
	}
	else
	{
		if(g_Config.m_SvCheckpointSave)
			str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_%s_race(Name, IP, Time, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25) VALUES ('%s', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d');", pScore->m_aPrefix, Data->m_aMap, Data->m_aName, Data->m_aIP, Data->m_Time, Data->m_aCpCurrent[0], Data->m_aCpCurrent[1], Data->m_aCpCurrent[2], Data->m_aCpCurrent[3], Data->m_aCpCurrent[4], Data->m_aCpCurrent[5], Data->m_aCpCurrent[6], Data->m_aCpCurrent[7], Data->m_aCpCurrent[8], Data->m_aCpCurrent[9], Data->m_aCpCurrent[10], Data->m_aCpCurrent[11], Data->m_aCpCurrent[12], Data->m_aCpCurrent[13], Data->m_aCpCurrent[14], Data->m_aCpCurrent[15], Data->m_aCpCurrent[16], Data->m_aCpCurrent[17], Data->m_aCpCurrent[18], Data->m_aCpCurrent[19], Data->m_aCpCurrent[20], Data->m_aCpCurrent[21], Data->m_aCpCurrent[22], Data->m_aCpCurrent[23], Data->m_aCpCurrent[24]);
		else
			str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_%s_race(Name, IP, Time) VALUES ('%s', '%s', '%d');", pScore->m_aPrefix, Data->m_aMap, Data->m_aName, Data->m_aIP, Data->m_Time);
		pCon->Query(aBuf);
	}
			
	dbg_msg("SQL", "Updating time done");
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
	
	StartSqlThread(SaveScoreThread, Tmp);
}

void CSqlScore::ShowRankThread(CSqlConnection *pCon, bool Error, void *pUser)
{
	smart_ptr<CScoreData> Data((CScoreData *)pUser);
	CSqlScore *pScore = Data->m_pScore;
	
	if(Error)
		return;

	// TODO: search function is incomplete

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "SELECT Name, IP, Time FROM %s_%s_race ORDER BY `Time` ASC;", pScore->m_aPrefix, Data->m_aMap);
	smart_ptr<CSqlResultSet> Result(pCon->QueryWithResult(aBuf));
	if(!Result.get())
		return;

	int RowCount = 0;
	bool Found = false;

	if(!Data->m_Search && g_Config.m_SvScoreIP)
	{
		while(Result->Next())
		{
			RowCount++;
			if(str_comp(Result->GetString("IP"), Data->m_aIP) == 0)
			{
				Found = true;
				break;
			}
		}
	}

	if(!Found)
	{
		Result->SetRowIndex(0);
		while(Result->Next())
		{
			RowCount++;
			//if(str_find_nocase(ppRow[0], Data->m_aName))
			if(str_comp(Result->GetString("Name"), Data->m_aName) == 0)
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
		IRace::FormatTimeLong(aTime, sizeof(aTime), Result->GetInteger("Time"));
		if(!Public)
			str_format(aBuf, sizeof(aBuf), "Your time: %s", aTime);
		else
			str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", RowCount, Result->GetString("Name"), aTime);
		if(Data->m_Search)
			str_append(aBuf, Data->m_aRequestingPlayer, sizeof(aBuf));
	}
	else
		str_format(aBuf, sizeof(aBuf), "%s is not ranked", Data->m_aName);

	if(Public)
		pScore->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	else
		pScore->GameServer()->SendChatTarget(Data->m_ClientID, aBuf);
			
	dbg_msg("SQL", "Showing rank done");
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
	
	StartSqlThread(ShowRankThread, Tmp);
}

void CSqlScore::ShowTop5Thread(CSqlConnection *pCon, bool Error, void *pUser)
{
	smart_ptr<CScoreData> Data((CScoreData *)pUser);
	CSqlScore *pScore = Data->m_pScore;
	
	if(Error)
		return;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "SELECT Name, Time FROM %s_%s_race ORDER BY `Time` ASC LIMIT %d, 5;", pScore->m_aPrefix, Data->m_aMap, Data->m_Num - 1);
	smart_ptr<CSqlResultSet> Result(pCon->QueryWithResult(aBuf));
	if(!Result.get())
		return;

	int Rank = Data->m_Num;

	pScore->GameServer()->SendChatTarget(Data->m_ClientID, "----------- Top 5 -----------");
	while(Result->Next())
	{
		char aTime[64];
		IRace::FormatTimeLong(aTime, sizeof(aTime), Result->GetInteger("Time"));
		str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", Rank, Result->GetString("Name"), aTime);
		pScore->GameServer()->SendChatTarget(Data->m_ClientID, aBuf);
		Rank++;
	}
	pScore->GameServer()->SendChatTarget(Data->m_ClientID, "------------------------------");

	dbg_msg("SQL", "Showing top5 done");
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
	
	StartSqlThread(ShowTop5Thread, Tmp);
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
