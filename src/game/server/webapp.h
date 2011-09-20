/* Webapp Class by Sushi and Redix */
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

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
		char m_aName[128];
		unsigned m_Crc;
		char m_aURL[128];
		char m_aAuthor[32];

		bool operator==(const CMapInfo& Other) { return str_comp(this->m_aName, Other.m_aName) == 0; }
	};

	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	array<CMapInfo> m_lMapList;
	CMapInfo m_CurrentMap;
	
	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server() { return m_pServer; }
	
	CMapInfo *AddMap(const char *pFilename);
	static int MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);

	void RegisterFields(class CRequest *pRequest, bool Api);
	void OnResponse(class CHttpConnection *pCon);
	
public:
	CServerWebapp(CGameContext *pGameServer);
	virtual ~CServerWebapp() { }
	
	CMapInfo *CurrentMap() { return &m_CurrentMap; }
	void OnInit();
};

#endif
