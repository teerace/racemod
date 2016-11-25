/* CSqlScore Class by Sushi */
#ifndef GAME_SERVER_SQLSCORE_H
#define GAME_SERVER_SQLSCORE_H

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include "../score.h"

struct CSqlConfig
{
	char m_aDatabase[16];
	char m_aPrefix[16];
	char m_aUser[32];
	char m_aPass[32];
	char m_aIp[32];
	int m_Port;
};

class CSqlScore : public IScore
{
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;
	
	sql::Driver *m_pDriver;
	sql::Connection *m_pConnection;
	sql::Statement *m_pStatement;
	sql::ResultSet *m_pResults;
	
	// copy of config vars
	const CSqlConfig *m_pSqlConfig;
	char m_aMap[64];
	
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }
	
	static void LoadScoreThread(void *pUser);
	static void SaveScoreThread(void *pUser);
	static void ShowRankThread(void *pUser);
	static void ShowTop5Thread(void *pUser);
	
	void Init();
	
	bool Connect();
	void Disconnect();
	
	// anti SQL injection
	void ClearString(char *pString, int Size);
	
public:
	
	CSqlScore(CGameContext *pGameServer, const CSqlConfig *pSqlConfig);
	~CSqlScore();
	
	void LoadScore(int ClientID, bool PrintRank);
	void SaveScore(int ClientID, int Time, int *pCpTime, bool NewRecord);
	void ShowRank(int ClientID, const char *pName, bool Search=false);
	void ShowTop5(int ClientID, int Debut=1);
};

struct CSqlScoreData
{
	CSqlScore *m_pSqlData;
	int m_ClientID;
	char m_aName[16];
	char m_aIP[16];
	int m_Time;
	int m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

#endif
