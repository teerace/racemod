/* Webapp class by Sushi and Redix */
#if defined(CONF_TEERACE)

#include <stdio.h>

#include <base/tl/algorithm.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/external/json/reader.h>
#include <engine/storage.h>

#include <game/http/response.h>

#include "score/wa_score.h"
#include "gamecontext.h"
#include "webapp.h"

CServerWebapp::CServerWebapp(CGameContext *pGameServer)
: IWebapp(pGameServer->Server()->Storage()),
  m_pGameServer(pGameServer),
  m_pServer(pGameServer->Server())
{
	// load maps
	Storage()->ListDirectory(IStorage::TYPE_SAVE, "maps/teerace", MaplistFetchCallback, this);
	m_lUploads.clear();
}

void CServerWebapp::RegisterFields(CRequest *pRequest, bool Api)
{
	if(Api)
		pRequest->AddField("API-AUTH", g_Config.m_WaApiKey);
}

void CServerWebapp::OnResponse(CHttpConnection *pCon)
{
	int Type = pCon->Type();
	CResponse *pResponse = pCon->Response();
	bool Error = pCon->Error() || pResponse->StatusCode() != 200;

	Json::Value JsonData;
	Json::Reader Reader;
	bool Json = false;
	if(!pCon->Error() && !pResponse->IsFile())
		Json = Reader.parse(pResponse->GetBody(), pResponse->GetBody()+pResponse->Size(), JsonData);

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
			
			if(str_comp(pResponse->GetBody(), "false") != 0 && Json)
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

				((CWebappScore*)GameServer()->Score())->LoadScore(ClientID, true);
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
			pUser->m_GlobalRank = str_toint(pResponse->GetBody());

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
			if(MapRank)
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
			if(Own && GlobalRank && MapRank)
				GameServer()->Score()->PlayerData(pUser->m_ClientID)->Set(Run.m_Time, Run.m_aCpTime);

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
			float LastTime = 0.0f;
			int SameTimeCount = 0;
			GameServer()->SendChatTarget(ClientID, "----------- Top 5 -----------");
			for(unsigned int i = 0; i < JsonData.size() && i < 5; i++)
			{
				Json::Value Run = JsonData[i];
				float Time = str_tofloat(Run["run"]["time"].asCString());

				if(Time == LastTime)
					SameTimeCount++;
				else
					SameTimeCount = 0;

				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %.3f second(s)",
					i+pUser->m_StartRank-SameTimeCount, Run["run"]["user"]["username"].asCString(),(int)Time/60, fmod(Time, 60));
				GameServer()->SendChatTarget(ClientID, aBuf);

				LastTime = Time;
			}
			GameServer()->SendChatTarget(ClientID, "------------------------------");
		}
	}
	else if(Type == WEB_PING_PING)
	{
		bool Online = !Error && str_comp(pResponse->GetBody(), "\"PONG\"") == 0;
		dbg_msg("webapp", "webapp is%s online", Online ? "" : " not");
		if(Online)
		{
			CRequest *pRequest = CreateRequest("maps/list/", CRequest::HTTP_GET);
			SendRequest(pRequest, WEB_MAP_LIST);
		}
	}
	else if(Type == WEB_MAP_LIST && Json && !Error)
	{
		char aFilename[256];
		const char *pPath = "maps/teerace/%s.map";
		bool Change = false;
		
		for(unsigned int i = 0; i < JsonData.size(); i++)
		{
			Json::Value Map = JsonData[i];
			if(!Map["crc"].isString())
				continue; // skip maps without crc

			CMapInfo Info;
			str_copy(Info.m_aName, Map["name"].asCString(), sizeof(Info.m_aName));
			sscanf(Map["crc"].asCString(), "%08x", &Info.m_Crc);
			
			if(Map["get_best_score"].type() && !str_comp(Info.m_aName, g_Config.m_SvMap))
			{
				float Time = str_tofloat(Map["get_best_score"]["time"].asCString());
				float aCheckpointTimes[25] = {0.0f};
				Json::Value Checkpoint = Map["get_best_score"]["checkpoints_list"];
				for(unsigned int j = 0; j < Checkpoint.size(); j++)
					aCheckpointTimes[j] = str_tofloat(Checkpoint[j].asCString());
				GameServer()->Score()->GetRecord()->Set(Time, aCheckpointTimes);
			}

			array<CMapInfo>::range r = find_linear(m_lMapList.all(), Info);
			if(r.empty() || r.front().m_Crc != Info.m_Crc)
			{
				str_format(aFilename, sizeof(aFilename), pPath, Map["name"].asCString());
				Download(aFilename, Map["get_download_url"].asCString(), WEB_DOWNLOAD_MAP);
				if(!r.empty())
					m_lMapList.remove_fast(r.front());
			}
			else if(r.front().m_ID == -1)
			{
				r.front().m_ID = Map["id"].asInt();
				r.front().m_RunCount = Map["run_count"].asInt();
				str_copy(r.front().m_aURL, Map["get_download_url"].asCString(), sizeof(r.front().m_aURL));
				str_copy(r.front().m_aAuthor, Map["author"].asCString(), sizeof(r.front().m_aAuthor));
				dbg_msg("webapp", "added map info: %s (%d)", r.front().m_aName, r.front().m_ID);
				Change = true;
			}
		}

		if(Change)
			OnInit();
	}
	else if(Type == WEB_DOWNLOAD_MAP)
	{
		if(!Error)
		{
			CMapInfo *pInfo = AddMap(pResponse->GetFilename());
			if(pInfo && str_comp(pInfo->m_aName, g_Config.m_SvMap) == 0)
				Server()->ReloadMap();
		}
		else
		{
			Storage()->RemoveFile(pResponse->GetPath(), IStorage::TYPE_SAVE);
			dbg_msg("webapp", "could not download map: %s", pResponse->GetFilename());
		}
	}
	else if(Type == WEB_RUN_POST)
	{
		CWebRunData *pUser = (CWebRunData*)pCon->UserData();
		if(pUser->m_Tick > -1)
		{
			char aFilename[256];
			char aURL[128];

			str_format(aFilename, sizeof(aFilename), "demos/teerace/%d_%d_%d.demo", pUser->m_Tick, g_Config.m_SvPort, pUser->m_ClientID);
			str_format(aURL, sizeof(aURL), "files/demo/%d/%d/", pUser->m_UserID, CurrentMap()->m_ID);
			AddUpload(aFilename, aURL, "demo_file", WEB_UPLOAD_DEMO, time_get()+time_freq()*2);

			str_format(aFilename, sizeof(aFilename), "ghosts/teerace/%d_%d_%d.gho", pUser->m_Tick, g_Config.m_SvPort, pUser->m_ClientID);
			str_format(aURL, sizeof(aURL), "files/ghost/%d/%d/", pUser->m_UserID, CurrentMap()->m_ID);
			AddUpload(aFilename, aURL, "ghost_file", WEB_UPLOAD_GHOST);
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
	if(!IsDir && Length >= 4 && str_comp(pName+Length-4, ".map") == 0)
		pWebapp->AddMap(pName);
	return 0;
}

CServerWebapp::CMapInfo *CServerWebapp::AddMap(const char *pFilename)
{
	char aFile[256];
	str_format(aFile, sizeof(aFile), "maps/teerace/%s", pFilename);
	CDataFileReader DataFile;
	if(!DataFile.Open(m_pServer->Storage(), aFile, IStorage::TYPE_SAVE))
		return 0;

	CMapInfo Info;
	Info.m_Crc = DataFile.Crc();
	DataFile.Close();
	str_copy(Info.m_aName, pFilename, min((int)sizeof(Info.m_aName),str_length(pFilename)-3));
	dbg_msg("", "added map: %s (%08x)", Info.m_aName, Info.m_Crc);
	int Num = m_lMapList.add(Info);
	return &m_lMapList[Num];
}

void CServerWebapp::OnInit()
{
	m_CurrentMap.m_ID = -1;
	str_copy(m_CurrentMap.m_aName, g_Config.m_SvMap, sizeof(m_CurrentMap.m_aName));
	array<CMapInfo>::range r = find_linear(m_lMapList.all(), m_CurrentMap);
	if(!r.empty())
	{
		m_CurrentMap = r.front();
		dbg_msg("webapp", "current map: %s (%d)", m_CurrentMap.m_aName, m_CurrentMap.m_ID);
	}
}

void CServerWebapp::Tick()
{
	// do uploads
	for(int i = 0; i < m_lUploads.size(); i++)
	{
		if(m_lUploads[i].m_StartTime <= time_get())
		{
			Upload(m_lUploads[i].m_aFilename, m_lUploads[i].m_aURL, m_lUploads[i].m_aUploadname, m_lUploads[i].m_Type);
			m_lUploads.remove_index_fast(i);
			i--; // since one item was removed
		}
	}
}

void CServerWebapp::AddUpload(const char *pFilename, const char *pURL, const char *pUploadName, int Type, int64 StartTime)
{
	m_lUploads.add(CUpload(pFilename, pURL, pUploadName, Type, StartTime));
}

#endif
