/* CWebapp Class by Sushi */
#ifndef GAME_SERVER_WEBAPP_H
#define GAME_SERVER_WEBAPP_H

#include <string>
#include <base/tl/array.h>
#include <engine/shared/jobs.h>

#include "webapp/user.h"
#include "webapp/run.h"

class CWebapp
{
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;
	
	CJobPool m_JobPool;
	
	NETADDR m_Addr;
	NETSOCKET m_Socket;
	
	array<std::string> m_lMapList;
	array<CJob*> m_Jobs;
	
	IDataOut *m_pFirst;
	IDataOut *m_pLast;
	
	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server() { return m_pServer; }
	
public:
	CWebapp(CGameContext *pGameServer);
	~CWebapp();
	
	const char *ApiKey();
	const char *MapName();
	
	void AddOutput(class IDataOut *pOut);
	void Tick();
	
	bool Connect();
	void Disconnect();
	std::string SendAndReceive(const char* pString);
	
	bool PingServer();
	void LoadMapList();
	CJob *AddJob(JOBFUNC pfnFunc, class IDataIn *pUserData);
	int UpdateJobs();
};

#endif
