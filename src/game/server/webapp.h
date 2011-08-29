/* Webapp Class by Sushi and Redix */
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <string>
#include <base/tl/array.h>

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
	bool m_Online;
	
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
	virtual ~CServerWebapp() { }

	const char *ApiKey();
	const char *ServerIP();
	const char *ApiPath();
	
	CMapInfo *CurrentMap() { return &m_CurrentMap; }
	
	bool DefaultScoring() { return m_DefaultScoring; }
	
	void Update();
	void OnResponse(int Type, IStream *pData, CWebData *pUserData, int StatusCode);

	bool Download(const char *pFilename, const char *pURL, int Type, CWebData *pUserData = 0);
	bool Upload(IOHANDLE File, const char *pURL, int Type, const char *pName, CWebData *pUserData = 0, int64 StartTime = -1);

	bool SendRequest(const char *pInString, int Type, class IStream *pResponse, CWebData *pUserData = 0, IOHANDLE File = 0, bool NeedOnline = true, int64 StartTime = -1)
	{
		if(NeedOnline && !m_Online)
			return false;
		return IWebapp::SendRequest(pInString, Type, pResponse, pUserData, File, StartTime);
	}
};

#endif
