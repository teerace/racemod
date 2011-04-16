/* CWebapp Class by Sushi and Redix*/
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <string>
#include <base/tl/array.h>
#include <base/tl/sorted_array.h>
#include <engine/shared/jobs.h>

#include "webapp/user.h"
#include "webapp/top.h"
#include "webapp/run.h"
#include "webapp/ping.h"
#include "webapp/map.h"
#include "webapp/upload.h"

class CWebapp
{
	class CHeader
	{
	public:
		int m_Size;
		int m_StatusCode;
		long m_ContentLength;
	};
	
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
	class IStorage *m_pStorage;
	
	CJobPool m_JobPool;
	
	NETADDR m_Addr;
	NETSOCKET m_Socket;
	
	array<std::string> m_lMapList;
	array<CJob*> m_Jobs;
	
	array<CUpload*> m_lUploads;
	
	LOCK m_OutputLock;
	
	IDataOut *m_pFirst;
	IDataOut *m_pLast;
	
	bool m_Online;
	
	CMapInfo m_CurrentMap;
	
	bool m_DefaultScoring;
	
	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server() { return m_pServer; }
	
	void LoadMaps();
	int UpdateJobs();
	
	static int MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);
	
public:
	static const char GET[];
	static const char POST[];
	static const char PUT[];
	static const char DOWNLOAD[];
	static const char UPLOAD[];
	
	CWebapp(CGameContext *pGameServer);
	~CWebapp();
	
	class IStorage *Storage() { return m_pStorage; }

	const char *ApiKey();
	const char *ServerIP();
	const char *ApiPath();
	const char *MapName();
	CMapInfo *CurrentMap() { return &m_CurrentMap; }
	
	bool IsOnline() { return m_Online; }
	
	bool DefaultScoring() { return m_DefaultScoring; }
	
	void AddOutput(class IDataOut *pOut);
	void Tick();
	
	bool Connect();
	void Disconnect();
	
	int GetHeaderInfo(char *pStr, int MaxSize, CHeader *pHeader);
	int RecvHeader(char *pBuf, int MaxSize, CHeader *pHeader);
	
	int SendAndReceive(const char *pInString, char **ppOutString);
	int Upload(unsigned char *pData, int Size);
	int SendUploadHeader(const char *pHeader);
	int SendUploadEnd();
	bool Download(const char *pFilename, const char *pURL);
	
	CJob *AddJob(JOBFUNC pfnFunc, class IDataIn *pUserData, bool NeedOnline = 1);
};

#endif
