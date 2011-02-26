/* CWebapp class by Sushi */
//#include <iostream>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>
#include <engine/shared/config.h>

#include "gamecontext.h"
#include "webapp.h"

CWebapp::CWebapp(CGameContext *pGameServer)
: m_pGameServer(pGameServer),
  m_pServer(pGameServer->Server())
{
	char aBuf[512];
	int Port = 80;
	str_copy(aBuf, g_Config.m_SvWebappIp, sizeof(aBuf));

	for(int k = 0; aBuf[k]; k++)
	{
		if(aBuf[k] == ':')
		{
			Port = str_toint(aBuf+k+1);
			aBuf[k] = 0;
			break;
		}
	}

	if(net_host_lookup(aBuf, &m_Addr, NETTYPE_IPV4) != 0)
	{
		net_host_lookup("localhost", &m_Addr, NETTYPE_IPV4);
	}
	
	m_Addr.port = Port;
	
	// only one at a time
	m_JobPool.Init(1);
	m_lMapList.clear();
	m_Jobs.clear();
	m_pFirst = 0;
	m_pLast = 0;
	m_Online = 0;
}

CWebapp::~CWebapp()
{
	// wait for the runnig jobs
	do
	{
		UpdateJobs();
	} while(m_Jobs.size() > 0);
	m_lMapList.clear();
	m_Jobs.clear();
	
	IDataOut *pNext;
	for(IDataOut *pItem = m_pFirst; pItem; pItem = pNext)
	{
		pNext = pItem->m_pNext;
		delete pItem;
	}
}

const char *CWebapp::ApiKey()
{
	return g_Config.m_SvApiKey;
}

const char *CWebapp::ServerIP()
{
	return g_Config.m_SvWebappIp;
}

const char *CWebapp::MapName()
{
	return g_Config.m_SvMap;
}

void CWebapp::AddOutput(IDataOut *pOut)
{
	// TODO: add a LOCK here?
	pOut->m_pNext = 0;
	if(m_pLast)
		m_pLast->m_pNext = pOut;
	else
		m_pFirst = pOut;
	
	m_pLast = pOut;
}

void CWebapp::Tick()
{
	int Jobs = UpdateJobs();
	if(Jobs > 0)
		dbg_msg("webapp", "Removed %d jobs", Jobs);
	
	IDataOut *pItem = m_pFirst;
	IDataOut *pNext;
	for(IDataOut *pItem = m_pFirst; pItem; pItem = pNext)
	{
		int Type = pItem->m_Type;
		if(Type == WEB_USER_AUTH)
		{
			CWebUser::COut *pData = (CWebUser::COut*)pItem;
			if(pData->m_UserID > 0)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "logged in: %d", pData->m_UserID);
				GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
				GameServer()->m_apPlayers[pData->m_ClientID]->m_UserID = pData->m_UserID;
			}
			else
			{
				GameServer()->SendChatTarget(pData->m_ClientID, "wrong username and/or password");
			}
		}
		else if(Type == WEB_PING_PING)
		{
			CWebPing::COut *pData = (CWebPing::COut*)pItem;
			m_Online = pData->m_Online;
			dbg_msg("webapp", "webapp is%s online", m_Online?"":" not");
			AddJob(CWebMap::LoadList, new CWebPing::CParam());
		}
		else if(Type == WEB_MAP_LIST)
		{
			CWebMap::COut *pData = (CWebMap::COut*)pItem;
			m_lMapList = pData->m_MapList;
			for(int i = 0; i < m_lMapList.size(); i++)
				dbg_msg("webapp", "Map: %s", m_lMapList[i].c_str());
		}
		pNext = pItem->m_pNext;
		delete pItem;
	}
	m_pFirst = 0;
	m_pLast = 0;
}

bool CWebapp::Connect()
{
	// connect to the server
	m_Socket = net_tcp_create(&m_Addr);
	if(m_Socket == NETSOCKET_INVALID)
		return false;
	
	return true;
}

void CWebapp::Disconnect()
{
	net_tcp_close(m_Socket);
}

std::string CWebapp::SendAndReceive(const char* pInString)
{
	// send the data
	net_tcp_connect(m_Socket, &m_Addr);
	std::cout << pInString << std::endl;
	int DataSent = net_tcp_send(m_Socket, pInString, str_length(pInString));
	
	// receive the data
	int Received = 0;
	std::string Data = "";
	do
	{
		char aBuf[512] = {0};
		Received = net_tcp_recv(m_Socket, aBuf, sizeof(aBuf)-1);

		if(Received > 0)
			Data.append(aBuf);
	} while(Received > 0);
	
	std::cout << "---recv start---\n" << Data << "\n---recv end---\n" << std::endl;
	
	// TODO: check the header
	int Start = Data.find("\r\n\r\n");
	if(Data.length() >= Start+4)
		Data = Data.substr(Start+4);
	
	return Data;
}

CJob *CWebapp::AddJob(JOBFUNC pfnFunc, IDataIn *pUserData, bool NeedOnline)
{
	if(NeedOnline && !m_Online)
		return 0;
	pUserData->m_pWebapp = this;
	int i = m_Jobs.add(new CJob());
	m_JobPool.Add(m_Jobs[i], pfnFunc, pUserData);
	return m_Jobs[i];
}

int CWebapp::UpdateJobs()
{
	int Num = 0;
	for(int i = 0; i < m_Jobs.size(); i++)
	{
		if(m_Jobs[i]->Status() == CJob::STATE_DONE)
		{
			delete m_Jobs[i];
			m_Jobs.remove_index_fast(i);
			Num++;
		}
	}
	return Num;
}
