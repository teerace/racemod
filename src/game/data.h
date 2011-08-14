#ifndef GAME_WEBAPP_DATA_H
#define GAME_WEBAPP_DATA_H

enum
{
	// server
	WEB_USER_AUTH = 0,
	WEB_USER_RANK_GLOBAL,
	WEB_USER_RANK_MAP,
	WEB_USER_TOP,
	WEB_USER_PLAYTIME,
	WEB_USER_UPDATESKIN,
	WEB_USER_FIND,
	WEB_PING_PING,
	WEB_MAP_LIST,
	WEB_MAP_DOWNLOADED,
	WEB_RUN_POST,

	// client
	WEB_API_TOKEN = 0,
	
	UPLOAD_DEMO = 0,
	UPLOAD_GHOST
};

class CWebData
{
public:
	int m_ClientID;

	virtual ~CWebData() {}
};

class CWebUserRankData : public CWebData
{
public:
	CWebUserRankData() : m_PrintRank(true) {}
	int m_UserID;
	bool m_PrintRank;
	int m_GlobalRank;
	char m_aName[32];
};

class CWebUserAuthData : public CWebData
{
public:
	int m_SendRconCmds;
};

class CWebUserTopData : public CWebData
{
public:
	int m_StartRank;
};

class CWebRunData : public CWebData
{
public:
	int m_UserID;
	int m_Tick;
};

#endif
