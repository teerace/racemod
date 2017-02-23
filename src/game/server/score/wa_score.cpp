#if defined(CONF_TEERACE)

#include <engine/external/json-parser/json-builder.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>

#include <game/teerace.h>

#include "../webapp.h"
#include "../gamecontext.h"
#include "wa_score.h"

void FormatSeconds(char *pBuf, int Size, int Time)
{
	str_format(pBuf, Size, "%d.%03d", Time / 1000, Time % 1000);
}

CWebappScore::CWebappScore(CGameContext *pGameServer) : m_pGameServer(pGameServer), m_GotRecord(false) { }

IServer *CWebappScore::Server() { return m_pGameServer->Server(); }
CServerWebapp *CWebappScore::Webapp() { return m_pGameServer->Webapp(); }

void CWebappScore::OnMapLoad()
{
	IScore::OnMapLoad();
	m_GotRecord = false;
}

void CWebappScore::OnPlayerInit(int ClientID, bool PrintRank)
{
	m_aPlayerData[ClientID].Reset();

	int UserID = Server()->GetUserID(ClientID);
	int MapID = Webapp()->CurrentMap()->m_ID;
	if(Webapp() && MapID > -1 && UserID > 0)
	{
		CUserRankData *pUserData = new CUserRankData(this);
		str_copy(pUserData->m_aName, Server()->GetUserName(ClientID), sizeof(pUserData->m_aName));
		pUserData->m_ClientID = ClientID;
		pUserData->m_UserID = UserID;
		pUserData->m_MapID = MapID;
		pUserData->m_PrintRank = PrintRank;

		char aURI[128];
		str_format(aURI, sizeof(aURI), "/users/rank/%d/", UserID);
		CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_GET, aURI);
		CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
		pInfo->SetCallback(OnUserRankGlobal, pUserData);
		Server()->SendHttp(pInfo, pRequest);
	}
}

void CWebappScore::OnPlayerFinish(int ClientID, int Time, int *pCpTime)
{
	bool NewPlayerRecord = m_aPlayerData[ClientID].UpdateTime(Time, pCpTime);
	if(!Webapp() || Webapp()->CurrentMap()->m_ID == -1)
		return;

	CRunData *pUserData = new CRunData(this);
	pUserData->m_UserID = Server()->GetUserID(ClientID);
	pUserData->m_MapID = Webapp()->CurrentMap()->m_ID;
	pUserData->m_ClientID = ClientID;
	pUserData->m_Tick = -1;

	if(NewPlayerRecord && Server()->GetUserID(ClientID) > 0)
	{
		if(m_GotRecord && UpdateRecord(Time) && g_Config.m_SvShowTimes)
			GameServer()->SendRecord(-1);
		// set demo and ghost so that it is saved
		Server()->SaveGhostAndDemo(ClientID);
		pUserData->m_Tick = Server()->Tick();
	}

	json_value *pData = json_object_new(0);
	char aBuf[1024];
	char aBuf2[32];
	json_object_push(pData, "map_id", json_integer_new(Webapp()->CurrentMap()->m_ID));
	str_format(aBuf, sizeof(aBuf), "%08x", Webapp()->CurrentMap()->m_Crc);
	json_object_push(pData, "map_crc", json_string_new(aBuf));
	json_object_push(pData, "user_id", json_integer_new(Server()->GetUserID(ClientID)));
	str_copy(aBuf, Server()->ClientName(ClientID), MAX_NAME_LENGTH);
	str_sanitize_strong(aBuf);
	json_object_push(pData, "nickname", json_string_new(aBuf));
	if(Server()->ClientClan(ClientID)[0])
	{
		str_copy(aBuf, Server()->ClientClan(ClientID), MAX_CLAN_LENGTH);
		str_sanitize_strong(aBuf);
		json_object_push(pData, "clan", json_string_new(aBuf));
	}
	FormatSeconds(aBuf, sizeof(aBuf), Time);
	json_object_push(pData, "time", json_string_new(aBuf));
	FormatSeconds(aBuf, sizeof(aBuf), pCpTime[0]);
	for(int i = 1; i < NUM_CHECKPOINTS; i++)
	{
		str_append(aBuf, ";", sizeof(aBuf));
		FormatSeconds(aBuf2, sizeof(aBuf2), pCpTime[i]);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	json_object_push(pData, "checkpoints", json_string_new(aBuf));

	char *pJson = new char[json_measure(pData)];
	json_serialize(pJson, pData);
	json_builder_free(pData);

	CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_POST, "/runs/new/");
	pRequest->SetBody(pJson, str_length(pJson), "application/json");
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	pInfo->SetCallback(OnRunPost, pUserData);
	Server()->SendHttp(pInfo, pRequest);
	delete[] pJson;

	// higher run count
	Webapp()->CurrentMap()->m_RunCount++;
}

void CWebappScore::ShowTop5(int ClientID, int Debut)
{
	if(!Webapp())
		return;

	if(Webapp()->CurrentMap()->m_ID > -1)
	{
		CUserTopData *aNameData = new CUserTopData(this);
		aNameData->m_StartRank = Debut;
		aNameData->m_ClientID = ClientID;

		char aURI[128];
		str_format(aURI, sizeof(aURI), "/maps/rank/%d/%d/", Webapp()->CurrentMap()->m_ID, Debut);
		CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_GET, aURI);
		CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
		pInfo->SetCallback(OnUserTop, aNameData);
		Server()->SendHttp(pInfo, pRequest);
	}
	else if(ClientID >= 0)
		GameServer()->SendChatTarget(ClientID, "This map is not a teerace map.");
}

void CWebappScore::ShowRank(int ClientID, const char *pName, bool Search)
{
	if(!Webapp())
		return;

	if(Webapp()->CurrentMap()->m_ID > -1)
	{
		int SearchID = -1;
		// search for players on the server
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			// search for 100% match
			if(GameServer()->m_apPlayers[i] && Server()->GetUserID(i) > 0 && (!str_comp(Server()->ClientName(i), pName) || !str_comp(Server()->GetUserName(i), pName)))
			{
				SearchID = i;
				break;
			}
		}
						
		if(SearchID < 0 && Search)
		{
			// search for players on the server
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				// search for part match
				if(GameServer()->m_apPlayers[i] && Server()->GetUserID(i) > 0 && (str_find_nocase(Server()->ClientName(i), pName) || str_find_nocase(Server()->GetUserName(i), pName)))
				{
					SearchID = i;
					break;
				}
			}
		}

		if(SearchID >= 0)
		{
			CUserRankData *aNameData = new CUserRankData(this);
			aNameData->m_ClientID = ClientID;
			str_copy(aNameData->m_aName, Server()->GetUserName(SearchID), sizeof(aNameData->m_aName));
			aNameData->m_UserID = Server()->GetUserID(SearchID);
			aNameData->m_MapID = Webapp()->CurrentMap()->m_ID;
			char aURI[128];
			str_format(aURI, sizeof(aURI), "/users/rank/%d/", aNameData->m_UserID);
			CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_GET, aURI);
			CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
			pInfo->SetCallback(OnUserRankGlobal, aNameData);
			Server()->SendHttp(pInfo, pRequest);
		}
		else if(Search)
		{
			json_value *pData = json_object_new(1);
			json_object_push(pData, "username", json_string_new(pName));
			char *pJson = new char[json_measure(pData)];
			json_serialize(pJson, pData);
			json_builder_free(pData);

			CUserRankData *aNameData = new CUserRankData(this);
			aNameData->m_ClientID = ClientID;
			str_copy(aNameData->m_aName, pName, sizeof(aNameData->m_aName));
			aNameData->m_MapID = Webapp()->CurrentMap()->m_ID;
			CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_POST, "/users/get_by_name/");
			pRequest->SetBody(pJson, str_length(pJson), "application/json");
			CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
			pInfo->SetCallback(OnUserFind, aNameData);
			Server()->SendHttp(pInfo, pRequest);
			delete[] pJson;
		}
	}
	else
		GameServer()->SendChatTarget(ClientID, "This map is not a teerace map.");
}

void CWebappScore::OnUserFind(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CUserRankData *pUser = (CUserRankData*)pUserData;
	CWebappScore *pScore = pUser->m_pScore;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CServerWebapp::CheckStatusCode(pScore->GameServer()->Console(), pResponse);

	pUser->m_UserID = 0;

	if(!Error)
	{
		json_settings JsonSettings;
		mem_zero(&JsonSettings, sizeof(JsonSettings));
		char aError[256];

		const char *pBody = ((CBufferResponse*)pResponse)->GetBody();
		json_value *pJsonData = json_parse_ex(&JsonSettings, pBody, pResponse->Size(), aError);
		if(pJsonData)
		{
			pUser->m_UserID = (*pJsonData)["id"].u.integer;
			if(pUser->m_UserID > 0)
				str_copy(pUser->m_aName, (*pJsonData)["username"], sizeof(pUser->m_aName));
		}
		else
			dbg_msg("json", aError);
		json_value_free(pJsonData);
	}

	if(pUser->m_UserID > 0)
	{
		char aURI[128];
		str_format(aURI, sizeof(aURI), "/users/rank/%d/", pUser->m_UserID);
		CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_GET, aURI);
		CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
		pInfo->SetCallback(OnUserRankGlobal, pUser);
		pScore->Server()->SendHttp(pInfo, pRequest);
		return; // do not delete userdata here
	}
	else if(pUser->m_PrintRank)
	{
		if(pScore->GameServer()->m_apPlayers[pUser->m_ClientID])
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "No match found for \"%s\".", pUser->m_aName);
			pScore->GameServer()->SendChatTarget(pUser->m_ClientID, aBuf);
		}
	}
	delete pUser;
}

void CWebappScore::OnUserRankGlobal(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CUserRankData *pUser = (CUserRankData*)pUserData;
	CWebappScore *pScore = pUser->m_pScore;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CServerWebapp::CheckStatusCode(pScore->GameServer()->Console(), pResponse);

	pUser->m_GlobalRank = 0;
	if(!Error)
		pUser->m_GlobalRank = str_toint(((CBufferResponse*)pResponse)->GetBody());

	char aURI[128];
	str_format(aURI, sizeof(aURI), "/users/map_rank/%d/%d/", pUser->m_UserID, pUser->m_MapID);
	CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_GET, aURI);
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	pInfo->SetCallback(OnUserRankMap, pUser); // do not delete userdata here
	pScore->Server()->SendHttp(pInfo, pRequest);
}

void CWebappScore::OnUserRankMap(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CUserRankData *pUser = (CUserRankData*)pUserData;
	CWebappScore *pScore = pUser->m_pScore;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CServerWebapp::CheckStatusCode(pScore->GameServer()->Console(), pResponse);

	CPlayer *pPl = pScore->GameServer()->m_apPlayers[pUser->m_ClientID];
	int GlobalRank = pUser->m_GlobalRank;
	int MapRank = 0;

	int Time = 0;
	int aCpTime[NUM_CHECKPOINTS] = { 0 };

	if(!pPl)
	{
		delete pUser;
		return;
	}

	if(!Error)
	{
		json_settings JsonSettings;
		mem_zero(&JsonSettings, sizeof(JsonSettings));
		char aError[256];

		const char *pBody = ((CBufferResponse*)pResponse)->GetBody();
		json_value *pJsonData = json_parse_ex(&JsonSettings, pBody, pResponse->Size(), aError);
		if(pJsonData)
		{
			MapRank = (*pJsonData)["position"].u.integer;
			const json_value &BestRun = (*pJsonData)["bestrun"];
			if(BestRun.type == json_object)
			{
				Time = IRace::TimeFromSecondsStr(BestRun["time"]);
				const json_value &CheckpointList = BestRun["checkpoints_list"];
				unsigned int CpNum = min(CheckpointList.u.array.length, (unsigned int)NUM_CHECKPOINTS);
				for(unsigned int i = 0; i < CpNum; i++)
					aCpTime[i] = IRace::TimeFromSecondsStr(CheckpointList[i]);
			}
		}
		else
			dbg_msg("json", aError);
		json_value_free(pJsonData);
	}

	bool Own = pUser->m_UserID == pScore->Server()->GetUserID(pUser->m_ClientID);
	if(Own && MapRank && Time > 0)
	{
		pScore->m_aPlayerData[pUser->m_ClientID].SetTime(Time, aCpTime);
		if(g_Config.m_SvShowBest)
		{
			pScore->m_aPlayerData[pUser->m_ClientID].UpdateCurTime(Time);
			int SendTo = g_Config.m_SvShowTimes ? -1 : pUser->m_ClientID;
			pScore->GameServer()->SendPlayerTime(SendTo, Time, pUser->m_ClientID);
		}
	}

	if(pUser->m_PrintRank)
	{
		char aBuf[256];
		bool Public = g_Config.m_SvShowTimes;
		if(!MapRank && !GlobalRank)
		{
			// do not send the rank to everyone if the player is not ranked at all
			Public = false;
			if(Own)
				str_copy(aBuf, "You are neither globally ranked nor on this map yet.", sizeof(aBuf));
			else
				str_format(aBuf, sizeof(aBuf), "%s is neither globally ranked nor on this map yet.", pUser->m_aName);
		}
		else if(!MapRank)
		{
			str_format(aBuf, sizeof(aBuf), "%s: Global Rank: %d | Map Rank: Not ranked yet (%s)",
				pUser->m_aName, GlobalRank, pScore->Server()->ClientName(pUser->m_ClientID));
		}
		else if(!GlobalRank)
		{
			char aTime[64];
			IRace::FormatTimeShort(aTime, sizeof(aTime), Time);
			str_format(aBuf, sizeof(aBuf), "%s: Not globally ranked yet | Map Rank: %d | Time: %s (%s)",
				pUser->m_aName, MapRank, aTime, pScore->Server()->ClientName(pUser->m_ClientID));
		}
		else
		{
			char aTime[64];
			IRace::FormatTimeShort(aTime, sizeof(aTime), Time);
			str_format(aBuf, sizeof(aBuf), "%s: Global Rank: %d | Map Rank: %d | Time: %s (%s)",
				pUser->m_aName, GlobalRank, MapRank, aTime, pScore->Server()->ClientName(pUser->m_ClientID));
		}

		if(Public)
			pScore->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		else
			pScore->GameServer()->SendChatTarget(pUser->m_ClientID, aBuf);
	}
	delete pUser;
}

void CWebappScore::OnUserTop(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CUserTopData *pUser = (CUserTopData*)pUserData;
	CWebappScore *pScore = pUser->m_pScore;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CServerWebapp::CheckStatusCode(pScore->GameServer()->Console(), pResponse);

	if(!Error)
	{
		json_settings JsonSettings;
		mem_zero(&JsonSettings, sizeof(JsonSettings));
		char aError[256];

		const char *pBody = ((CBufferResponse*)pResponse)->GetBody();
		json_value *pJsonData = json_parse_ex(&JsonSettings, pBody, pResponse->Size(), aError);
		if(pJsonData && pJsonData->u.array.length > 0)
		{
			int ClientID = pUser->m_ClientID;
			if(pUser->m_StartRank == 1)
			{
				const json_value &Run = (*pJsonData)[0];
				pScore->UpdateRecord(IRace::TimeFromSecondsStr(Run["run"]["time"]));
				pScore->m_GotRecord = true;
			}
			if(ClientID >= 0 && pScore->GameServer()->m_apPlayers[ClientID])
			{
				char aBuf[256];
				int LastTime = 0;
				int SameTimeCount = 0;
				pScore->GameServer()->SendChatTarget(ClientID, "----------- Top 5 -----------");
				for(unsigned int i = 0; i < pJsonData->u.array.length && i < 5; i++)
				{
					const json_value &Run = (*pJsonData)[i];
					int Time = IRace::TimeFromSecondsStr(Run["run"]["time"]);

					if(Time == LastTime)
						SameTimeCount++;
					else
						SameTimeCount = 0;

					char aTime[64];
					IRace::FormatTimeLong(aTime, sizeof(aTime), Time);
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s",
						i + pUser->m_StartRank - SameTimeCount, (const char*)Run["run"]["user"]["username"], aTime);
					pScore->GameServer()->SendChatTarget(ClientID, aBuf);

					LastTime = Time;
				}
				pScore->GameServer()->SendChatTarget(ClientID, "------------------------------");
			}
		}
		else
			dbg_msg("json", aError);
		json_value_free(pJsonData);
	}
	delete pUser;
}

void CWebappScore::OnRunPost(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CRunData *pUser = (CRunData*)pUserData;
	CWebappScore *pScore = pUser->m_pScore;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CServerWebapp::CheckStatusCode(pScore->GameServer()->Console(), pResponse);

	if(!Error && pUser->m_Tick > -1)
	{
		char aFilename[256];
		char aURL[128];

		pScore->Server()->Race_GetPath(aFilename, sizeof(aFilename), pUser->m_ClientID, false, pUser->m_Tick);
		str_format(aURL, sizeof(aURL), "/files/demo/%d/%d/", pUser->m_UserID, pUser->m_MapID);
		pScore->Webapp()->AddUpload(aFilename, aURL, "demo_file", time_get() + time_freq() * 2);

		pScore->Server()->Ghost_GetPath(aFilename, sizeof(aFilename), pUser->m_ClientID, false, pUser->m_Tick);
		str_format(aURL, sizeof(aURL), "/files/ghost/%d/%d/", pUser->m_UserID, pUser->m_MapID);
		pScore->Webapp()->AddUpload(aFilename, aURL, "ghost_file");
	}
	delete pUser;
}

#endif
