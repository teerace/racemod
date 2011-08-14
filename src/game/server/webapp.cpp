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

void CServerWebapp::OnResponse(int Type, IStream *pData, void *pUserData, int StatusCode)
{
	bool Error = StatusCode != 200;
	Json::Value JsonData;
	Json::Reader Reader;
	bool Json = false;
	if(StatusCode != -1 && !pData->IsFile())
		Json = Reader.parse(pData->GetData(), pData->GetData()+pData->Size(), JsonData);

	// TODO: add event listener (server and client)
	if(Type == WEB_USER_AUTH)
	{
		CWebUserAuthData *pUser = (CWebUserAuthData*)pUserData;
		int ClientID = pUser->m_ClientID;
		if(GameServer()->m_apPlayers[ClientID])
		{
			if(Error)
			{
				GameServer()->SendChatTarget(ClientID, "unknown error");
				return;
			}

			int SendRconCmds = pUser->m_SendRconCmds;
			int UserID = 0;
			
			if(str_comp(pData->GetData(), "false") != 0 && Json)
				UserID = JsonData["id"].asInt();
			
			if(UserID > 0)
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s has logged in as %s", Server()->ClientName(ClientID), JsonData["username"].asCString());
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
				Server()->SetUserID(ClientID, UserID);
				Server()->SetUserName(ClientID, JsonData["username"].asCString());
				
				// auth staff members
				if(JsonData["is_staff"].asBool())
					Server()->StaffAuth(ClientID, SendRconCmds);

				GameServer()->m_apPlayers[ClientID]->m_RequestedBestTime = true;

				CWebUserRankData *pUserData = new CWebUserRankData();
				str_copy(pUserData->m_aName, Server()->GetUserName(ClientID), sizeof(pUserData->m_aName));
				pUserData->m_ClientID = ClientID;
				pUserData->m_UserID = UserID;

				char aURL[128];
				str_format(aURL, sizeof(aURL), "users/rank/%d/", UserID);
				str_format(aBuf, sizeof(aBuf), CServerWebapp::GET, ApiPath(), aURL, ServerIP(), ApiKey());
				SendRequest(aBuf, WEB_USER_RANK_GLOBAL, new CBufferStream(), pUserData);
			}
			else
			{
				GameServer()->SendChatTarget(ClientID, "wrong username and/or password");
			}
		}
	}
	else if(Type == WEB_USER_FIND)
	{
		CWebUserRankData *pUser = (CWebUserRankData*)pUserData;
		pUser->m_UserID = 0;
		
		if(!Error && Json)
			pUser->m_UserID = JsonData["id"].asInt();

		if(pUser->m_UserID)
		{
			str_copy(pUser->m_aName, JsonData["username"].asCString(), sizeof(pUser->m_aName));
			CWebUserRankData *pNewData = new CWebUserRankData();
			mem_copy(pNewData, pUser, sizeof(CWebUserRankData));

			char aBuf[512];
			char aURL[128];
			str_format(aURL, sizeof(aURL), "users/rank/%d/", pUser->m_UserID);
			str_format(aBuf, sizeof(aBuf), CServerWebapp::GET, ApiPath(), aURL, ServerIP(), ApiKey());
			SendRequest(aBuf, WEB_USER_RANK_GLOBAL, new CBufferStream(), pNewData);
		}
		else if(pUser->m_PrintRank)
		{
			if(GameServer()->m_apPlayers[pUser->m_ClientID])
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "No match found for \"%s\".", pUser->m_aName);
				GameServer()->SendChatTarget(pUser->m_ClientID, aBuf);
			}
		}
	}
	else if(Type == WEB_USER_RANK_GLOBAL)
	{
		CWebUserRankData *pUser = (CWebUserRankData*)pUserData;
		pUser->m_GlobalRank = 0;

		if(!Error)
			pUser->m_GlobalRank = str_toint(pData->GetData());

		if(pUser->m_GlobalRank)
		{
			CWebUserRankData *pNewData = new CWebUserRankData();
			mem_copy(pNewData, pUser, sizeof(CWebUserRankData));

			char aBuf[512];
			char aURL[128];
			str_format(aURL, sizeof(aURL), "users/map_rank/%d/%d/", pUser->m_UserID, CurrentMap()->m_ID);
			str_format(aBuf, sizeof(aBuf), CServerWebapp::GET, ApiPath(), aURL, ServerIP(), ApiKey());
			SendRequest(aBuf, WEB_USER_RANK_MAP, new CBufferStream(), pNewData);
		}
		else if(pUser->m_PrintRank)
		{
			if(pUser->m_UserID == Server()->GetUserID(pUser->m_ClientID))
				GameServer()->SendChatTarget(pUser->m_ClientID, "You are not globally ranked yet.");
			else
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s is not globally ranked yet.", pUser->m_aName);
				GameServer()->SendChatTarget(pUser->m_ClientID, aBuf);
			}
		}
	}
	else if(Type == WEB_USER_RANK_MAP)
	{
		CWebUserRankData *pUser = (CWebUserRankData*)pUserData;
		int GlobalRank = pUser->m_GlobalRank;
		int MapRank = 0;
		CPlayerData Run;

		if(!Error && Json)
		{
			MapRank = JsonData["position"].asInt();
			if(MapRank && !DefaultScoring())
			{
				float Time = str_tofloat(JsonData["bestrun"]["time"].asCString());
				float aCheckpointTimes[25] = {0.0f};
				Json::Value Checkpoint = JsonData["bestrun"]["checkpoints_list"];
				for(unsigned int i = 0; i < Checkpoint.size(); i++)
					aCheckpointTimes[i] = str_tofloat(Checkpoint[i].asCString());
				Run.Set(Time, aCheckpointTimes);
			}
		}

		if(GameServer()->m_apPlayers[pUser->m_ClientID])
		{
			bool Own = pUser->m_UserID == Server()->GetUserID(pUser->m_ClientID);
			if(Own && GlobalRank && MapRank && !DefaultScoring())
			{
				GameServer()->m_apPlayers[pUser->m_ClientID]->m_GlobalRank = GlobalRank;
				GameServer()->m_apPlayers[pUser->m_ClientID]->m_MapRank = MapRank;
				GameServer()->Score()->PlayerData(pUser->m_ClientID)->Set(Run.m_Time, Run.m_aCpTime);
			}

			if(pUser->m_PrintRank)
			{
				char aBuf[256];
				if(!MapRank)
					str_format(aBuf, sizeof(aBuf), "%s: Global Rank: %d | Map Rank: Not ranked yet (%s)",
						pUser->m_aName, GlobalRank, Server()->ClientName(pUser->m_ClientID));
				else
				{
					if(Run.m_Time < 60.0f)
						str_format(aBuf, sizeof(aBuf), "%s: Global Rank: %d | Map Rank: %d | Time: %.3f (%s)",
							pUser->m_aName, GlobalRank, MapRank, Run.m_Time, Server()->ClientName(pUser->m_ClientID));
					else
						str_format(aBuf, sizeof(aBuf), "%s: Global Rank: %d | Map Rank: %d | Time: %02d:%06.3f (%s)",
							pUser->m_aName, GlobalRank, MapRank, (int)Run.m_Time/60,
							fmod(Run.m_Time, 60), Server()->ClientName(pUser->m_ClientID));
				}
				
				if(g_Config.m_SvShowTimes)
					GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
				else
					GameServer()->SendChatTarget(pUser->m_ClientID, aBuf);
			}
		}
	}
	else if(Type == WEB_USER_TOP && Json && !Error)
	{
		CWebUserTopData *pUser = (CWebUserTopData*)pUserData;
		int ClientID = pUser->m_ClientID;
		if(GameServer()->m_apPlayers[ClientID])
		{
			char aBuf[256];
			GameServer()->SendChatTarget(ClientID, "----------- Top 5 -----------");
			for(int i = 0; i < JsonData.size() && i < 5; i++)
			{
				Json::Value Run = JsonData[i];
				float Time = str_tofloat(Run["run"]["time"].asCString());
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %.3f second(s)",
					i+pUser->m_StartRank, Run["run"]["user"]["username"].asCString(), (int)Time/60, fmod(Time, 60));
				GameServer()->SendChatTarget(ClientID, aBuf);
			}
			GameServer()->SendChatTarget(ClientID, "------------------------------");
		}
	}
	else if(Type == WEB_PING_PING)
	{
		m_Online = !Error && str_comp(pData->GetData(), "\"PONG\"") == 0;
		dbg_msg("webapp", "webapp is%s online", m_Online ? "" : " not");
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), CServerWebapp::GET, ApiPath(), "maps/list/", ServerIP(), ApiKey());
		SendRequest(aBuf, WEB_MAP_LIST, new CBufferStream());
	}
	else if(Type == WEB_MAP_LIST && Json && !Error)
	{
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
				Download(aFilename, Map["get_download_url"].asCString(), WEB_MAP_DOWNLOADED);
			}
			else if(DoCrcCheck) // map found... check crc
			{
				str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
				CDataFileReader DataFile;
				if(!DataFile.Open(m_pServer->Storage(), aFilename, IStorage::TYPE_SAVE))
				{
					str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
					Download(aFilename, Map["get_download_url"].asCString(), WEB_MAP_DOWNLOADED);
				}
				else
				{
					char aCrc[16];
					str_format(aCrc, sizeof(aCrc), "%x", DataFile.Crc());
					if(str_comp(aCrc, Map["crc"].asCString()))
					{
						str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
						Download(aFilename, Map["get_download_url"].asCString(), WEB_MAP_DOWNLOADED);
					}
					DataFile.Close();
				}
			}
		}
	}
	else if(Type == WEB_MAP_DOWNLOADED)
	{
		const char *pMap = ((CFileStream*)pData)->GetFilename();
		char aMap[256];
		str_copy(aMap, pMap, min((int)sizeof(aMap),str_length(pMap)-3));

		if(!Error)
		{
			m_lMapList.add(aMap);
			dbg_msg("webapp", "added map: %s", aMap);
			if(str_comp(pMap, g_Config.m_SvMap) == 0)
				Server()->ReloadMap();
		}
		else
			dbg_msg("webapp", "couldn't download map: %s", aMap);
	}
	else if(Type == WEB_RUN_POST)
	{
		CWebRunData *pUser = (CWebRunData*)pUserData;
		if(pUser->m_Tick > -1)
		{
			// demo
			/*CUpload *pDemo = new CUpload(UPLOAD_DEMO);
			pDemo->m_ClientID = pUser[1];
			pDemo->m_UserID = pUser[0];
			str_format(pDemo->m_aFilename, sizeof(pDemo->m_aFilename), "demos/teerace/%d_%d_%d.demo", pUser[2], g_Config.m_SvPort, pUser[1]);
			m_lUploads.add(pDemo);
			
			// ghost
			CUpload *pGhost = new CUpload(UPLOAD_GHOST);
			pGhost->m_ClientID = pUser[1];
			pGhost->m_UserID = pUser[0];
			str_format(pGhost->m_aFilename, sizeof(pGhost->m_aFilename), "ghosts/teerace/%d_%d_%d.gho", pUser[2], g_Config.m_SvPort, pUser[1]);
			m_lUploads.add(pGhost);*/
		}
	}
	
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
	char aStr[256];
	str_format(aStr, sizeof(aStr), DOWNLOAD, pURL, ServerIP());
	return SendRequest(aStr, Type, new CFileStream(pFilename, m_pServer->Storage()), pUserData);
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
