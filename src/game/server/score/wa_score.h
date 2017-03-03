#ifndef GAME_SERVER_WASCORE_H
#define GAME_SERVER_WASCORE_H

#include "../score.h"

class CWebappScore : public IScore
{
	class CScoreData
	{
	public:
		CScoreData(CWebappScore *pScore) : m_pScore(pScore) { }
		CWebappScore *m_pScore;
		int m_ClientID;

		virtual ~CScoreData() {}
	};

	class CUserRankData : public CScoreData
	{
	public:
		CUserRankData(CWebappScore *pScore) : CScoreData(pScore), m_PrintRank(true) { }
		int m_UserID;
		int m_MapID;
		bool m_PrintRank;
		int m_GlobalRank;
		int m_MapRank;
		int m_Time;
		int m_RefCount;
		char m_aName[32];
	};

	class CUserTopData : public CScoreData
	{
	public:
		CUserTopData(CWebappScore *pScore) : CScoreData(pScore) { }
		int m_StartRank;
		int m_MapID;
	};

	class CRunData : public CScoreData
	{
	public:
		CRunData(CWebappScore *pScore) : CScoreData(pScore) { }
		int m_UserID;
		int m_MapID;
		int m_Tick;
	};

	class CGameContext *m_pGameServer;

	bool m_GotRecord;
	
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server();
	CServerWebapp *Webapp();

	void PrintRank(const CUserRankData *pUser);
	void RequestRank(CUserRankData *pUserData);

	static void OnUserFind(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnUserRankGlobal(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnUserRankMap(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnUserTop(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnRunPost(class IResponse *pResponse, bool ConnError, void *pUserData);
	
public:
	CWebappScore(CGameContext *pGameServer);
	~CWebappScore() {}

	void OnMapLoad();
	
	void OnPlayerInit(int ClientID, bool PrintRank);
	void OnPlayerFinish(int ClientID, int Time, int *pCpTime);
	
	void ShowTop5(int ClientID, int Debut=1);
	void ShowRank(int ClientID, const char *pName, bool Search=false);
};

#endif
