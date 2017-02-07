/* Webapp Class by Sushi and Redix */
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <base/tl/sorted_array.h>
#include <engine/shared/http.h>

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
	unsigned m_Crc;
	char m_aURL[128];
	char m_aAuthor[32];

	bool operator==(const CMapInfo& Other) const { return str_comp(this->m_aName, Other.m_aName) == 0; }
	bool operator<(const CMapInfo& Other) const { return str_comp(this->m_aName, Other.m_aName) < 0; }
};

class CMapList
{
public:
	bool m_Changed;
	int64 m_LastReload;

	CMapList() : m_Changed(false), m_LastReload(-1) { m_aMapTypes[0] = 0; }

	void Reset(const char *pMapTypes);

	bool Update(class CServerWebapp *pWebapp, const char *pData, int Size);
	const CMapInfo *AddMapFile(const char *pFilename, unsigned Crc);
	const CMapInfo *FindMap(const char *pName) const;

	const char *GetMapTypes() const { return m_aMapTypes; };
	const CMapInfo *GetMap(int Index) const { return &m_lMaps[Index]; };
	int GetMapCount() const { return m_lMaps.size(); };

private:
	sorted_array<CMapInfo> m_lMaps;
	char m_aMapTypes[128];
};

class CServerWebapp
{
public:
	// helper functions
	static class CBufferRequest *CreateAuthedApiRequest(int Method, const char *pURI);
	static void RegisterFields(IRequest *pRequest);
	static void CheckStatusCode(class IConsole *pConsole, class IResponse *pResponse);

	CServerWebapp(class CGameContext *pGameServer);
	virtual ~CServerWebapp() { }
	
	CMapInfo *CurrentMap() { return &m_CurrentMap; }
	const CMapList *MapList() const { return m_pCurrentMapList; }

	bool UpdateMapList();
	void LoadMapList();
	void SendPing();
	void UpdateMapVotes();

	void OnInit();
	void OnAuth(int ClientID, const char *pToken, int SendRconCmds);
	void Tick();

	void DownloadMap(const char *pFilename, const char *pURI) { Download(pFilename, pURI, OnDownloadMap); }

	void AddUpload(const char *pFilename, const char *pURI, const char *pUploadName, FHttpCallback pfnCallback, int64 StartTime = -1);
	void AddUpload(const char *pFilename, const char *pURI, const char *pUploadName, int64 StartTime = -1)
	{
		AddUpload(pFilename, pURI, pUploadName, OnUploadFile, StartTime);
	}

private:
	enum
	{
		NUM_CACHED_MAPLISTS=4
	};

	class CWebData
	{
	public:
		CWebData(CServerWebapp *pWebapp) : m_pWebapp(pWebapp) { }
		CServerWebapp *m_pWebapp;
		int m_ClientID;

		virtual ~CWebData() {}
	};

	class CUserAuthData : public CWebData
	{
	public:
		CUserAuthData(CServerWebapp *pWebapp) : CWebData(pWebapp) { }
		int m_SendRconCmds;
	};

	class CMapListData : public CWebData
	{
	public:
		CMapListData(CServerWebapp *pWebapp) : CWebData(pWebapp) { }
		char m_aMapTypes[128];
	};

	class CUploadData : public CWebData
	{
	public:
		CUploadData(CServerWebapp *pWebapp) : CWebData(pWebapp) { }
		char m_aFilename[512];
	};

	class CUpload
	{
	public:
		char m_aFilename[128];
		char m_aURL[128];
		char m_aUploadname[128];
		FHttpCallback m_pfnCallback;
		int64 m_StartTime;

		CUpload() {}
		CUpload(const char* pFilename, const char* pURL, const char* pUploadname, FHttpCallback pfnCallback, int64 StartTime);
	};

	class CGameContext *m_pGameServer;

	array<CUpload> m_lUploads;

	int64 m_LastPing;
	int64 m_LastMapVoteUpdate;

	CMapList *m_pCurrentMapList;
	CMapList m_aCachedMapLists[NUM_CACHED_MAPLISTS];
	CMapInfo m_CurrentMap;

	// http callbacks
	static void OnUserAuth(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnPingPing(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnMapList(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnDownloadMap(class IResponse *pResponse, bool ConnError, void *pUserData);
	static void OnUploadFile(class IResponse *pResponse, bool ConnError, void *pUserData);

	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server();
	class IScore *Score();
	class IStorage *Storage();

	static int MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);

	void Download(const char *pFilename, const char *pURI, FHttpCallback pfnCallback);
	void Upload(const char *pFilename, const char *pURI, const char *pUploadName, FHttpCallback pfnCallback);
};

#endif
