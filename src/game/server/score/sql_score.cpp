/* CSqlScore class by Sushi */
#if defined(CONF_SQL)

#include <string.h>
#include <engine/shared/config.h>

#include "../gamecontext.h"
#include "sql_score.h"

static LOCK gs_SqlLock = 0;

CSqlScore::CSqlScore(CGameContext *pGameServer)
: m_pGameServer(pGameServer),
  m_pServer(pGameServer->Server()),
  m_pDatabase(g_Config.m_SvSqlDatabase),
  m_pPrefix(g_Config.m_SvSqlPrefix),
  m_pUser(g_Config.m_SvSqlUser),
  m_pPass(g_Config.m_SvSqlPw),
  m_pIp(g_Config.m_SvSqlIp),
  m_Port(g_Config.m_SvSqlPort)
{
	str_copy(m_aMap, g_Config.m_SvMap, sizeof(m_aMap));
	ClearString(m_aMap, sizeof(m_aMap));
	
	if(gs_SqlLock == 0)
		gs_SqlLock = lock_create();
	
	Init();
}

CSqlScore::~CSqlScore()
{
	lock_wait(gs_SqlLock);
	lock_release(gs_SqlLock);
}

bool CSqlScore::Connect()
{
	try 
	{
		// Create connection
		m_pDriver = get_driver_instance();
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "tcp://%s:%d", m_pIp, m_Port);
		m_pConnection = m_pDriver->connect(aBuf, m_pUser, m_pPass);
		
		// Create Statement
		m_pStatement = m_pConnection->createStatement();
		
		// Create database if not exists
		str_format(aBuf, sizeof(aBuf), "CREATE DATABASE IF NOT EXISTS %s", m_pDatabase);
		m_pStatement->execute(aBuf);
		
		// Connect to specific database
		m_pConnection->setSchema(m_pDatabase);
		dbg_msg("SQL", "SQL connection established");
		return true;
	} 
	catch (sql::SQLException &e)
	{
		dbg_msg("SQL", "ERROR: SQL connection failed");
		return false;
	}
	return false;
}

void CSqlScore::Disconnect()
{
	try
	{
		delete m_pConnection;
		dbg_msg("SQL", "SQL connection disconnected");
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("SQL", "ERROR: No SQL connection");
	}
}

// create tables... should be done only once
void CSqlScore::Init()
{
	// create connection
	if(Connect())
	{
		try
		{
			// create tables
			char aBuf[768];
			str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_%s_race (Name VARCHAR(31) NOT NULL, Time INTEGER DEFAULT 0, IP VARCHAR(16) DEFAULT '0.0.0.0', cp1 INTEGER DEFAULT 0, cp2 INTEGER DEFAULT 0, cp3 INTEGER DEFAULT 0, cp4 INTEGER DEFAULT 0, cp5 INTEGER DEFAULT 0, cp6 INTEGER DEFAULT 0, cp7 INTEGER DEFAULT 0, cp8 INTEGER DEFAULT 0, cp9 INTEGER DEFAULT 0, cp10 INTEGER DEFAULT 0, cp11 INTEGER DEFAULT 0, cp12 INTEGER DEFAULT 0, cp13 INTEGER DEFAULT 0, cp14 INTEGER DEFAULT 0, cp15 INTEGER DEFAULT 0, cp16 INTEGER DEFAULT 0, cp17 INTEGER DEFAULT 0, cp18 INTEGER DEFAULT 0, cp19 INTEGER DEFAULT 0, cp20 INTEGER DEFAULT 0, cp21 INTEGER DEFAULT 0, cp22 INTEGER DEFAULT 0, cp23 INTEGER DEFAULT 0, cp24 INTEGER DEFAULT 0, cp25 INTEGER DEFAULT 0);", m_pPrefix, m_aMap);
			m_pStatement->execute(aBuf);
			dbg_msg("SQL", "Tables were created successfully");
			
			// get the best time
			str_format(aBuf, sizeof(aBuf), "SELECT Time FROM %s_%s_race ORDER BY `Time` ASC LIMIT 0, 1;", m_pPrefix, m_aMap);
			m_pResults = m_pStatement->executeQuery(aBuf);
			
			if(m_pResults->next())
			{
				GetRecord()->m_Time = m_pResults->getInt("Time");
				char aColumn[8];
				for(int i = 0; i < NUM_CHECKPOINTS; i++)
				{
					str_format(aColumn, sizeof(aColumn), "cp%d", i+1);
					GetRecord()->m_aCpTime[i] = m_pResults->getInt(aColumn);
				}
				
				dbg_msg("SQL", "Getting best time on server done");
			
				// delete results
				delete m_pResults;
			}
				
			// delete statement
			delete m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Tables were NOT created");
		}

		// disconnect from database
		Disconnect();
	}
}

// update stuff
void CSqlScore::LoadScoreThread(void *pUser)
{
	lock_wait(gs_SqlLock);
	
	CSqlScoreData *pData = (CSqlScoreData *)pUser;
	
	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			// check strings
			pData->m_pSqlData->ClearString(pData->m_aName, sizeof(pData->m_aName));
			
			char aBuf[512];
			// check if there is an entry with the same ip
			if(g_Config.m_SvScoreIP)
			{
				str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE IP='%s';", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aIP);
				pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);
				
				if(pData->m_pSqlData->m_pResults->next())
				{
					// get the best time
					pData->m_pSqlData->PlayerData(pData->m_ClientID)->m_Time = pData->m_pSqlData->m_pResults->getInt("Time");
					char aColumn[8];
					for(int i = 0; i < NUM_CHECKPOINTS; i++)
					{
						str_format(aColumn, sizeof(aColumn), "cp%d", i+1);
						pData->m_pSqlData->PlayerData(pData->m_ClientID)->m_aCpTime[i] = pData->m_pSqlData->m_pResults->getInt(aColumn);
					}
					
					dbg_msg("SQL", "Getting best time done");
				
					// delete statement and results
					delete pData->m_pSqlData->m_pStatement;
					delete pData->m_pSqlData->m_pResults;
				
					// disconnect from database
					pData->m_pSqlData->Disconnect();
					
					delete pData;

					lock_release(gs_SqlLock);
					
					return;
				}
				
			}
		
			str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE Name='%s';", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aName);
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);
			if(pData->m_pSqlData->m_pResults->next())
			{
				// check if IP differs
				const char* pIP = pData->m_pSqlData->m_pResults->getString("IP").c_str();
				if(str_comp(pIP, pData->m_aIP) != 0)
				{
					// set the new ip
					str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET IP='%s' WHERE Name='%s';", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aIP, pData->m_aName);
					pData->m_pSqlData->m_pStatement->execute(aBuf);
				}
				
				// get the best time
				pData->m_pSqlData->PlayerData(pData->m_ClientID)->m_Time = pData->m_pSqlData->m_pResults->getInt("Time");
				char aColumn[8];
				if(g_Config.m_SvCheckpointSave)
				{
					for(int i = 0; i < NUM_CHECKPOINTS; i++)
					{
						str_format(aColumn, sizeof(aColumn), "cp%d", i+1);
						pData->m_pSqlData->PlayerData(pData->m_ClientID)->m_aCpTime[i] = pData->m_pSqlData->m_pResults->getInt(aColumn);
					}
				}
			}
			
			dbg_msg("SQL", "Getting best time done");
			
			// delete statement and results
			delete pData->m_pSqlData->m_pStatement;
			delete pData->m_pSqlData->m_pResults;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not update account");
		}
		
		// disconnect from database
		pData->m_pSqlData->Disconnect();
	}
	
	delete pData;

	lock_release(gs_SqlLock);
}

void CSqlScore::LoadScore(int ClientID, bool PrintRank)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), sizeof(Tmp->m_aName));
	Server()->GetClientAddr(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));
	Tmp->m_pSqlData = this;
	
	void *LoadThread = thread_create(LoadScoreThread, Tmp);
	thread_detach(LoadThread);
}

void CSqlScore::SaveScoreThread(void *pUser)
{
	lock_wait(gs_SqlLock);
	
	CSqlScoreData *pData = (CSqlScoreData *)pUser;
	
	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			// check strings
			pData->m_pSqlData->ClearString(pData->m_aName, sizeof(pData->m_aName));
			
			char aBuf[768];
			
			// fisrt check for IP
			str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE IP='%s';", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aIP);
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);
			
			// if ip found...
			if(pData->m_pSqlData->m_pResults->next())
			{
				// update time
				if(g_Config.m_SvCheckpointSave)
					str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET Name='%s', Time='%d', cp1='%d', cp2='%d', cp3='%d', cp4='%d', cp5='%d', cp6='%d', cp7='%d', cp8='%d', cp9='%d', cp10='%d', cp11='%d', cp12='%d', cp13='%d', cp14='%d', cp15='%d', cp16='%d', cp17='%d', cp18='%d', cp19='%d', cp20='%d', cp21='%d', cp22='%d', cp23='%d', cp24='%d', cp25='%d' WHERE IP='%s';", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aName, pData->m_Time, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24], pData->m_aIP);
				else
					str_format(aBuf, sizeof(aBuf), "UPDATE %s_%s_race SET Name='%s', Time='%d' WHERE IP='%s';", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aName, pData->m_Time, pData->m_aIP);
				pData->m_pSqlData->m_pStatement->execute(aBuf);
				
				dbg_msg("SQL", "Updating time done");
				
				// delete results statement
				delete pData->m_pSqlData->m_pResults;
				delete pData->m_pSqlData->m_pStatement;
				
				// disconnect from database
				pData->m_pSqlData->Disconnect();
				
				delete pData;
				
				lock_release(gs_SqlLock);
				
				return;
			}
			
			// if no entry found... create a new one
			str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_%s_race(Name, IP, Time, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25) VALUES ('%s', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d');", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aName, pData->m_aIP, pData->m_Time, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24]);
			pData->m_pSqlData->m_pStatement->execute(aBuf);
			
			dbg_msg("SQL", "Updateing time done");
			
			// delete results statement
			delete pData->m_pSqlData->m_pResults;
			delete pData->m_pSqlData->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not update time");
		}
		
		// disconnect from database
		pData->m_pSqlData->Disconnect();
	}
	
	delete pData;

	lock_release(gs_SqlLock);
}

void CSqlScore::SaveScore(int ClientID, int Time, int *pCpTime, bool NewRecord)
{
	if(!NewRecord)
		return;

	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Time = PlayerData(ClientID)->m_Time;
	mem_copy(Tmp->m_aCpCurrent, PlayerData(ClientID)->m_aCpTime, sizeof(Tmp->m_aCpCurrent));
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), sizeof(Tmp->m_aName));
	Server()->GetClientAddr(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));
	Tmp->m_pSqlData = this;
	
	void *SaveThread = thread_create(SaveScoreThread, Tmp);
	thread_detach(SaveThread);
}

void CSqlScore::ShowRankThread(void *pUser)
{
	lock_wait(gs_SqlLock);
	
	CSqlScoreData *pData = (CSqlScoreData *)pUser;
	
	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			// check strings
			pData->m_pSqlData->ClearString(pData->m_aName, sizeof(pData->m_aName));
			
			// check sort methode
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SELECT Name, IP, Time FROM %s_%s_race ORDER BY `Time` ASC;", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap);
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);
			int RowCount = 0;
			bool Found = false;
			while(pData->m_pSqlData->m_pResults->next())
			{
				RowCount++;
				
				if(pData->m_Search)
				{
					if(str_find_nocase(pData->m_pSqlData->m_pResults->getString("Name").c_str(), pData->m_aName))
					{
						Found = true;
						break;
					}
				}
				else if(!str_comp(pData->m_pSqlData->m_pResults->getString("IP").c_str(), pData->m_aIP))
				{
					Found = true;
					break;
				}
			}
			
			if(!Found)
			{
				str_format(aBuf, sizeof(aBuf), "%s is not ranked", pData->m_aName);
				pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
			}
			else
			{
				int Time = pData->m_pSqlData->m_pResults->getInt("Time");
				if(!g_Config.m_SvShowTimes)
					str_format(aBuf, sizeof(aBuf), "Your time: %d minute(s) %d.%03d second(s)",
						Time / (60 * 1000), (Time / 1000) % 60, Time % 1000);
				else
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %d.%03d second(s)",
						RowCount, pData->m_pSqlData->m_pResults->getString("Name").c_str(), Time / (60 * 1000), (Time / 1000) % 60, Time % 1000);
				
				if(pData->m_Search)
					strcat(aBuf, pData->m_aRequestingPlayer);
					
				pData->m_pSqlData->GameServer()->SendChatTarget(-1, aBuf);
			}
			
			dbg_msg("SQL", "Showing rank done");
			
			// delete results and statement
			delete pData->m_pSqlData->m_pResults;	
			delete pData->m_pSqlData->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not show rank");
		}
		
		// disconnect from database
		pData->m_pSqlData->Disconnect();
	}
	
	delete pData;

	lock_release(gs_SqlLock);
}

void CSqlScore::ShowRank(int ClientID, const char *pName, bool Search)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, pName, sizeof(Tmp->m_aName));
	Server()->GetClientAddr(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));
	Tmp->m_Search = Search;
	str_format(Tmp->m_aRequestingPlayer, sizeof(Tmp->m_aRequestingPlayer), " (%s)", Server()->ClientName(ClientID));
	Tmp->m_pSqlData = this;
	
	void *RankThread = thread_create(ShowRankThread, Tmp);
	thread_detach(RankThread);
}

void CSqlScore::ShowTop5Thread(void *pUser)
{
	lock_wait(gs_SqlLock);
	
	CSqlScoreData *pData = (CSqlScoreData *)pUser;
	
	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			// check sort methode
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SELECT Name, Time FROM %s_%s_race ORDER BY `Time` ASC LIMIT %d, 5;", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_Num-1);
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);
			
			// show top5
			pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, "----------- Top 5 -----------");
			
			int Rank = pData->m_Num;
			int Time = 0;
			while(pData->m_pSqlData->m_pResults->next())
			{
				Time = pData->m_pSqlData->m_pResults->getInt("Time");
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %d.%03d second(s)",
					Rank, pData->m_pSqlData->m_pResults->getString("Name").c_str(), Time / (60 * 1000), (Time / 1000) % 60, Time % 1000);
				pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
				Rank++;
			}
			pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, "------------------------------");
			
			dbg_msg("SQL", "Showing top5 done");
			
			// delete results and statement
			delete pData->m_pSqlData->m_pResults;
			delete pData->m_pSqlData->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("SQL", "ERROR: Could not show top5");
		}
		
		// disconnect from database
		pData->m_pSqlData->Disconnect();
	}
	
	delete pData;

	lock_release(gs_SqlLock);
}

void CSqlScore::ShowTop5(int ClientID, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;
	Tmp->m_pSqlData = this;
	
	void *Top5Thread = thread_create(ShowTop5Thread, Tmp);
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
