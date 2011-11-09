/* Webapp Class by Sushi and Redix */
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <base/tl/array.h>

#include <game/http/request.h>
#include <game/data.h>
#include <game/webapp.h>

class CServerWebapp : public IWebapp
{
public:
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

	CServerWebapp(CGameContext *pGameServer);
	virtual ~CServerWebapp() { }
	
	CMapInfo *CurrentMap() { return &m_CurrentMap; }
	CMapInfo *GetMap(int Index) { return &m_lMapList[Index]; }
	int GetMapCount() { return m_lMapList.size(); }
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
		int m_Type;
		int64 m_StartTime;

		CUpload() {}
		CUpload(const char* pFilename, const char* pURL, const char* pUploadname, int Type, int64 StartTime)
		{
			str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
			str_copy(m_aURL, pURL, sizeof(m_aURL));
			str_copy(m_aUploadname, pUploadname, sizeof(m_aUploadname));
			m_Type = Type;
			m_StartTime = StartTime;
		}
	};
	array<CUpload> m_lUploads;

	array<CMapInfo> m_lMapList;
	CMapInfo m_CurrentMap;
	
	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server() { return m_pServer; }
	
	CMapInfo *AddMap(const char *pFilename);
	static int MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);

	void AddUpload(const char *pFilename, const char *pURI, const char *pUploadName, int Type, int64 StartTime = -1);

	void RegisterFields(class CRequest *pRequest, bool Api);
	void OnResponse(class CHttpConnection *pCon);
};

#endif
