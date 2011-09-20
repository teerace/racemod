#if defined(CONF_TEERACE)

#include <engine/external/json/writer.h>

#include "../gamecontext.h"
#include "../webapp.h"
#include "wa_score.h"

CWebappScore::CWebappScore(CGameContext *pGameServer) : m_pWebapp(pGameServer->Webapp()), m_pGameServer(pGameServer), m_pServer(pGameServer->Server()) {}

void CWebappScore::LoadScore(int ClientID, bool PrintRank)
{
	int UserID = Server()->GetUserID(ClientID);
	if(Webapp() && UserID > 0)
	{
		CWebUserRankData *pUserData = new CWebUserRankData();
		str_copy(pUserData->m_aName, Server()->GetUserName(ClientID), sizeof(pUserData->m_aName));
		pUserData->m_ClientID = ClientID;
		pUserData->m_UserID = UserID;
		pUserData->m_PrintRank = PrintRank;

		char aURI[128];
		str_format(aURI, sizeof(aURI), "users/rank/%d/", UserID);
		CRequest *pRequest = Webapp()->CreateRequest(aURI, CRequest::HTTP_GET);
		Webapp()->SendRequest(pRequest, WEB_USER_RANK_GLOBAL, pUserData);
	}
}

void CWebappScore::SaveScore(int ClientID, float Time, float *pCpTime, bool NewRecord)
{
	if(Webapp())
	{
		CWebRunData *pUserData = new CWebRunData();
		pUserData->m_UserID = Server()->GetUserID(ClientID);
		pUserData->m_ClientID = ClientID;
		pUserData->m_Tick = -1;

		if(NewRecord && Server()->GetUserID(ClientID) > 0)
		{
			// set demo and ghost so that it is saved
			Server()->SaveGhostDemo(ClientID);
			pUserData->m_Tick = Server()->Tick();
		}

		if(Webapp()->CurrentMap()->m_ID > -1)
		{
			Json::Value Run;
			Json::FastWriter Writer;

			char aBuf[1024];
			Run["map_id"] = Webapp()->CurrentMap()->m_ID;
			str_format(aBuf, sizeof(aBuf), "%08x", Webapp()->CurrentMap()->m_Crc);
			Run["map_crc"] = aBuf;
			Run["user_id"] = Server()->GetUserID(ClientID);
			str_copy(aBuf, Server()->ClientName(ClientID), MAX_NAME_LENGTH);
			str_sanitize_strong(aBuf);
			Run["nickname"] = aBuf;
			if(Server()->ClientClan(ClientID)[0])
			{
				str_copy(aBuf, Server()->ClientClan(ClientID), MAX_CLAN_LENGTH);
				str_sanitize_strong(aBuf);
				Run["clan"] = aBuf;
			}
			str_format(aBuf, sizeof(aBuf), "%.3f", Time);
			Run["time"] = aBuf;
			str_format(aBuf, sizeof(aBuf), "%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f",
				pCpTime[0], pCpTime[1], pCpTime[2], pCpTime[3], pCpTime[4], pCpTime[5], pCpTime[6], pCpTime[7], pCpTime[8], pCpTime[9],
				pCpTime[10], pCpTime[11], pCpTime[12], pCpTime[13], pCpTime[14], pCpTime[15], pCpTime[16], pCpTime[17], pCpTime[18], pCpTime[19],
				pCpTime[20], pCpTime[21], pCpTime[22], pCpTime[23], pCpTime[24], pCpTime[25]);
			Run["checkpoints"] = aBuf;

			std::string Json = Writer.write(Run);

			CRequest *pRequest = Webapp()->CreateRequest("runs/new/", CRequest::HTTP_POST);
			pRequest->SetBody(Json.c_str(), Json.length());
			Webapp()->SendRequest(pRequest, WEB_RUN_POST, pUserData);
		}
		
		// higher run count
		Webapp()->CurrentMap()->m_RunCount++;
	}
	
	// stop ghost record
	if(Server()->IsGhostRecording(ClientID))
		Server()->StopGhostRecord(ClientID, Time);
}

void CWebappScore::ShowTop5(int ClientID, int Debut)
{
	if(Webapp())
	{
		if(Webapp()->CurrentMap()->m_ID > -1)
		{
			CWebUserTopData *aNameData = new CWebUserTopData();
			aNameData->m_StartRank = Debut;
			aNameData->m_ClientID = ClientID;

			char aURI[128];
			str_format(aURI, sizeof(aURI), "maps/rank/%d/%d/", Webapp()->CurrentMap()->m_ID, Debut);
			CRequest *pRequest = Webapp()->CreateRequest(aURI, CRequest::HTTP_GET);
			Webapp()->SendRequest(pRequest, WEB_USER_TOP, aNameData);
		}
		else
			GameServer()->SendChatTarget(ClientID, "This map is not a teerace map.");
	}
}

void CWebappScore::ShowRank(int ClientID, const char *pName, bool Search)
{
	if(Webapp())
	{
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

			CWebUserRankData *aNameData = new CWebUserRankData();
			aNameData->m_ClientID = ClientID;

			if(SearchID >= 0)
			{
				str_copy(aNameData->m_aName, Server()->GetUserName(SearchID), sizeof(aNameData->m_aName));
				aNameData->m_UserID = Server()->GetUserID(SearchID);
				char aURI[128];
				str_format(aURI, sizeof(aURI), "users/rank/%d/", aNameData->m_UserID);
				CRequest *pRequest = Webapp()->CreateRequest(aURI, CRequest::HTTP_GET);
				Webapp()->SendRequest(pRequest, WEB_USER_RANK_GLOBAL, aNameData);
			}
			else if(Search)
			{
				str_copy(aNameData->m_aName, pName, sizeof(aNameData->m_aName));
				Json::Value Data;
				Json::FastWriter Writer;
				Data["username"] = pName;
				std::string Json = Writer.write(Data);
				CRequest *pRequest = Webapp()->CreateRequest("users/get_by_name/", CRequest::HTTP_POST);
				pRequest->SetBody(Json.c_str(), Json.length());
				Webapp()->SendRequest(pRequest, WEB_USER_FIND, aNameData);
			}
		}
		else
			GameServer()->SendChatTarget(ClientID, "This map is not a teerace map.");
	}
}

#endif
