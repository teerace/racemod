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
	for(IDataOut *pItem = m_pFirst; pItem; pItem = pItem->m_pNext)
		delete pItem;
}

const char *CWebapp::ApiKey()
{
	return g_Config.m_SvApiKey;
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
		dbg_msg("", "Removed %d jobs", Jobs);
	
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
	//std::cout << pInString << std::endl;
	int DataSent = net_tcp_send(m_Socket, pInString, str_length(pInString));
	
	// receive the data
	int Received = 0;
	std::string Data = "";
	do
	{
		char aBuf[512] = {0};
		Received = net_tcp_recv(m_Socket, aBuf, 511);

		if(Received > 0)
			Data.append(aBuf);
	} while(Received > 0);
	
	//std::cout << "---recv start---\n" << Data << "\n---recv end---\n" << std::endl;
	
	// TODO: check the header
	int Start = Data.find("\r\n\r\n");
	if(Data.length() >= Start+4)
		Data = Data.substr(Start+4);
	
	return Data;
}

// TODO: thread
bool CWebapp::PingServer()
{
	// connect to the server
	if(!Connect())
		return false;
	
	// TODO: use /api/1/ping/
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "GET /api/1/hello/ HTTP/1.1\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\n\r\n", g_Config.m_SvApiKey);
	std::string Received = SendAndReceive(aBuf);
	Disconnect();
	
	//std::cout << "Recv: '" << Received << "'" << std::endl;
	
	if(!Received.compare("\"PONG\""))
		return true;
	
	return false;
}

// TODO: thread
void CWebapp::LoadMapList()
{
	// clear maplist
	m_lMapList.clear();
	
	if(!Connect())
		return;
		
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "GET /api/1/maps/list/ HTTP/1.1\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\n\r\n", g_Config.m_SvApiKey);
	std::string Received = SendAndReceive(aBuf);
	Disconnect();
	
	// cutting out the maps fomr the Received data
	Json::Value Maplist;
	Json::Reader Reader;
	bool ParsingSuccessful = Reader.parse(Received, Maplist);
	if(!ParsingSuccessful)
		return;
	
	for(int i = 0; i < Maplist.size(); i++)
	{
		Json::Value Map = Maplist[i];
		m_lMapList.add(Map["name"].asString());
		dbg_msg("LoadedMap", "%s", m_lMapList[i].c_str());
	}
}

CJob *CWebapp::AddJob(JOBFUNC pfnFunc, IDataIn *pUserData)
{
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
