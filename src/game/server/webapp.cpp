/* Webapp class by Sushi and Redix */
#if defined(CONF_TEERACE)

#include <base/tl/algorithm.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <engine/external/json/reader.h>

#include "score.h"
#include "gamecontext.h"
#include "webapp.h"

const char CServerWebapp::GET[] = "GET %s/%s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nConnection: close\r\n\r\n";
const char CServerWebapp::POST[] = "POST %s/%s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s";
const char CServerWebapp::PUT[] = "PUT %s/%s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s";
const char CServerWebapp::DOWNLOAD[] = "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";
const char CServerWebapp::UPLOAD[] = "POST %s/%s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nContent-Type: multipart/form-data; boundary=frontier\r\nContent-Length: %d\r\n\r\n--frontier\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"file.file\"\r\nContent-Type: application/octet-stream\r\n\r\n";

CServerWebapp::CServerWebapp(CGameContext *pGameServer)
: IWebapp(g_Config.m_SvWebappIp),
  m_pGameServer(pGameServer),
  m_pServer(pGameServer->Server()),
  m_DefaultScoring(g_Config.m_SvDefaultScoring)
{
	m_Online = 0;
	m_lUploads.delete_all();
	LoadMaps();
}

CServerWebapp::~CServerWebapp()
{
	m_lMapList.clear();
	m_lUploads.delete_all();
}

const char *CServerWebapp::ApiKey() { return g_Config.m_SvApiKey; }
const char *CServerWebapp::ServerIP() { return g_Config.m_SvWebappIp; }
const char *CServerWebapp::ApiPath() { return g_Config.m_SvApiPath; }

void CServerWebapp::Update()
{
	int Jobs = IWebapp::Update();
	if(Jobs > 0)
		dbg_msg("webapp", "removed %d jobs", Jobs);
}

void CServerWebapp::OnResponse(int Type, IStream *pData, void *pUserData)
{
	Json::Value JsonData;
	Json::Reader Reader;
	bool Json = false;
	if(pData->Readable())
		Json = Reader.parse(pData->GetData(), pData->GetData()+pData->Size(), JsonData);

	// TODO: add event listener (server and client)
	if(Type == WEB_USER_AUTH)
	{
		int *pUser = (int*)pUserData;
		int ClientID = pUser[0];
		if(GameServer()->m_apPlayers[ClientID])
		{
			int SendRconCmds = pUser[1];
			int UserID = 0;
			
			if(str_comp(pData->GetData(), "false") != 0)
			{
				if(Json)
					UserID = JsonData["id"].asInt();
			}
			
			if(UserID > 0)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "%s has logged in as %s", Server()->ClientName(ClientID), JsonData["username"].asCString());
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
				Server()->SetUserID(ClientID, UserID);
				Server()->SetUserName(ClientID, JsonData["username"].asCString());
				
				// auth staff members
				if(JsonData["is_staff"].asBool())
					Server()->StaffAuth(ClientID, SendRconCmds);
				
				/*CWebUser::CParam *pParams = new CWebUser::CParam();
				str_copy(pParams->m_aName, Server()->GetUserName(ClientID), sizeof(pParams->m_aName));
				pParams->m_ClientID = ClientID;
				pParams->m_UserID = UserID;
				pParams->m_GetBestRun = 1;
				AddJob(CWebUser::GetRank, pParams);*/
			}
			else
			{
				GameServer()->SendChatTarget(ClientID, "wrong username and/or password");
			}
		}
	}
	/*else if(Type == WEB_USER_RANK)
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
	}*/
	else if(Type == WEB_USER_TOP)
	{
		if(!Json)
			return;
		
		int *pUser = (int*)pUserData;
		if(GameServer()->m_apPlayers[pUser[1]])
		{
			char aBuf[256];
			GameServer()->SendChatTarget(pUser[1], "----------- Top 5 -----------");
			for(int i = 0; i < JsonData.size() && i < 5; i++)
			{
				Json::Value Run = JsonData[i];
				float Time = str_tofloat(Run["run"]["time"].asCString());
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %.3f second(s)",
					i+pUser[0], Run["run"]["user"]["username"].asCString(), (int)Time/60, fmod(Time, 60));
				GameServer()->SendChatTarget(pUser[1], aBuf);
			}
			GameServer()->SendChatTarget(pUser[1], "------------------------------");
		}
	}
	else if(Type == WEB_PING_PING)
	{
		m_Online = str_comp(pData->GetData(), "\"PONG\"") == 0;
		dbg_msg("webapp", "webapp is%s online", IsOnline() ? "" : " not");
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), CServerWebapp::GET, ApiPath(), "maps/list/", ServerIP(), ApiKey());
		SendRequest(aBuf, WEB_MAP_LIST, new CBufferStream());
	}
	else if(Type == WEB_MAP_LIST)
	{
		if(!Json)
			return;
		
		bool DoCrcCheck = !GameServer()->CrcCheck();
		char aFilename[256];
		const char *pPath = "maps/teerace/%s.map";
		
		GameServer()->CheckedCrc();
		
		for(unsigned int i = 0; i < JsonData.size(); i++)
		{
			Json::Value Map = JsonData[i];
			
			if(!DefaultScoring() && Map["get_best_score"].type() && !str_comp(Map["name"].asCString(), g_Config.m_SvMap))
			{
				float Time = str_tofloat(Map["get_best_score"]["time"].asCString());
				float aCheckpointTimes[25] = {0.0f};
				Json::Value Checkpoint = Map["get_best_score"]["checkpoints_list"];
				for(unsigned int j = 0; j < Checkpoint.size(); j++)
					aCheckpointTimes[j] = str_tofloat(Checkpoint[j].asCString());
				
				GameServer()->Score()->GetRecord()->Set(Time, aCheckpointTimes);
				m_CurrentMap.m_ID = Map["id"].asInt();
				m_CurrentMap.m_RunCount = Map["run_count"].asInt();
				str_copy(m_CurrentMap.m_aCrc, Map["crc"].asCString(), sizeof(m_CurrentMap.m_aCrc));
				str_copy(m_CurrentMap.m_aURL, Map["get_download_url"].asCString(), sizeof(m_CurrentMap.m_aURL));
				str_copy(m_CurrentMap.m_aAuthor, Map["author"].asCString(), sizeof(m_CurrentMap.m_aAuthor));
				continue;
			}
			
			array<std::string>::range r = find_linear(m_lMapList.all(), Map["name"].asString());
			if(r.empty())
			{
				str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
				char *pMapName = (char *)mem_alloc(Map["name"].asString().length()+1, 1);
				str_copy(pMapName, Map["name"].asCString(), Map["name"].asString().length()+1);
				Download(aFilename, Map["get_download_url"].asCString(), WEB_MAP_DOWNLOADED, pMapName);
			}
			else if(DoCrcCheck) // map found... check crc
			{
				str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
				CDataFileReader DataFile;
				if(!DataFile.Open(m_pServer->Storage(), aFilename, IStorage::TYPE_SAVE))
				{
					str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
					char *pMapName = (char *)mem_alloc(Map["name"].asString().length()+1, 1);
					str_copy(pMapName, Map["name"].asCString(), Map["name"].asString().length()+1);
					Download(aFilename, Map["get_download_url"].asCString(), WEB_MAP_DOWNLOADED, pMapName);
				}
				else
				{
					char aCrc[16];
					str_format(aCrc, sizeof(aCrc), "%x", DataFile.Crc());
					if(str_comp(aCrc, Map["crc"].asCString()))
					{
						str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
						char *pMapName = (char *)mem_alloc(Map["name"].asString().length()+1, 1);
						str_copy(pMapName, Map["name"].asCString(), Map["name"].asString().length()+1);
						Download(aFilename, Map["get_download_url"].asCString(), WEB_MAP_DOWNLOADED, pMapName);
					}
					DataFile.Close();
				}
			}
		}
	}
	else if(Type == WEB_MAP_DOWNLOADED)
	{
		const char *pMap = (const char*)pUserData;
		m_lMapList.add(pMap);
		dbg_msg("webapp", "added map: %s", pMap);
		if(str_comp(pMap, g_Config.m_SvMap) == 0)
			Server()->ReloadMap();
	}
	/*else if(Type == WEB_RUN)
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
	}*/
	
	// uploading stuff to webapp
	/*for(int i = 0; i < m_lUploads.size(); i++)
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
	}*/
}

/*int CServerWebapp::Upload(unsigned char *pData, int Size)
{
	// send data
	/*int Bytes = net_tcp_send(Socket(), pData, Size);
	thread_sleep(10); // limit upload rate
	return Bytes;
}

int CServerWebapp::SendUploadHeader(const char *pHeader)
{
	NETADDR Address = Addr();
	net_tcp_connect(Socket(), &Address);
	
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
}*/

bool CServerWebapp::Download(const char *pFilename, const char *pURL, int Type, void *pUserData)
{
	// TODO: limit transfer rate
	char aStr[256];
	str_format(aStr, sizeof(aStr), DOWNLOAD, pURL, ServerIP());
	
	IOHANDLE File = m_pServer->Storage()->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	
	return SendRequest(aStr, Type, new CFileStream(File), pUserData);
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
