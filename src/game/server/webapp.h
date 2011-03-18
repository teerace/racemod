/* CWebapp Class by Sushi and Redix*/
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <string>
#include <base/tl/array.h>
#include <engine/shared/jobs.h>

#include "webapp/user.h"
#include "webapp/top.h"
#include "webapp/run.h"
#include "webapp/ping.h"
#include "webapp/map.h"

class CWebapp
{
	class CHeader
	{
	public:
		int m_Size;
		int m_StatusCode;
		long m_ContentLength;
	};
	
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;
	class IStorage *m_pStorage;
	
	CJobPool m_JobPool;
	
	NETADDR m_Addr;
	NETSOCKET m_Socket;
	
	array<std::string> m_lMapList;
	array<CJob*> m_Jobs;
	
	LOCK m_OutputLock;
	
	IDataOut *m_pFirst;
	IDataOut *m_pLast;
	
	bool m_Online;
	
	int m_CurrentMapID;
	
	bool m_StandardScoring;
	
	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server() { return m_pServer; }
	class IStorage *Storage() { return m_pStorage; }
	
	void LoadMaps();
	int UpdateJobs();
	
	static void MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);
	
public:
	static const char GET[];
	static const char POST[];
	static const char PUT[];
	static const char DOWNLOAD[];
	
	CWebapp(CGameContext *pGameServer);
	~CWebapp();
	
	const char *ApiKey();
	const char *ServerIP();
	const char *MapName();
	int MapID() { return m_CurrentMapID; }
	
	bool IsOnline() { return m_Online; }
	
	bool StandardScoring() { return m_StandardScoring; }
	
	void AddOutput(class IDataOut *pOut);
	void Tick();
	
	bool Connect();
	void Disconnect();
	
	int GetHeaderInfo(char *pStr, int MaxSize, CHeader *pHeader);
	int RecvHeader(char *pBuf, int MaxSize, CHeader *pHeader);
	
	int SendAndReceive(const char *pInString, char **ppOutString);
	bool Download(const char *pFilename, const char *pURL);
	
	CJob *AddJob(JOBFUNC pfnFunc, class IDataIn *pUserData, bool NeedOnline = 1);
};

#endif
