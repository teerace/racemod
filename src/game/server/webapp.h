/* Webapp Class by Sushi and Redix */
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <base/tl/array.h>
#include <base/tl/sorted_array.h>

#include <engine/shared/http.h>

class CServerWebapp
{
public:
	class CMapInfo
	{
	public:
		enum
		{
			MAPSTATE_NONE,
			MAPSTATE_INFO_MISSING,
			MAPSTATE_FILE_MISSING,
			MAPSTATE_DOWNLOADING,
			MAPSTATE_COMPLETE
		};
		CMapInfo() : m_State(MAPSTATE_NONE), m_ID(-1) { }
		int m_State;
		int m_RunCount;
		int m_ID;
		char m_aName[128];
		int m_MapType;
		unsigned m_Crc;
		char m_aURL[128];
		char m_aAuthor[32];

		bool operator==(const CMapInfo& Other) { return str_comp(this->m_aName, Other.m_aName) == 0; }
		bool operator<(const CMapInfo& Other) { return this->m_ID < Other.m_ID; }
	};

	// helper functions
	static class CBufferRequest *CreateAuthedApiRequest(int Method, const char *pURI);
	static void RegisterFields(IRequest *pRequest);
	static void CheckStatusCode(class IConsole *pConsole, int Status);
	static void Download(class CGameContext *pGameServer, const char *pFilename, const char *pURI, FHttpCallback pfnCallback);
	static void Upload(class CGameContext *pGameServer, const char *pFilename, const char *pURI, const char *pUploadName, FHttpCallback pfnCallback);

	// http callbacks
	static void OnUserAuth(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnUserFind(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnUserRankGlobal(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnUserRankMap(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnUserTop(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnPingPing(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnMapList(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnDownloadMap(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnRunPost(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnUploadFile(class IResponse *pResponse, bool ConnError, void *pUserData);

	CServerWebapp(class CGameContext *pGameServer);
	virtual ~CServerWebapp() { }
	
	CMapInfo *CurrentMap() { return &m_CurrentMap; }
	CMapInfo *GetMap(int Index) { return &m_lMapList[Index]; }
	int GetMapCount() { return m_lMapList.size(); }
	void LoadMapList();
	void OnInit();
	void Tick();

private:
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	class CUpload
	{
	public:
		char m_aFilename[128];
		char m_aURL[128];
		char m_aUploadname[128];
		FHttpCallback m_pfnCallback;
		int64 m_StartTime;

		CUpload() {}
		CUpload(const char* pFilename, const char* pURL, const char* pUploadname, FHttpCallback pfnCallback, int64 StartTime)
		{
			str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
			str_copy(m_aURL, pURL, sizeof(m_aURL));
			str_copy(m_aUploadname, pUploadname, sizeof(m_aUploadname));
			m_pfnCallback = pfnCallback;
			m_StartTime = StartTime;
		}
	};
	array<CUpload> m_lUploads;

	int m_LastMapListLoad;
	sorted_array<CMapInfo> m_lMapList;
	CMapInfo m_CurrentMap;
	
	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server() { return m_pServer; }
	
	CMapInfo *AddMap(const char *pFilename, unsigned Crc);
	static int MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);
	void AddUpload(const char *pFilename, const char *pURI, const char *pUploadName, FHttpCallback pfnCallback, int64 StartTime = -1);
	void AddMapVotes();
};

#endif
