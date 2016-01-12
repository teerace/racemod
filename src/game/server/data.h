#ifndef GAME_WEBAPP_DATA_H
#define GAME_WEBAPP_DATA_H

#include "gamecontext.h"

class CWebData
{
public:
	CWebData(CGameContext *pGameServer) : m_pGameServer(pGameServer) { }
	CGameContext *m_pGameServer;
	int m_ClientID;

	virtual ~CWebData() {}
};

class CWebUserRankData : public CWebData
{
public:
	CWebUserRankData(CGameContext *pGameServer) : CWebData(pGameServer), m_PrintRank(true) { }
	int m_UserID;
	bool m_PrintRank;
	int m_GlobalRank;
	char m_aName[32];
};

class CWebUserAuthData : public CWebData
{
public:
	CWebUserAuthData(CGameContext *pGameServer) : CWebData(pGameServer) { }
	int m_SendRconCmds;
};

class CWebUserTopData : public CWebData
{
public:
	CWebUserTopData(CGameContext *pGameServer) : CWebData(pGameServer) { }
	int m_StartRank;
};

class CWebRunData : public CWebData
{
public:
	CWebRunData(CGameContext *pGameServer) : CWebData(pGameServer) { }
	int m_UserID;
	int m_Tick;
};

class CWebUploadData : public CWebData
{
public:
	CWebUploadData(CGameContext *pGameServer) : CWebData(pGameServer) { }
	char m_aFilename[512];
};

#endif
