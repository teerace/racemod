/* CSqlScore Class by Sushi */
#ifndef GAME_SERVER_SQLSCORE_H
#define GAME_SERVER_SQLSCORE_H

#include "../score.h"

class CSqlConnection;
struct CSqlConfig;

struct CSqlExecData
{
	typedef void(*FQueryFunc)(CSqlConnection *pCon, bool Error, void *pUserData);

	CSqlExecData(class CSqlScore *pScore, FQueryFunc pFunc, void *pUserData = 0)
		: m_pScore(pScore), m_pFunc(pFunc), m_pUserData(pUserData) { }

	CSqlScore *m_pScore;
	FQueryFunc m_pFunc;
	void *m_pUserData;
};

class CSqlScore : public IScore
{
	class CScoreData
	{
	public:
		CScoreData(CSqlScore *pScore) : m_pScore(pScore) { }
		CSqlScore *m_pScore;
		int m_ClientID;
		char m_aMap[64];
		char m_aName[32];
		char m_aIP[16];
		int m_Time;
		int m_aCpCurrent[NUM_CHECKPOINTS];
		int m_Num;
		bool m_Search;
		char m_aRequestingPlayer[MAX_NAME_LENGTH];
	};

	class CGameContext *m_pGameServer;
	class IServer *m_pServer;
	
	bool m_DbExists;
	
	// config vars
	CSqlConfig *m_pSqlConfig;
	char m_aPrefix[16];
	
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	static void ExecSqlFunc(void *pUser);
	
	static void LoadScoreThread(CSqlConnection *pCon, bool Error, void *pUser);
	static void SaveScoreThread(CSqlConnection *pCon, bool Error, void *pUser);
	static void ShowRankThread(CSqlConnection *pCon, bool Error, void *pUser);
	static void ShowTop5Thread(CSqlConnection *pCon, bool Error, void *pUser);
	
	void Init();
	
	// anti SQL injection
	void ClearString(char *pString, int Size);
	
public:
	
	CSqlScore(CGameContext *pGameServer);
	~CSqlScore();

	void OnMapLoad();
	
	void OnPlayerInit(int ClientID, bool PrintRank);
	void OnPlayerFinish(int ClientID, int Time, int *pCpTime);

	void ShowRank(int ClientID, const char *pName, bool Search=false);
	void ShowTop5(int ClientID, int Debut=1);
};

#endif
