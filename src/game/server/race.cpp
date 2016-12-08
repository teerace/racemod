#include <base/math.h>
#include <base/color.h>

#include <engine/external/json-parser/json-builder.h>

#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/teerace.h>
#include <game/version.h>

#include "gamecontext.h"

#include "gamemodes/race.h"
#include "score.h"
#include "webapp.h"

void CGameContext::SendRecord(int ClientID)
{
	// no support for DDNet
	CNetMsg_Sv_Record Msg;
	Msg.m_Time = Score()->GetRecord()->m_Time;

	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && m_apPlayers[i]->m_RaceClient)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
		}
	}
	else
	{
		if(m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_RaceClient)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::SendPlayerTime(int ClientID, int Time, int ID)
{
	CNetMsg_Sv_PlayerTime Msg;
	Msg.m_Time = Time;
	Msg.m_ClientID = ID;

	CMsgPacker MsgDDNet(29 /* NETMSGTYPE_SV_PLAYERTIME */);
	MsgDDNet.AddInt(Time / 10);
	MsgDDNet.AddInt(ID);

	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && m_apPlayers[i]->m_RaceClient)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
			else if(m_apPlayers[i] && m_apPlayers[i]->m_DDNetClient)
				Server()->SendMsg(&MsgDDNet, MSGFLAG_VITAL, i);
		}
	}
	else
	{
		if(m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_RaceClient)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
		else if(m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_DDNetClient)
			Server()->SendMsg(&MsgDDNet, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::SendRaceTime(int ClientID, int Time, int CpDiff)
{
	if(m_apPlayers[ClientID]->m_RaceClient || m_apPlayers[ClientID]->m_DDNetClient)
	{
		CNetMsg_Sv_RaceTime Msg;
		Msg.m_Time = Time / 1000;
		Msg.m_Check = CpDiff / 10;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
	else if(CpDiff)
	{
		char aBuf[128];
		char aDiff[128];
		IRace::FormatTimeDiff(aDiff, sizeof(aDiff), CpDiff, false);
		str_format(aBuf, sizeof(aBuf), "Checkpoint | Diff : %s", aDiff);
		SendBroadcast(aBuf, ClientID);
	}
}

#if defined(CONF_TEERACE)
void CGameContext::OnTeeraceAuth(int ClientID, const char *pToken, int SendRconCmds)
{
	if(m_pWebapp && Server()->GetUserID(ClientID) <= 0)
		m_pWebapp->OnAuth(ClientID, pToken, SendRconCmds);
}
#endif

void CGameContext::ConKillPl(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if(!pSelf->m_apPlayers[CID])
		return;
	
	pSelf->m_apPlayers[CID]->KillCharacter(WEAPON_GAME);
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "%s killed by admin", pSelf->Server()->ClientName(CID));
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
}

void CGameContext::ConTeleport(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int CID2 = clamp(pResult->GetInteger(1), 0, (int)MAX_CLIENTS-1);
	if(pSelf->m_apPlayers[CID1] && pSelf->m_apPlayers[CID2])
	{
		CCharacter* pChr = pSelf->GetPlayerChar(CID1);
		if(pChr)
		{
			pChr->GetCore()->m_Pos = pSelf->m_apPlayers[CID2]->m_ViewPos;
			pSelf->RaceController()->SetRaceState(CID1, CGameControllerRACE::RACE_FINISHED);
		}
		else
			pSelf->m_apPlayers[CID1]->m_ViewPos = pSelf->m_apPlayers[CID2]->m_ViewPos;
	}
}

void CGameContext::ConTeleportTo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if(pSelf->m_apPlayers[CID])
	{
		CCharacter* pChr = pSelf->GetPlayerChar(CID);
		vec2 TelePos = vec2(pResult->GetInteger(1), pResult->GetInteger(2));
		if(pChr)
		{
			pChr->GetCore()->m_Pos = TelePos;
			pSelf->RaceController()->SetRaceState(CID, CGameControllerRACE::RACE_FINISHED);
		}
		else
			pSelf->m_apPlayers[CID]->m_ViewPos = TelePos;
	}
}

void CGameContext::ConGetPos(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if(pSelf->m_apPlayers[CID])
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s pos: %d @ %d", pSelf->Server()->ClientName(CID), (int)pSelf->m_apPlayers[CID]->m_ViewPos.x, (int)pSelf->m_apPlayers[CID]->m_ViewPos.y);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "race", aBuf);
	}
}

#if defined(CONF_TEERACE)
void CGameContext::ConPing(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_pWebapp)
		pSelf->m_pWebapp->m_LastPing = -1;
}

void CGameContext::ConMaplist(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->m_pWebapp)
		return;

	int MapListSize = pSelf->m_pWebapp->GetMapCount();
	int Page = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 0, (int)(MapListSize/21)) : 0;
	int Start = max(0, MapListSize - 20*(Page+1));
	for(int i = Start; i < Start+20; i++)
	{
		CServerWebapp::CMapInfo *pMapInfo = pSelf->m_pWebapp->GetMap(i);
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%d. %s by %s", i, pMapInfo->m_aName, pMapInfo->m_aAuthor);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "teerace", aBuf);
	}
}

void CGameContext::ConReloadMaplist(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pSelf->m_pWebapp)
		pSelf->m_pWebapp->LoadMapList();
}

void CGameContext::ConUpdateMapVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->m_pWebapp)
		return;

	char aBuf[256];

	// open config file
	IOHANDLE File = 0;
	if(!g_Config.m_WaAutoAddMaps) // only save to config if we dont add votes automatically
	{
		File = pSelf->Storage()->OpenFile(pSelf->Server()->GetConfigFilename(), IOFLAG_UPDATE, IStorage::TYPE_ALL);
		if(!File)
		{
			str_format(aBuf, sizeof(aBuf), "failed to save vote option to config file %s", pSelf->Server()->GetConfigFilename());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		}
	}

	bool First = true;
	int Argument = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 0, pSelf->m_pWebapp->GetMapCount()-1) : -1;
	for(int i = 0; i < pSelf->m_pWebapp->GetMapCount(); i++)
	{
		int ID = pResult->NumArguments() ? Argument : i;
		CServerWebapp::CMapInfo *pMapInfo = pSelf->m_pWebapp->GetMap(ID);

		char aVoteDescription[128];
		if(str_find(g_Config.m_WaVoteDescription, "%s"))
			str_format(aVoteDescription, sizeof(aVoteDescription), g_Config.m_WaVoteDescription, pMapInfo->m_aName);
		else
			str_format(aVoteDescription, sizeof(aVoteDescription), "change map to %s", pMapInfo->m_aName);

		// check for duplicate entry
		int OptionFound = false;
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while(pOption)
		{
			if(str_comp_nocase(aVoteDescription, pOption->m_aDescription) == 0)
			{
				str_format(aBuf, sizeof(aBuf), "option '%s' already exists", aVoteDescription);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				OptionFound = true;
				break;
			}
			pOption = pOption->m_pNext;
		}

		if(OptionFound)
			continue;

		// add the option
		++pSelf->m_NumVoteOptions;
		char aCommand[128];
		str_format(aCommand, sizeof(aCommand), "sv_map %s", pMapInfo->m_aName);
		int Len = str_length(aCommand);

		pOption = (CVoteOptionServer *)pSelf->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pOption->m_pNext = 0;
		pOption->m_pPrev = pSelf->m_pVoteOptionLast;
		if(pOption->m_pPrev)
			pOption->m_pPrev->m_pNext = pOption;
		pSelf->m_pVoteOptionLast = pOption;
		if(!pSelf->m_pVoteOptionFirst)
			pSelf->m_pVoteOptionFirst = pOption;

		str_copy(pOption->m_aDescription, aVoteDescription, sizeof(pOption->m_aDescription));
		mem_copy(pOption->m_aCommand, aCommand, Len+1);
		str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

		// inform clients about added option
		CNetMsg_Sv_VoteOptionAdd OptionMsg;
		OptionMsg.m_pDescription = pOption->m_aDescription;
		pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);

		if(File)
		{
			if(First)
			{
				io_seek(File, 0, IOSEEK_END); // seek to the end
				First = false;
			}
			str_format(aBuf, sizeof(aBuf), "\nadd_vote \"%s\" \"%s\"", aVoteDescription, aCommand);
			io_write(File, aBuf, str_length(aBuf));
		}
	}

	if(File)
		io_close(File);
}

void CGameContext::ConchainSpecialMapTypes(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	char aTypeList[128];
	str_copy(aTypeList, pResult->GetString(0), sizeof(aTypeList));

	// allways put the default for current gametype in the configuration
	char aCurGametype[32];
	if(str_find_nocase(g_Config.m_SvGametype, "cap"))
	{
		if(g_Config.m_SvNoItems)
			str_copy(aCurGametype, "fastcap-no-weapons", sizeof(aCurGametype));
		else
			str_copy(aCurGametype, "fastcap", sizeof(aCurGametype));
	}
	else
		str_copy(aCurGametype, "race", sizeof(aCurGametype));

	bool GameTypeFound = false;
	const char *pCurrentGametype = aCurGametype;
	int CurrentGametypeLen = str_length(aCurGametype);
	const char *pNextType = aTypeList;
	while(*pNextType)
	{
		int WordLen = 0;
		while(pNextType[WordLen] && pNextType[WordLen] != ' ')
			WordLen++;

		if(WordLen == CurrentGametypeLen && str_comp_num(pNextType, pCurrentGametype, CurrentGametypeLen) == 0)
		{
			pNextType += CurrentGametypeLen;
			while(*pNextType && *pNextType == ' ')
				pNextType++;

			GameTypeFound = true;
			break;
		}

		pNextType++;
	}

	if(!GameTypeFound)
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), " %s", aCurGametype);
		str_append(aTypeList, aBuf, sizeof(aTypeList));
		pResult->ChangeArgument(0, aTypeList);
	}

	pfnCallback(pResult, pCallbackUserData);
}
#endif

void CGameContext::ChatConInfo(IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Race mod %s (C)Rajh, Redix and Sushi", RACE_VERSION);
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", aBuf);
#if defined(CONF_TEERACE)
	str_format(aBuf, sizeof(aBuf), "Please visit 'http://%s/about/' for more information about teerace.", g_Config.m_WaWebappIp);
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", aBuf);
#endif
}

void CGameContext::ChatConTop5(IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;

	if(!g_Config.m_SvShowTimes)
	{
		pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "Showing the Top5 is not allowed on this server.");
		return;
	}

	if(pResult->NumArguments() > 0)
		pSelf->Score()->ShowTop5(pSelf->m_ChatConsoleClientID, max(1, pResult->GetInteger(0)));
	else
		pSelf->Score()->ShowTop5(pSelf->m_ChatConsoleClientID);
}

void CGameContext::ChatConRank(IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;

	if(g_Config.m_SvShowTimes && pResult->NumArguments() > 0)
	{
		char aStr[256];
		str_copy(aStr, pResult->GetString(0), sizeof(aStr));

		// strip trailing spaces
		int i = str_length(aStr);
		while(i >= 0)
		{
			if (aStr[i] < 0 || aStr[i] > 32)
				break;
			aStr[i] = 0;
			i--;
		}

		pSelf->Score()->ShowRank(pSelf->m_ChatConsoleClientID, aStr, true);
	}
	else
		pSelf->Score()->ShowRank(pSelf->m_ChatConsoleClientID, pSelf->Server()->ClientName(pSelf->m_ChatConsoleClientID));
}

void CGameContext::ChatConShowOthers(IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;

	if(!g_Config.m_SvShowOthers && !pSelf->Server()->IsAuthed(pSelf->m_ChatConsoleClientID))
	{
		pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "This command is not allowed on this server.");
		return;
	}

	if(pSelf->m_apPlayers[pSelf->m_ChatConsoleClientID]->m_RaceClient || pSelf->m_apPlayers[pSelf->m_ChatConsoleClientID]->m_DDNetClient)
		pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "Please use the settings to switch this option.");
	else
		pSelf->m_apPlayers[pSelf->m_ChatConsoleClientID]->m_ShowOthers = !pSelf->m_apPlayers[pSelf->m_ChatConsoleClientID]->m_ShowOthers;
}

void CGameContext::ChatConCmdlist(IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;

	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "---Command List---");
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "\"/info\" information about the mod");
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "\"/rank\" shows your rank");
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "\"/rank NAME\" shows the rank of a specific player");
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "\"/top5 X\" shows the top 5");
#if defined(CONF_TEERACE)
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "\"/mapinfo\" shows infos about the map");
#endif
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "\"/show_others\" show other players?");
}

#if defined(CONF_TEERACE)
void CGameContext::ChatConMapInfo(IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;

	if(!pSelf->m_pWebapp)
	{
		pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "This server does not use the webapp.");
		return;
	}

	if(pSelf->m_pWebapp->CurrentMap()->m_ID < 0)
	{
		pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "This map is not a teerace map.");
		return;
	}

	char aBuf[256];
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "----------- Mapinfo -----------");
	str_format(aBuf, sizeof(aBuf), "Name: %s", pSelf->Server()->GetMapName());
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", aBuf);
	str_format(aBuf, sizeof(aBuf), "Author: %s", pSelf->m_pWebapp->CurrentMap()->m_aAuthor);
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", aBuf);
	str_format(aBuf, sizeof(aBuf), "URL: http://%s%s", g_Config.m_WaWebappIp, pSelf->m_pWebapp->CurrentMap()->m_aURL);
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", aBuf);
	str_format(aBuf, sizeof(aBuf), "Finished runs: %d", pSelf->m_pWebapp->CurrentMap()->m_RunCount);
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", aBuf);
	pSelf->ChatConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "-------------------------------");
}
#endif

void CGameContext::InitChatConsole()
{
	m_pChatConsole = CreateConsole(CFGFLAG_SERVERCHAT);
	m_ChatConsoleClientID = -1;

	ChatConsole()->RegisterPrintCallback(IConsole::OUTPUT_LEVEL_STANDARD, SendChatResponse, this);
	ChatConsole()->Register("info", "", CFGFLAG_SERVERCHAT, ChatConInfo, this, "");
	ChatConsole()->Register("top5", "?i", CFGFLAG_SERVERCHAT, ChatConTop5, this, "");
	ChatConsole()->Register("rank", "?r", CFGFLAG_SERVERCHAT, ChatConRank, this, "");
	ChatConsole()->Register("show_others", "", CFGFLAG_SERVERCHAT, ChatConShowOthers, this, "");
	ChatConsole()->Register("cmdlist", "", CFGFLAG_SERVERCHAT, ChatConCmdlist, this, "");

#if defined(CONF_TEERACE)
	ChatConsole()->Register("mapinfo", "", CFGFLAG_SERVERCHAT, ChatConMapInfo, this, "");
#endif
}

void CGameContext::SendChatResponse(const char *pLine, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	if(pSelf->m_ChatConsoleClientID == -1)
		return;

	static volatile int ReentryGuard = 0;
	if(ReentryGuard)
		return;
	ReentryGuard++;

	while(*pLine && *pLine != ' ')
		pLine++;
	if(*pLine && *(pLine + 1))
		pSelf->SendChatTarget(pSelf->m_ChatConsoleClientID, pLine + 1);

	ReentryGuard--;
}

void CGameContext::LoadMapSettings()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();
	CMapItemInfo *pItem = (CMapItemInfo *)pMap->FindItem(MAPITEMTYPE_INFO, 0);
	if(pItem && pItem->m_Settings > -1)
	{
		// load settings
		if(pItem->m_Settings > -1)
		{
			int Size = pMap->GetUncompressedDataSize(pItem->m_Settings);
			const char *pTmp = (char*)pMap->GetData(pItem->m_Settings);
			const char *pEnd = pTmp + Size;
			while(pTmp < pEnd)
			{
				Console()->ExecuteLineFlag(pTmp, CFGFLAG_MAPSETTINGS);
				pTmp += str_length(pTmp) + 1;
			}
		}
	}
}

#if defined(CONF_TEERACE)
void CGameContext::UpdateTeeraceSkin(int ClientID)
{
	if(!m_pWebapp || Server()->GetUserID(ClientID) <= 0)
		return;

	CPlayer *pPlayer = m_apPlayers[ClientID];

	json_value *pData = json_object_new(1);
	json_object_push(pData, "skin_name", json_string_new(pPlayer->m_TeeInfos.m_SkinName));
	if(pPlayer->m_TeeInfos.m_UseCustomColor)
	{
		json_object_push(pData, "body_color", json_integer_new(HslToRgb(pPlayer->m_TeeInfos.m_ColorBody)));
		json_object_push(pData, "feet_color", json_integer_new(HslToRgb(pPlayer->m_TeeInfos.m_ColorFeet)));
	}
	char *pJson = new char[json_measure(pData)];
	json_serialize(pJson, pData);
	json_builder_free(pData);

	char aURI[128];
	str_format(aURI, sizeof(aURI), "/users/skin/%d/", Server()->GetUserID(ClientID));
	CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_PUT, aURI);
	pRequest->SetBody(pJson, str_length(pJson), "application/json");
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	//pInfo->SetCallback(CServerWebapp::OnUserUpdateSkin, this);
	m_pServer->SendHttp(pInfo, pRequest);
	delete[] pJson;
}

void CGameContext::UpdateTeeracePlaytime(int ClientID)
{
	int UserID = Server()->GetUserID(ClientID);
	if (!m_pWebapp || UserID <= 0)
		return;

	// calculate time in seconds
	int Seconds = Server()->GetPlayTicks(ClientID) / Server()->TickSpeed();
	json_value *pData = json_object_new(1);
	json_object_push(pData, "seconds", json_integer_new(Seconds));
	char *pJson = new char[json_measure(pData)];
	json_serialize(pJson, pData);
	json_builder_free(pData);

	char aURI[128];
	str_format(aURI, sizeof(aURI), "/users/playtime/%d/", UserID);
	CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_PUT, aURI);
	pRequest->SetBody(pJson, str_length(pJson), "application/json");
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	//pInfo->SetCallback(CServerWebapp::OnUserPlaytime, this);
	m_pServer->SendHttp(pInfo, pRequest);
	delete[] pJson;
}
#endif

int CmaskRace(CGameContext *pGameServer, int Owner)
{
	int Mask = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pGameServer->m_apPlayers[i] && (pGameServer->m_apPlayers[i]->m_ShowOthers || i == Owner))
			Mask = Mask | (1 << i);
	}
	return Mask;
}