/* CWebapp class by Sushi */
#include <base/tl/algorithm.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include "gamecontext.h"
#include "webapp.h"

// TODO: use libcurl?

const char CWebapp::GET[] = "GET %s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nConnection: close\r\n\r\n";
const char CWebapp::POST[] = "POST %s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s";
const char CWebapp::PUT[] = "PUT %s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s";
const char CWebapp::DOWNLOAD[] = "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";

CWebapp::CWebapp(CGameContext *pGameServer)
: m_pGameServer(pGameServer),
  m_pServer(pGameServer->Server()),
  m_pStorage(m_pServer->Storage())
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
	m_Jobs.delete_all();
	
	m_OutputLock = lock_create();
	m_pFirst = 0;
	m_pLast = 0;
	
	m_Online = 0;
	LoadMaps();
}

CWebapp::~CWebapp()
{
	// wait for the runnig jobs
	do
	{
		UpdateJobs();
	} while(m_Jobs.size() > 0);
	m_lMapList.clear();
	m_Jobs.delete_all();
	
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
	lock_wait(m_OutputLock);
	pOut->m_pNext = 0;
	if(m_pLast)
		m_pLast->m_pNext = pOut;
	else
		m_pFirst = pOut;
	m_pLast = pOut;
	lock_release(m_OutputLock);
}

void CWebapp::Tick()
{
	int Jobs = UpdateJobs();
	if(Jobs > 0)
		dbg_msg("webapp", "Removed %d jobs", Jobs);
	
	// TODO: add event listener (server and client)
	lock_wait(m_OutputLock);
	IDataOut *pItem = m_pFirst;
	IDataOut *pNext;
	for(IDataOut *pItem = m_pFirst; pItem; pItem = pNext)
	{
		int Type = pItem->m_Type;
		if(Type == WEB_USER_AUTH)
		{
			CWebUser::COut *pData = (CWebUser::COut*)pItem;
			if(GameServer()->m_apPlayers[pData->m_ClientID])
			{
				if(pData->m_UserID > 0)
				{
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "logged in successfully (%s)", pData->m_aUsername);
					GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					GameServer()->m_apPlayers[pData->m_ClientID]->m_UserID = pData->m_UserID;
					
					CWebUser::CParam *pParams = new CWebUser::CParam();
					pParams->m_ClientID = pData->m_ClientID;
					pParams->m_UserID = pData->m_UserID;
					pParams->m_PrintRank = 1;
					AddJob(CWebUser::GetRank, pParams);
				}
				else
				{
					GameServer()->SendChatTarget(pData->m_ClientID, "wrong username and/or password");
				}
			}
		}
		else if(Type == WEB_USER_RANK)
		{
			CWebUser::COut *pData = (CWebUser::COut*)pItem;
			if(GameServer()->m_apPlayers[pData->m_ClientID])
			{
				GameServer()->m_apPlayers[pData->m_ClientID]->m_GlobalRank = pData->m_GlobalRank;
				GameServer()->m_apPlayers[pData->m_ClientID]->m_MapRank = pData->m_MapRank;
				if(pData->m_PrintRank)
				{
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "Global Rank: %d | Map Rank: %d (%s)",
						pData->m_GlobalRank, pData->m_MapRank, Server()->ClientName(pData->m_ClientID));
					
					if(g_Config.m_SvShowTimes)
						GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
					else
						GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
				}
			}
		}
		else if(Type == WEB_PING_PING)
		{
			CWebPing::COut *pData = (CWebPing::COut*)pItem;
			m_Online = pData->m_Online;
			dbg_msg("webapp", "webapp is%s online", m_Online?"":" not");
			AddJob(CWebMap::LoadList, new CWebMap::CParam());
		}
		else if(Type == WEB_MAP_LIST)
		{
			CWebMap::COut *pData = (CWebMap::COut*)pItem;
			array<std::string> NeededMaps;
			array<std::string> NeededURL;
			for(int i = 0; i < pData->m_MapList.size(); i++)
			{
				array<std::string>::range r = find_linear(m_lMapList.all(), pData->m_MapList[i]);
				if(r.empty())
				{
					NeededMaps.add(pData->m_MapList[i]);
					NeededURL.add(pData->m_MapURL[i]);
				}
			}
			if(NeededMaps.size() > 0)
			{
				CWebMap::CParam *pParam = new CWebMap::CParam();
				pParam->m_MapList = NeededMaps;
				pParam->m_MapURL = NeededURL;
				AddJob(CWebMap::DownloadMaps, pParam);
			}
		}
		else if(Type == WEB_MAP_DOWNLOADED)
		{
			CWebMap::COut *pData = (CWebMap::COut*)pItem;
			m_lMapList.add(pData->m_MapList[0]);
			dbg_msg("webapp", "added map: %s", pData->m_MapList[0].c_str());
			if(str_comp(pData->m_MapList[0].c_str(), MapName()) == 0)
				Server()->ReloadMap();
		}
		pNext = pItem->m_pNext;
		delete pItem;
	}
	m_pFirst = 0;
	m_pLast = 0;
	lock_release(m_OutputLock);
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

int CWebapp::GetHeaderInfo(char *pStr, int MaxSize, CHeader *pHeader)
{
	char aBuf[512] = {0};
	char *pData = pStr;
	while(str_comp_num(pData, "\r\n\r\n", 4) != 0)
	{
		pData++;
		if(pData > pStr+MaxSize)
			return -1;
	}
	pData += 4;
	int HeaderSize = pData - pStr;
	int BufSize = min((int)sizeof(aBuf),HeaderSize);
	mem_copy(aBuf, pStr, BufSize);
	
	pData = aBuf;
	//dbg_msg("webapp", "\n---header start---\n%s\n---header end---\n", aBuf);
	
	if(sscanf(pData, "HTTP/%*d.%*d %d %*s\r\n", &pHeader->m_StatusCode) != 1)
		return -2;
	
	while(sscanf(pData, "Content-Length: %ld\r\n", &pHeader->m_ContentLength) != 1)
	{
		while(str_comp_num(pData, "\r\n", 2) != 0)
		{
			pData++;
			if(pData > aBuf+BufSize)
				return -3;
		}
		pData += 2;
	}
	
	return HeaderSize;
}

int CWebapp::RecvHeader(char *pBuf, int MaxSize, CHeader *pHeader)
{
	int HeaderSize;
	int AddSize;
	int Size = 0;
	do
	{
		char *pWrite = pBuf + Size;
		AddSize = net_tcp_recv(m_Socket, pWrite, MaxSize-Size);
		Size += AddSize;
		HeaderSize = GetHeaderInfo(pBuf, Size, pHeader);
	} while(HeaderSize == -1 && MaxSize-Size > 0 && AddSize > 0);
	pHeader->m_Size = HeaderSize;
	return Size;
}

int CWebapp::SendAndReceive(const char *pInString, char **ppOutString)
{
	//dbg_msg("webapp", "\n---send start---\n%s\n---send end---\n", pInString);
	
	net_tcp_connect(m_Socket, &m_Addr);
	int DataSent = net_tcp_send(m_Socket, pInString, str_length(pInString));
	
	CHeader Header;
	int Size = 0;
	int MemLeft = 0;
	char *pWrite = 0;
	do
	{
		char aBuf[512] = {0};
		char *pData = aBuf;
		if(!pWrite)
		{
			Size = RecvHeader(aBuf, sizeof(aBuf), &Header);
			
			if(Header.m_Size < 0)
				return -1;
			
			if(Header.m_StatusCode != 200)
				return -Header.m_StatusCode;
			
			pData += Header.m_Size;
			MemLeft = Header.m_ContentLength;
			*ppOutString = (char *)mem_alloc(MemLeft+1, 1);
			mem_zero(*ppOutString, MemLeft+1);
			pWrite = *ppOutString;
		}
		else
			Size = net_tcp_recv(m_Socket, aBuf, sizeof(aBuf));
		
		if(Size > 0)
		{
			int Write = Size - (pData - aBuf);
			if(Write > MemLeft)
			{
				mem_free(*ppOutString);
				return -2;
			}
			mem_copy(pWrite, pData, Write);
			pWrite += Write;
			MemLeft = *ppOutString + Header.m_ContentLength - pWrite;
		}
	} while(Size > 0);
	
	if(MemLeft != 0)
	{
		mem_free(*ppOutString);
		return -3;
	}
	
	//dbg_msg("webapp", "\n---recv start---\n%s\n---recv end---\n", *ppOutString);
	
	return Header.m_ContentLength;
}

bool CWebapp::Download(const char *pFilename, const char *pURL)
{
	// TODO: limit transfer rate, crc check
	char aStr[256];
	str_format(aStr, sizeof(aStr), DOWNLOAD, pURL, ServerIP());
	
	net_tcp_connect(m_Socket, &m_Addr);
	int DataSent = net_tcp_send(m_Socket, aStr, str_length(aStr));
	
	CHeader Header;
	int Size = 0;
	int FileSize = 0;
	IOHANDLE File = 0;
	do
	{
		char aBuf[1024] = {0};
		char *pData = aBuf;
		if(!File)
		{
			Size = RecvHeader(aBuf, sizeof(aBuf), &Header);
			if(Header.m_Size < 0 || Header.m_StatusCode != 200)
				return 0;
			
			pData += Header.m_Size;
			dbg_msg("webapp", "saving file to %s", pFilename);
			File = Storage()->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if(!File)
				return 0;
		}
		else
			Size = net_tcp_recv(m_Socket, aBuf, sizeof(aBuf));
		
		if(Size > 0)
		{
			int Write = Size - (pData - aBuf);
			FileSize += Write;
			io_write(File, pData, Write);
		}
	} while(Size > 0);
	
	if(File)
	{
		io_close(File);
		if(FileSize != Header.m_ContentLength)
			Storage()->RemoveFile(pFilename, IStorage::TYPE_SAVE);
	}
	
	return File != 0 && FileSize == Header.m_ContentLength;
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

void CWebapp::MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CWebapp *pWebapp = (CWebapp*)pUser;
	int Length = str_length(pName);
	if(IsDir || Length < 4 || str_comp(pName+Length-4, ".map") != 0)
		return;
	
	char aBuf[256];
	str_copy(aBuf, pName, min((int)sizeof(aBuf),Length-3));
	pWebapp->m_lMapList.add(aBuf);
}

void CWebapp::LoadMaps()
{
	m_lMapList.clear();
	m_pServer->Storage()->ListDirectory(IStorage::TYPE_SAVE, "maps/teerace", MaplistFetchCallback, this);
}
