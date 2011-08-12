/* Webapp Class by Sushi and Redix */
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <string>
#include <base/tl/array.h>
#include <engine/external/json/writer.h>

#include <game/data.h>
#include <game/webapp.h>

// TODO: move this somewhere else?
struct CRankUserData
{
	CRankUserData() : m_PrintRank(true) {}
	int m_ClientID;
	int m_UserID;
	bool m_PrintRank;
	int m_GlobalRank;
	char m_aName[32];
};

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
	virtual ~CServerWebapp();

	const char *ApiKey();
	const char *ServerIP();
	const char *ApiPath();
	
	CMapInfo *CurrentMap() { return &m_CurrentMap; }
	
	bool DefaultScoring() { return m_DefaultScoring; }
	
	void Update();
	void OnResponse(int Type, IStream *pData, void *pUserData, int StatusCode);

	/*int Upload(unsigned char *pData, int Size);
	int SendUploadHeader(const char *pHeader);
	int SendUploadEnd();*/
	bool Download(const char *pFilename, const char *pURL, int Type = -1, void *pUserData = 0);

	bool SendRequest(const char *pInString, int Type, class IStream *pResponse, void *pUserData = 0, bool NeedOnline = true)
	{
		if(NeedOnline && !m_Online)
			return false;
		return IWebapp::SendRequest(pInString, Type, pResponse, pUserData);
	}
};

#endif
