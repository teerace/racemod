/* Webapp class by Sushi and Redix */
#if defined(CONF_TEERACE)

#include <base/tl/algorithm.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/external/json/reader.h>
#include <engine/storage.h>

#include <game/http/http_con.h>
#include <game/http/response.h>

#include "score.h"
#include "gamecontext.h"
#include "webapp.h"

CServerWebapp::CServerWebapp(CGameContext *pGameServer)
: IWebapp(pGameServer->Server()->Storage()),
  m_pGameServer(pGameServer),
  m_pServer(pGameServer->Server()),
  m_DefaultScoring(g_Config.m_WaDefaultScoring)
{
	LoadMaps();
}

void CServerWebapp::RegisterFields(CRequest *pRequest, bool Api)
{
	if(Api)
		pRequest->AddField("API-AUTH", g_Config.m_WaApiKey);
}

void CServerWebapp::OnResponse(CHttpConnection *pCon)
{
	int Type = pCon->Type();
	CResponse *pData = pCon->Response();
	bool Error = pCon->Error() || pCon->Response()->StatusCode() != 200;

	Json::Value JsonData;
	Json::Reader Reader;
	bool Json = false;
	if(!pCon->Error() && !pData->IsFile())
		Json = Reader.parse(pData->GetBody(), pData->GetBody()+pData->Size(), JsonData);

	// TODO: add event listener (server and client)
	if(Type == WEB_USER_AUTH)
	{
		CWebUserAuthData *pUser = (CWebUserAuthData*)pCon->UserData();
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
			
			if(str_comp(pData->GetBody(), "false") != 0 && Json)
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

				char aURI[128];
				str_format(aURI, sizeof(aURI), "users/rank/%d/", UserID);
				CRequest *pRequest = CreateRequest(aURI, CRequest::HTTP_GET);
				SendRequest(pRequest, WEB_USER_RANK_GLOBAL, pUserData);
			}
			else
			{
				GameServer()->SendChatTarget(ClientID, "wrong username and/or password");
			}
		}
	}
	else if(Type == WEB_USER_FIND)
	{
		CWebUserRankData *pUser = (CWebUserRankData*)pCon->UserData();
		pUser->m_UserID = 0;
		
		if(!Error && Json)
			pUser->m_UserID = JsonData["id"].asInt();

		if(pUser->m_UserID)
		{
			str_copy(pUser->m_aName, JsonData["username"].asCString(), sizeof(pUser->m_aName));
			CWebUserRankData *pNewData = new CWebUserRankData();
			mem_copy(pNewData, pUser, sizeof(CWebUserRankData));

			char aURI[128];
			str_format(aURI, sizeof(aURI), "users/rank/%d/", pUser->m_UserID);
			CRequest *pRequest = CreateRequest(aURI, CRequest::HTTP_GET);
			SendRequest(pRequest, WEB_USER_RANK_GLOBAL, pNewData);
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
		CWebUserRankData *pUser = (CWebUserRankData*)pCon->UserData();
		pUser->m_GlobalRank = 0;

		if(!Error)
			pUser->m_GlobalRank = str_toint(pData->GetBody());

		if(pUser->m_GlobalRank)
		{
			CWebUserRankData *pNewData = new CWebUserRankData();
			mem_copy(pNewData, pUser, sizeof(CWebUserRankData));

			char aURI[128];
			str_format(aURI, sizeof(aURI), "users/map_rank/%d/%d/", pUser->m_UserID, CurrentMap()->m_ID);
			CRequest *pRequest = CreateRequest(aURI, CRequest::HTTP_GET);
			SendRequest(pRequest, WEB_USER_RANK_MAP, pNewData);
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
		CWebUserRankData *pUser = (CWebUserRankData*)pCon->UserData();
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
		CWebUserTopData *pUser = (CWebUserTopData*)pCon->UserData();
		int ClientID = pUser->m_ClientID;
		if(GameServer()->m_apPlayers[ClientID])
		{
			char aBuf[256];
			GameServer()->SendChatTarget(ClientID, "----------- Top 5 -----------");
			for(unsigned int i = 0; i < JsonData.size() && i < 5; i++)
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
		bool Online = !Error && str_comp(pData->GetBody(), "\"PONG\"") == 0;
		dbg_msg("webapp", "webapp is%s online", Online ? "" : " not");
		if(Online)
		{
			CRequest *pRequest = CreateRequest("maps/list/", CRequest::HTTP_GET);
			SendRequest(pRequest, WEB_MAP_LIST);
		}
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
				Download(aFilename, Map["get_download_url"].asCString(), WEB_DOWNLOAD_MAP);
			}
			else if(DoCrcCheck) // map found... check crc
			{
				str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
				CDataFileReader DataFile;
				if(!DataFile.Open(m_pServer->Storage(), aFilename, IStorage::TYPE_SAVE))
				{
					str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
					Download(aFilename, Map["get_download_url"].asCString(), WEB_DOWNLOAD_MAP);
				}
				else
				{
					char aCrc[16];
					str_format(aCrc, sizeof(aCrc), "%x", DataFile.Crc());
					if(str_comp(aCrc, Map["crc"].asCString()))
					{
						str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
						Download(aFilename, Map["get_download_url"].asCString(), WEB_DOWNLOAD_MAP);
					}
					DataFile.Close();
				}
			}
		}
	}
	else if(Type == WEB_DOWNLOAD_MAP)
	{
		const char *pMap = pData->GetFilename();
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
		{
			Storage()->RemoveFile(pData->GetPath(), IStorage::TYPE_SAVE);
			dbg_msg("webapp", "could not download map: %s", aMap);
		}
	}
	else if(Type == WEB_RUN_POST)
	{
		CWebRunData *pUser = (CWebRunData*)pCon->UserData();
		if(pUser->m_Tick > -1)
		{
			char aFilename[256];
			char aURL[128];

			// TODO: upload
			// demo
			/*str_format(aFilename, sizeof(aFilename), "demos/teerace/%d_%d_%d.demo", pUser->m_Tick, g_Config.m_SvPort, pUser->m_ClientID);
			str_format(aURL, sizeof(aURL), "files/demo/%d/%d/", pUser->m_UserID, CurrentMap()->m_ID);
			Upload(aFilename, aURL, WEB_UPLOAD_DEMO, "demo_file", 0, time_get()+time_freq()*2);*/

			// ghost
			/*str_format(aFilename, sizeof(aFilename), "ghosts/teerace/%d_%d_%d.gho", pUser->m_Tick, g_Config.m_SvPort, pUser->m_ClientID);
			str_format(aURL, sizeof(aURL), "files/ghost/%d/%d/", pUser->m_UserID, CurrentMap()->m_ID);
			Upload(aFilename, aURL, WEB_UPLOAD_GHOST, "ghost_file");*/
		}
	}
	else if(Type == WEB_UPLOAD_DEMO || Type == WEB_UPLOAD_GHOST)
	{
		if(!Error)
			dbg_msg("webapp", "uploaded file: %s", pCon->Request()->GetFilename());
		else
			dbg_msg("webapp", "could not upload file: %s", pCon->Request()->GetFilename());
		Storage()->RemoveFile(pCon->Request()->GetPath(), IStorage::TYPE_SAVE);
	}
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
	Storage()->ListDirectory(IStorage::TYPE_SAVE, "maps/teerace", MaplistFetchCallback, this);
}

#endif
