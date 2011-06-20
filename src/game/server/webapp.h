/* CWebapp Class by Sushi and Redix*/
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <string>
#include <base/tl/array.h>
#include <base/tl/sorted_array.h>

#include <game/webapp.h>

#include "webapp/user.h"
#include "webapp/top.h"
#include "webapp/run.h"
#include "webapp/ping.h"
#include "webapp/map.h"
#include "webapp/upload.h"

class CServerWebapp : public CWebapp
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

	class CUpload
	{
	public:
		CUpload(int Type) { m_Type = Type; }
		int m_Type;
		int m_ClientID;
		int m_UserID;
		char m_aFilename[256];
	};

	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	array<std::string> m_lMapList;
	
	array<CUpload*> m_lUploads;

	CMapInfo m_CurrentMap;
	
	bool m_DefaultScoring;
	
	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server() { return m_pServer; }
	
	void LoadMaps();
	
	static int MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);
	
public:
	static const char GET[];
	static const char POST[];
	static const char PUT[];
	static const char DOWNLOAD[];
	static const char UPLOAD[];
	
	CServerWebapp(CGameContext *pGameServer);
	virtual ~CServerWebapp();

	const char *ApiKey();
	const char *ServerIP();
	const char *ApiPath();
	const char *MapName();
	CMapInfo *CurrentMap() { return &m_CurrentMap; }
	
	bool DefaultScoring() { return m_DefaultScoring; }
	
	void Tick();

	int Upload(unsigned char *pData, int Size);
	int SendUploadHeader(const char *pHeader);
	int SendUploadEnd();
	bool Download(const char *pFilename, const char *pURL);
};

#endif
