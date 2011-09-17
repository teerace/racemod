/* Webapp Class by Sushi and Redix */
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <string>
#include <base/tl/array.h>

#include <game/http/request.h>
#include <game/data.h>
#include <game/webapp.h>

class CServerWebapp : public IWebapp
{
	class CMapInfo
	{
	public:
		CMapInfo() { m_ID = -1; }
		int m_RunCount;
		int m_ID;
		char m_aCrc[16];
		char m_aURL[128];
		char m_aAuthor[32];
	};

	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	array<std::string> m_lMapList;

	CMapInfo m_CurrentMap;
	bool m_DefaultScoring;
	
	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server() { return m_pServer; }
	
	void LoadMaps();
	static int MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);

	void RegisterFields(class CRequest *pRequest, bool Api);
	void OnResponse(class CHttpConnection *pCon);
	
public:
	CServerWebapp(CGameContext *pGameServer);
	virtual ~CServerWebapp() { }
	
	CMapInfo *CurrentMap() { return &m_CurrentMap; }
	bool DefaultScoring() { return m_DefaultScoring; }
};

#endif
