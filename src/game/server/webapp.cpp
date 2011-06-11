/* CWebapp class by Sushi and Redix*/
#if defined(CONF_TEERACE)

#include <stdio.h>

#include <base/tl/algorithm.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>

#include "gamecontext.h"
#include "webapp.h"

// TODO: use libcurl?

const char CServerWebapp::GET[] = "GET %s/%s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nConnection: close\r\n\r\n";
const char CServerWebapp::POST[] = "POST %s/%s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s";
const char CServerWebapp::PUT[] = "PUT %s/%s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s";
const char CServerWebapp::DOWNLOAD[] = "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";
const char CServerWebapp::UPLOAD[] = "POST %s/%s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nContent-Type: multipart/form-data; boundary=frontier\r\nContent-Length: %d\r\n\r\n--frontier\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"file.file\"\r\nContent-Type: application/octet-stream\r\n\r\n";

CServerWebapp::CServerWebapp(CGameContext *pGameServer)
: CWebapp(pGameServer->Server()->Storage(), g_Config.m_SvWebappIp),
  m_pGameServer(pGameServer),
  m_pServer(pGameServer->Server()),
  m_DefaultScoring(g_Config.m_SvDefaultScoring)
{
	m_lUploads.delete_all();

	LoadMaps();
}

CServerWebapp::~CServerWebapp()
{
	m_lMapList.clear();
	m_lUploads.delete_all();
}

const char *CServerWebapp::ApiKey()
{
	return g_Config.m_SvApiKey;
}

const char *CServerWebapp::ServerIP()
{
	return g_Config.m_SvWebappIp;
}

const char *CServerWebapp::ApiPath()
{
	return g_Config.m_SvApiPath;
}

const char *CServerWebapp::MapName()
{
	return g_Config.m_SvMap;
}

void CServerWebapp::Tick()
{
	int Jobs = UpdateJobs();
	if(Jobs > 0)
		dbg_msg("webapp", "Removed %d jobs", Jobs);
	
	// TODO: add event listener (server and client)
	lock_wait(m_OutputLock);
	IDataOut *pNext = 0;
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
					str_format(aBuf, sizeof(aBuf), "%s has logged in as %s", Server()->ClientName(pData->m_ClientID), pData->m_aUsername);
					GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
					Server()->SetUserID(pData->m_ClientID, pData->m_UserID);
					Server()->SetUserName(pData->m_ClientID, pData->m_aUsername);
					
					// auth staff members
					if(pData->m_IsStaff)
						Server()->StaffAuth(pData->m_ClientID);
					
					CWebUser::CParam *pParams = new CWebUser::CParam();
					str_copy(pParams->m_aName, Server()->GetUserName(pData->m_ClientID), sizeof(pParams->m_aName));
					pParams->m_ClientID = pData->m_ClientID;
					pParams->m_UserID = pData->m_UserID;
					pParams->m_GetBestRun = 1;
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
					char aBuf[256];
					if(!pData->m_GlobalRank)
					{
						if(pData->m_MatchFound)
						{
							if(pData->m_UserID == Server()->GetUserID(pData->m_ClientID))
								str_copy(aBuf, "You are not globally ranked yet.", sizeof(aBuf));
							else
								str_format(aBuf, sizeof(aBuf), "%s is not globally ranked yet.", pData->m_aUsername);
						}
						else
							str_format(aBuf, sizeof(aBuf), "No match found for \"%s\".", pData->m_aUsername);
						GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					}
					else
					{
						if(!pData->m_MapRank)
							str_format(aBuf, sizeof(aBuf), "%s: Global Rank: %d | Map Rank: Not ranked yet (%s)",
								pData->m_aUsername, pData->m_GlobalRank, Server()->ClientName(pData->m_ClientID));
						else
						{
							if(pData->m_BestRun.m_Time < 60.0f)
								str_format(aBuf, sizeof(aBuf), "%s: Global Rank: %d | Map Rank: %d | Time: %.3f (%s)",
									pData->m_aUsername, pData->m_GlobalRank, pData->m_MapRank, pData->m_BestRun.m_Time,
									Server()->ClientName(pData->m_ClientID));
							else
								str_format(aBuf, sizeof(aBuf), "%s: Global Rank: %d | Map Rank: %d | Time: %02d:%06.3f (%s)",
									pData->m_aUsername, pData->m_GlobalRank, pData->m_MapRank, (int)pData->m_BestRun.m_Time/60,
									fmod(pData->m_BestRun.m_Time, 60), Server()->ClientName(pData->m_ClientID));
						}
						
						if(g_Config.m_SvShowTimes)
							GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
						else
							GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
					}
				}
				
				// saving the best run
				if(pData->m_GetBestRun && pData->m_MapRank)
					GameServer()->Score()->PlayerData(pData->m_ClientID)->Set(pData->m_BestRun.m_Time, pData->m_BestRun.m_aCpTime);
			}
		}
		else if(Type == WEB_USER_TOP)
		{
			CWebTop::COut *pData = (CWebTop::COut*)pItem;
			if(GameServer()->m_apPlayers[pData->m_ClientID])
			{
				char aBuf[256];
				GameServer()->SendChatTarget(pData->m_ClientID, "----------- Top 5 -----------");
				for(int i = 0; i < pData->m_lUserRanks.size() && i < 5; i++)
				{
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %.3f second(s)",
						i+pData->m_Start, pData->m_lUserRanks[i].m_aName, (int)pData->m_lUserRanks[i].m_Time/60, fmod(pData->m_lUserRanks[i].m_Time, 60));
					GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
				}
				GameServer()->SendChatTarget(pData->m_ClientID, "------------------------------");
			}
		}
		else if(Type == WEB_PING_PING)
		{
			CWebPing::COut *pData = (CWebPing::COut*)pItem;
			SetOnline(pData->m_Online);
			dbg_msg("webapp", "webapp is%s online", IsOnline()?"":" not");
			CWebMap::CParam *pParam = new CWebMap::CParam();
			pParam->m_CrcCheck = pData->m_CrcCheck;
			AddJob(CWebMap::LoadList, pParam);
		}
		else if(Type == WEB_MAP_LIST)
		{
			CWebMap::COut *pData = (CWebMap::COut*)pItem;
			array<std::string> NeededMaps;
			array<std::string> NeededURL;
			for(int i = 0; i < pData->m_lMapName.size(); i++)
			{
				// get current map
				if(!str_comp(pData->m_lMapName[i].c_str(), MapName()))
				{
					GameServer()->Score()->GetRecord()->Set(pData->m_lMapRecord[i].m_Time, pData->m_lMapRecord[i].m_aCpTime);
					m_CurrentMap.m_ID = pData->m_lMapID[i];
					m_CurrentMap.m_RunCount = pData->m_lMapRunCount[i];
					str_copy(m_CurrentMap.m_aCrc, pData->m_lMapCrc[i].c_str(), sizeof(m_CurrentMap.m_aCrc));
					str_copy(m_CurrentMap.m_aURL, pData->m_lMapURL[i].c_str(), sizeof(m_CurrentMap.m_aURL));
					str_copy(m_CurrentMap.m_aAuthor, pData->m_lMapAuthor[i].c_str(), sizeof(m_CurrentMap.m_aAuthor));
				}
				
				array<std::string>::range r = find_linear(m_lMapList.all(), pData->m_lMapName[i]);
				if(r.empty())
				{
					NeededMaps.add(pData->m_lMapName[i]);
					NeededURL.add(pData->m_lMapURL[i]);
				}
				else if(pData->m_CrcCheck)// map found... check crc
				{
					char aFilename[256];
					str_format(aFilename, sizeof(aFilename), "maps/teerace/%s.map", pData->m_lMapName[i].c_str());
					CDataFileReader DataFile;
					if(!DataFile.Open(Storage(), aFilename, IStorage::TYPE_SAVE))
					{
						NeededMaps.add(pData->m_lMapName[i]);
						NeededURL.add(pData->m_lMapURL[i]);
					}
					else
					{
						char aCrc[16];
						str_format(aCrc, sizeof(aCrc), "%x", DataFile.Crc());
						if(str_comp(aCrc, pData->m_lMapCrc[i].c_str()))
						{
							NeededMaps.add(pData->m_lMapName[i]);
							NeededURL.add(pData->m_lMapURL[i]);
						}
						
						DataFile.Close();
					}
				}
			}
			if(NeededMaps.size() > 0)
			{
				CWebMap::CParam *pParam = new CWebMap::CParam();
				pParam->m_lMapName = NeededMaps;
				pParam->m_lMapURL = NeededURL;
				AddJob(CWebMap::DownloadMaps, pParam);
			}
		}
		else if(Type == WEB_MAP_DOWNLOADED)
		{
			CWebMap::COut *pData = (CWebMap::COut*)pItem;
			m_lMapList.add(pData->m_lMapName[0]);
			dbg_msg("webapp", "added map: %s", pData->m_lMapName[0].c_str());
			if(str_comp(pData->m_lMapName[0].c_str(), MapName()) == 0)
				Server()->ReloadMap();
		}
		else if(Type == WEB_RUN)
		{
			CWebRun::COut *pData = (CWebRun::COut*)pItem;
			if(pData->m_Tick > -1)
			{
				// demo
				CUpload *pDemo = new CUpload(UPLOAD_DEMO);
				pDemo->m_ClientID = pData->m_ClientID;
				pDemo->m_UserID = pData->m_UserID;
				str_format(pDemo->m_aFilename, sizeof(pDemo->m_aFilename), "demos/teerace/%d_%d_%d.demo", pData->m_Tick, g_Config.m_SvPort, pData->m_ClientID);
				m_lUploads.add(pDemo);
				
				// ghost
				CUpload *pGhost = new CUpload(UPLOAD_GHOST);
				pGhost->m_ClientID = pData->m_ClientID;
				pGhost->m_UserID = pData->m_UserID;
				str_format(pGhost->m_aFilename, sizeof(pGhost->m_aFilename), "ghosts/teerace/%d_%d_%d.gho", pData->m_Tick, g_Config.m_SvPort, pData->m_ClientID);
				m_lUploads.add(pGhost);
			}
		}
		pNext = pItem->m_pNext;
		delete pItem;
	}
	m_pFirst = 0;
	m_pLast = 0;
	lock_release(m_OutputLock);
	
	// uploading stuff to webapp
	for(int i = 0; i < m_lUploads.size(); i++)
	{
		CUpload *pUpload = m_lUploads[i];
		if(pUpload->m_Type == UPLOAD_DEMO)
		{
			if(!Server()->IsRecording(pUpload->m_ClientID))
			{
				CWebUpload::CParam *pParams = new CWebUpload::CParam();
				pParams->m_UserID = pUpload->m_UserID;
				str_copy(pParams->m_aFilename, pUpload->m_aFilename, sizeof(pParams->m_aFilename));
				AddJob(CWebUpload::UploadDemo, pParams);
				
				delete pUpload;
				m_lUploads.remove_index_fast(i);
			}
		}
		else if(pUpload->m_Type == UPLOAD_GHOST)
		{
			if(!Server()->IsGhostRecording(pUpload->m_ClientID))
			{
				CWebUpload::CParam *pParams = new CWebUpload::CParam();
				pParams->m_UserID = pUpload->m_UserID;
				str_copy(pParams->m_aFilename, pUpload->m_aFilename, sizeof(pParams->m_aFilename));
				AddJob(CWebUpload::UploadGhost, pParams);
				
				delete pUpload;
				m_lUploads.remove_index_fast(i);
			}
		}
	}
}

int CServerWebapp::Upload(unsigned char *pData, int Size)
{
	// send data
	int Bytes = net_tcp_send(Socket(), pData, Size);
	thread_sleep(10); // limit upload rate
	return Bytes;
}

int CServerWebapp::SendUploadHeader(const char *pHeader)
{
	net_tcp_connect(Socket(), &Addr());
	
	int Bytes = net_tcp_send(Socket(), pHeader, str_length(pHeader));
	return Bytes;
}

int CServerWebapp::SendUploadEnd()
{	
	char aEnd[256];
	str_copy(aEnd, "\r\n--frontier--\r\n", sizeof(aEnd));
	int Bytes = net_tcp_send(Socket(), aEnd, str_length(aEnd));
	net_tcp_recv(Socket(), aEnd, sizeof(aEnd));
	//dbg_msg("webapp", "\n---recv start---\n%s\n---recv end---\n", aEnd);
	net_tcp_close(Socket());
	return Bytes;
}

bool CServerWebapp::Download(const char *pFilename, const char *pURL)
{
	// TODO: limit transfer rate
	char aStr[256];
	str_format(aStr, sizeof(aStr), DOWNLOAD, pURL, ServerIP());
	
	net_tcp_connect(Socket(), &Addr());
	net_tcp_send(Socket(), aStr, str_length(aStr));
	
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
			Size = net_tcp_recv(Socket(), aBuf, sizeof(aBuf));
		
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

int CServerWebapp::MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CServerWebapp *pWebapp = (CServerWebapp*)pUser;
	int Length = str_length(pName);
	if(IsDir || Length < 4 || str_comp(pName+Length-4, ".map") != 0)
		return 0;
	
	char aBuf[256];
	str_copy(aBuf, pName, min((int)sizeof(aBuf),Length-3));
	pWebapp->m_lMapList.add(aBuf);
	
	return 0;
}

void CServerWebapp::LoadMaps()
{
	m_lMapList.clear();
	m_pServer->Storage()->ListDirectory(IStorage::TYPE_SAVE, "maps/teerace", MaplistFetchCallback, this);
}

#endif
