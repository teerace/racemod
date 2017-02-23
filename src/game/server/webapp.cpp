/* Webapp class by Sushi and Redix */
#if defined(CONF_TEERACE)

#include <stdio.h>

#include <base/tl/algorithm.h>
#include <engine/external/json-parser/json-builder.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/http.h>
#include <engine/storage.h>

#include <game/teerace.h>
#include <game/version.h>

#include "gamecontext.h"
#include "webapp.h"
#include "score.h"

void CMapList::Reset(const char *pMapTypes)
{
	m_Changed = false;
	m_LastReload = -1;
	str_copy(m_aMapTypes, pMapTypes, sizeof(m_aMapTypes));

	// remove map info
	for(int i = 0; i < m_lMaps.size(); i++)
	{
		CMapInfo *pMapInfo = &m_lMaps[i];
		if(pMapInfo->m_State == CMapInfo::MAPSTATE_COMPLETE)
			pMapInfo->m_State = CMapInfo::MAPSTATE_INFO_MISSING;
		else if(pMapInfo->m_State == CMapInfo::MAPSTATE_FILE_MISSING || pMapInfo->m_State == CMapInfo::MAPSTATE_DOWNLOADING)
		{
			m_lMaps.remove_index(i);
			i--;
		}
	}
}

bool CMapList::Update(CServerWebapp *pWebapp, const char *pData, int Size)
{
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];

	json_value *pJsonData = json_parse_ex(&JsonSettings, pData, Size, aError);
	if(!pJsonData)
	{
		dbg_msg("json", aError);
		json_value_free(pJsonData);
		return false;
	}

	char aFilename[256];
	const char *pPath = "/maps/teerace/%s.map";

	// mark all entries as missing
	for(int i = 0; i < m_lMaps.size(); i++)
		m_lMaps[i].m_ID = -1;

	for(unsigned int i = 0; i < pJsonData->u.array.length; i++)
	{
		const json_value &Map = (*pJsonData)[i];
		if(Map["crc"].type != json_string)
			continue; // skip maps without crc

		CMapInfo Info;
		str_copy(Info.m_aName, Map["name"], sizeof(Info.m_aName));
		sscanf(Map["crc"], "%08x", &Info.m_Crc);
		Info.m_ID = Map["id"].u.integer;
		Info.m_RunCount = Map["run_count"].u.integer;
		str_copy(Info.m_aURL, Map["get_download_url"], sizeof(Info.m_aURL));
		str_copy(Info.m_aAuthor, Map["author"], sizeof(Info.m_aAuthor));

		array<CMapInfo>::range r = find_linear(m_lMaps.all(), Info);
		if(r.empty()) // new entry
		{
			Info.m_State = CMapInfo::MAPSTATE_DOWNLOADING;
			m_lMaps.add(Info);
			if(g_Config.m_Debug)
				dbg_msg("webapp", "added map info: '%s' (%d)", Info.m_aName, Info.m_ID);

			str_format(aFilename, sizeof(aFilename), pPath, Info.m_aName);
			pWebapp->DownloadMap(aFilename, Map["get_download_url"]);
		}
		else
		{
			if(r.front().m_Crc != Info.m_Crc) // we have a wrong version
			{
				Info.m_State = CMapInfo::MAPSTATE_DOWNLOADING;
				str_format(aFilename, sizeof(aFilename), pPath, Info.m_aName);
				dbg_msg("webapp", "updating map file: '%s' (%08x)", Info.m_aName, Info.m_Crc);
				pWebapp->DownloadMap(aFilename, Map["get_download_url"]);
			}
			else // we already have this
			{
				if(r.front().m_State == CMapInfo::MAPSTATE_INFO_MISSING)
				{
					Info.m_State = CMapInfo::MAPSTATE_COMPLETE;
					if(g_Config.m_Debug)
						dbg_msg("webapp", "added map info: '%s' (%d)", Info.m_aName, Info.m_ID);
					m_Changed = true;
				}
				else if(r.front().m_State == CMapInfo::MAPSTATE_FILE_MISSING)
				{
					Info.m_State = CMapInfo::MAPSTATE_DOWNLOADING;
					str_format(aFilename, sizeof(aFilename), pPath, Info.m_aName);
					dbg_msg("webapp", "downloading missing map file: '%s' (%08x)", Info.m_aName, Info.m_Crc);
					pWebapp->DownloadMap(aFilename, Map["get_download_url"]);
				}
				else // keep state
					Info.m_State = r.front().m_State;
			}

			// update info
			r.front() = Info;
		}
	}

	// check for removed entries
	for(int i = 0; i < m_lMaps.size(); i++)
	{
		CMapInfo *pMapInfo = &m_lMaps[i];
		if(pMapInfo->m_ID == -1)
		{
			if(pMapInfo->m_State == CMapInfo::MAPSTATE_COMPLETE)
			{
				// info was removed from list; file exists
				pMapInfo->m_State = CMapInfo::MAPSTATE_INFO_MISSING;
				m_Changed = true;
			}
			else if(pMapInfo->m_State == CMapInfo::MAPSTATE_FILE_MISSING || pMapInfo->m_State == CMapInfo::MAPSTATE_DOWNLOADING)
			{
				// info was removed from list; file missing
				m_lMaps.remove_index(i);
				i--;
			}
			else
				continue;
			if(g_Config.m_Debug)
				dbg_msg("webapp", "removed map info: '%s'", pMapInfo->m_aName);
		}
	}

	json_value_free(pJsonData);
	return true;
}

const CMapInfo *CMapList::FindMap(const char *pName) const
{
	CMapInfo Info;
	str_copy(Info.m_aName, pName, sizeof(Info.m_aName));
	array<CMapInfo>::range r = find_linear(m_lMaps.all(), Info);
	if(r.empty() || r.front().m_State != CMapInfo::MAPSTATE_COMPLETE)
		return 0;
	return &r.front();
}

const CMapInfo *CMapList::AddMapFile(const char *pFilename, unsigned Crc)
{
	CMapInfo Info;
	Info.m_Crc = Crc;
	str_copy(Info.m_aName, pFilename, min((int)sizeof(Info.m_aName), str_length(pFilename) - 3));

	array<CMapInfo>::range r = find_linear(m_lMaps.all(), Info);
	if(r.empty()) // new entry
	{
		Info.m_State = CMapInfo::MAPSTATE_INFO_MISSING;
		int Num = m_lMaps.add(Info);
		dbg_msg("webapp", "added map file: '%s' (%08x)", Info.m_aName, Info.m_Crc);
		return &m_lMaps[Num];
	}
	else if (r.front().m_State == CMapInfo::MAPSTATE_DOWNLOADING) // entry already exists
	{
		if (r.front().m_Crc == Info.m_Crc)
		{
			r.front().m_State = CMapInfo::MAPSTATE_COMPLETE;
			dbg_msg("webapp", "added map file: '%s' (%08x)", Info.m_aName, Info.m_Crc);
			m_lMaps.sort_range();
			m_Changed = true;
			return &r.front();
		}
		// something went wrong
		r.front().m_State = CMapInfo::MAPSTATE_FILE_MISSING;
	}
	return 0;
}

IServer *CServerWebapp::Server() { return m_pGameServer->Server(); }
IScore *CServerWebapp::Score() { return m_pGameServer->Score(); }
IStorage *CServerWebapp::Storage() { return m_pGameServer->Storage(); }

CBufferRequest *CServerWebapp::CreateAuthedApiRequest(int Method, const char *pURI)
{
	CBufferRequest *pRequest = ITeerace::CreateApiRequest(Method, pURI);
	RegisterFields(pRequest);
	return pRequest;
}

void CServerWebapp::RegisterFields(IRequest *pRequest)
{
	pRequest->AddField("API-AUTH", g_Config.m_WaApiKey);
	pRequest->AddField("API-GAMESERVER-VERSION", TEERACE_GAMESERVER_VERSION);
}

void CServerWebapp::CheckStatusCode(IConsole *pConsole, class IResponse *pResponse)
{
	if(!pResponse)
		return;
	if(pResponse->StatusCode() == 432)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "webapp",
			"This server is outdated and cannot fully cooperate with Teerace, hence its support is currently disabled. Please notify the server administrator.");
	}
	else if(pResponse->StatusCode() == 403)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "webapp",
			"This server was denied access to Teerace network. Please notify the server administator.");
	}
}

void CServerWebapp::Download(const char *pFilename, const char *pURI, FHttpCallback pfnCallback)
{
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	CBufferRequest *pRequest = new CBufferRequest(IRequest::HTTP_GET, pURI);
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host(), File, pFilename);
	pInfo->SetCallback(pfnCallback, this);
	pInfo->SetPriority(HTTP_PRIORITY_LOW);
	Server()->SendHttp(pInfo, pRequest);
}

void CServerWebapp::Upload(const char *pFilename, const char *pURI, const char *pUploadName, FHttpCallback pfnCallback)
{
	CUploadData *pUserData = new CUploadData(this);
	str_copy(pUserData->m_aFilename, pFilename, sizeof(pUserData->m_aFilename));
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	CFileRequest *pRequest = ITeerace::CreateApiUpload(pURI);
	RegisterFields(pRequest);
	pRequest->SetFile(File, pFilename, pUploadName);
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	pInfo->SetCallback(pfnCallback, pUserData);
	pInfo->SetPriority(HTTP_PRIORITY_LOW);
	Server()->SendHttp(pInfo, pRequest);
}

CServerWebapp::CServerWebapp(CGameContext *pGameServer)
	: m_pGameServer(pGameServer), m_LastPing(-1), m_LastMapVoteUpdate(-1)
{
	// load maps
	Storage()->ListDirectory(IStorage::TYPE_SAVE, "maps/teerace", MaplistFetchCallback, this);
	m_lUploads.clear();
	m_pCurrentMapList = &m_aCachedMapLists[0];
}

void CServerWebapp::OnUserAuth(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CUserAuthData *pUser = (CUserAuthData*)pUserData;
	CServerWebapp *pWebapp = pUser->m_pWebapp;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWebapp->GameServer()->Console(), pResponse);

	int ClientID = pUser->m_ClientID;
	if(pWebapp->GameServer()->m_apPlayers[ClientID])
	{
		if(Error)
		{
			pWebapp->GameServer()->SendChatTarget(ClientID, "unknown error");
			delete pUser;
			return;
		}

		int SendRconCmds = pUser->m_SendRconCmds;
		int UserID = 0;

		json_settings JsonSettings;
		mem_zero(&JsonSettings, sizeof(JsonSettings));
		char aError[256];

		const char *pBody = ((CBufferResponse*)pResponse)->GetBody();
		json_value *pJsonData = json_parse_ex(&JsonSettings, pBody, pResponse->Size(), aError);
		if(!pJsonData)
			dbg_msg("json", aError);

		if(str_comp(pBody, "false") != 0 && pJsonData)
			UserID = (*pJsonData)["id"].u.integer;

		if(UserID > 0)
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "%s has logged in as %s", pWebapp->Server()->ClientName(ClientID), (const char*)(*pJsonData)["username"]);
			pWebapp->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			pWebapp->Server()->SetUserID(ClientID, UserID);
			pWebapp->Server()->SetUserName(ClientID, (*pJsonData)["username"]);

			// auth staff members
			if((bool)(*pJsonData)["is_staff"])
				pWebapp->Server()->StaffAuth(ClientID, SendRconCmds);

			pWebapp->Score()->OnPlayerInit(ClientID, true);
		}
		else
		{
			pWebapp->GameServer()->SendChatTarget(ClientID, "wrong username and/or password");
		}
		json_value_free(pJsonData);
	}
	delete pUser;
}

void CServerWebapp::OnPingPing(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CServerWebapp *pWebapp = (CServerWebapp*)pUserData;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWebapp->GameServer()->Console(), pResponse);

	if(!Error && g_Config.m_Debug)
		dbg_msg("webapp", "webapp is online");

	if(Error)
	{
		pWebapp->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
			"webapp", "WARNING: Webapp is not responding");
		return;
	}

	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];

	const char *pBody = ((CBufferResponse*)pResponse)->GetBody();
	json_value *pJsonData = json_parse_ex(&JsonSettings, pBody, pResponse->Size(), aError);
	if(pJsonData)
	{
		const json_value &Awards = (*pJsonData)["awards"];
		for(unsigned int i = 0; i < Awards.u.array.length; i++)
		{
			const json_value &Award = Awards[i];
			int UserID = Award["user_id"].u.integer;

			if(!UserID)
				continue;

			// show awards to everyone only if the player is there
			for(int j = 0; j < MAX_CLIENTS; j++)
			{
				if(UserID != pWebapp->Server()->GetUserID(j))
					continue;

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s achieved award \"%s\".", pWebapp->Server()->ClientName(j), (const char*)Award["name"]);
				pWebapp->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			}
		}
	}
	else
		dbg_msg("json", aError);
	json_value_free(pJsonData);
}

void CServerWebapp::OnMapList(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CMapListData *pUser = (CMapListData*)pUserData;
	CServerWebapp *pWebapp = pUser->m_pWebapp;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWebapp->GameServer()->Console(), pResponse);

	if(Error)
	{
		delete pUser;
		return;
	}

	for(int i = 0; i < NUM_CACHED_MAPLISTS; i++)
		if(str_comp(pWebapp->m_aCachedMapLists[i].GetMapTypes(), pUser->m_aMapTypes) == 0)
		{
			const char *pBody = ((CBufferResponse*)pResponse)->GetBody();
			bool Res = pWebapp->m_aCachedMapLists[i].Update(pWebapp, pBody, pResponse->Size());
			if(Res && pWebapp->m_CurrentMap.m_ID == -1)
				pWebapp->OnInitMap();
			break;
		}

	delete pUser;
}

void CServerWebapp::OnDownloadMap(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CFileResponse *pRes = (CFileResponse*)pResponse;
	CServerWebapp *pWebapp = (CServerWebapp*)pUserData;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWebapp->GameServer()->Console(), pResponse);

	if(!Error)
	{
		bool Added = false;
		char aName[128];
		for(int i = 0; i < NUM_CACHED_MAPLISTS; i++)
		{
			const CMapInfo *pInfo = pWebapp->m_aCachedMapLists[i].AddMapFile(pRes->GetFilename(), pRes->GetCrc());
			if(pInfo)
			{
				Added = true;
				str_copy(aName, pInfo->m_aName, sizeof(aName));
			}
		}
		if(Added && str_comp(aName, g_Config.m_SvMap) == 0)
			pWebapp->Server()->ReloadMap();
		if(!Added)
			pWebapp->Storage()->RemoveFile(pRes->GetPath(), IStorage::TYPE_SAVE);
	}
	else
	{
		pWebapp->GameServer()->Storage()->RemoveFile(pRes->GetPath(), IStorage::TYPE_SAVE);
		dbg_msg("webapp", "could not download map: '%s'", pRes->GetPath());
	}
}

void CServerWebapp::OnUploadFile(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CUploadData *pUser = (CUploadData*)pUserData;
	CServerWebapp *pWebapp = pUser->m_pWebapp;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWebapp->GameServer()->Console(), pResponse);

	if(!Error)
		dbg_msg("webapp", "uploaded file: '%s'", pUser->m_aFilename);
	else
		dbg_msg("webapp", "could not upload file: '%s'", pUser->m_aFilename);
	pWebapp->Storage()->RemoveFile(pUser->m_aFilename, IStorage::TYPE_SAVE);
	delete pUser;
}

int CServerWebapp::MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CServerWebapp *pWebapp = (CServerWebapp*)pUser;
	int Length = str_length(pName);
	if(!IsDir && Length >= 4 && str_comp(pName + Length - 4, ".map") == 0)
	{
		char aFile[256];
		str_format(aFile, sizeof(aFile), "maps/teerace/%s", pName);
		unsigned MapCrc = 0;
		unsigned MapSize = 0;
		if(!CDataFileReader::GetCrcSize(pWebapp->Storage(), aFile, IStorage::TYPE_SAVE, &MapCrc, &MapSize))
			return 0;
		for(int i = 0; i < NUM_CACHED_MAPLISTS; i++)
			pWebapp->m_aCachedMapLists[i].AddMapFile(pName, MapCrc);
	}
	return 0;
}

void CServerWebapp::OnInitMap()
{
	const CMapInfo *pMap = m_pCurrentMapList->FindMap(g_Config.m_SvMap);
	if(pMap)
	{
		m_CurrentMap = *pMap;
		Score()->ShowTop5(-1);
		dbg_msg("webapp", "current map: '%s' (%d)", m_CurrentMap.m_aName, m_CurrentMap.m_ID);
	}
}

void CServerWebapp::OnAuth(int ClientID, const char *pToken, int SendRconCmds)
{
	json_value *pData = json_object_new(1);
	json_object_push(pData, "api_token", json_string_new(pToken));
	char *pJson = new char[json_measure(pData)];
	json_serialize(pJson, pData);
	json_builder_free(pData);

	CUserAuthData *pUserData = new CUserAuthData(this);
	pUserData->m_ClientID = ClientID;
	pUserData->m_SendRconCmds = SendRconCmds;

	CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_POST, "/users/auth_token/");
	pRequest->SetBody(pJson, str_length(pJson), "application/json");
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	pInfo->SetCallback(CServerWebapp::OnUserAuth, pUserData);
	Server()->SendHttp(pInfo, pRequest);
	delete[] pJson;
}

void CServerWebapp::Tick()
{
	int64 Now = time_get();

	// do uploads
	for(int i = 0; i < m_lUploads.size(); i++)
	{
		if(m_lUploads[i].m_StartTime <= Now)
		{
			Upload(m_lUploads[i].m_aFilename, m_lUploads[i].m_aURL, m_lUploads[i].m_aUploadname, m_lUploads[i].m_pfnCallback);
			m_lUploads.remove_index_fast(i);
			i--; // since one item was removed
		}
	}

	// reload maplist regularly
	if(m_pCurrentMapList->m_LastReload < 0 || m_pCurrentMapList->m_LastReload + time_freq() * 60 * g_Config.m_WaMaplistRefreshInterval < Now)
	{
		UpdateMapList();
		LoadMapList();
	}

	// ping every minute
	if(g_Config.m_SvRegister && (m_LastPing < 0 || m_LastPing + time_freq() * 60 < Now))
		SendPing();

	// only one vote update every 3 seconds
	// TODO: check resend buffer size
	if(g_Config.m_WaAutoAddMaps && m_pCurrentMapList->m_Changed && (m_LastMapVoteUpdate < 0 || m_LastMapVoteUpdate + time_freq() * 3 < Now))
		UpdateMapVotes();
}

bool CServerWebapp::UpdateMapList()
{
	if(str_comp(m_pCurrentMapList->GetMapTypes(), g_Config.m_WaMapTypes) != 0)
	{
		for(int i = 0; i < NUM_CACHED_MAPLISTS; i++)
			if(str_comp(m_aCachedMapLists[i].GetMapTypes(), g_Config.m_WaMapTypes) == 0)
			{
				dbg_msg("webapp", "switching map list");
				m_pCurrentMapList = &m_aCachedMapLists[i];
				m_pCurrentMapList->m_Changed = true;
				return false;
			}

		// empty
		for(int i = 0; i < NUM_CACHED_MAPLISTS; i++)
			if(m_aCachedMapLists[i].GetMapTypes()[0] == 0)
			{
				dbg_msg("webapp", "loading new map list");
				m_pCurrentMapList = &m_aCachedMapLists[i];
				m_pCurrentMapList->Reset(g_Config.m_WaMapTypes);
				return true;
			}

		CMapList *pOldest = &m_aCachedMapLists[0];
		for(int i = 1; i < NUM_CACHED_MAPLISTS; i++)
		{
			if(m_aCachedMapLists[i].m_LastReload < pOldest->m_LastReload)
				pOldest = &m_aCachedMapLists[i];
		}

		dbg_msg("webapp", "loading new map list");
		m_pCurrentMapList = pOldest;
		m_pCurrentMapList->Reset(g_Config.m_WaMapTypes);
		return true;
	}
	return false;
}

void CServerWebapp::LoadMapList()
{
	dbg_msg("webapp", "reloading map list");

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "/maps/list/%s/", m_pCurrentMapList->GetMapTypes());

	CMapListData *pUserData = new CMapListData(this);
	str_copy(pUserData->m_aMapTypes, m_pCurrentMapList->GetMapTypes(), sizeof(pUserData->m_aMapTypes));

	CBufferRequest *pRequest = CreateAuthedApiRequest(IRequest::HTTP_GET, aBuf);
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	pInfo->SetCallback(OnMapList, pUserData);
	Server()->SendHttp(pInfo, pRequest);

	m_pCurrentMapList->m_LastReload = time_get();
}

void CServerWebapp::SendPing()
{
	json_value *pData = json_object_new(3);
	json_value *pUsers = json_object_new(0);
	json_value *pAnonymous = json_array_new(0);
	int Num = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			if(Server()->GetUserID(i) > 0)
			{
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d", Server()->GetUserID(i));
				char aName[MAX_NAME_LENGTH];
				str_copy(aName, Server()->ClientName(i), sizeof(aName));
				str_sanitize_strong(aName);
				json_object_push(pUsers, aBuf, json_string_new(aName));
			}
			else
			{
				char aName[MAX_NAME_LENGTH];
				str_copy(aName, Server()->ClientName(i), sizeof(aName));
				str_sanitize_strong(aName);
				json_array_push(pAnonymous, json_string_new(aName));
				Num++;
			}
		}
	}
	json_object_push(pData, "users", pUsers);
	json_object_push(pData, "anonymous", pAnonymous);
	json_object_push(pData, "map", json_string_new(g_Config.m_SvMap));
	char *pJson = new char[json_measure(pData)];
	json_serialize(pJson, pData);
	json_builder_free(pData);

	CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_POST, "/ping/");
	pRequest->SetBody(pJson, str_length(pJson), "application/json");
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	pInfo->SetCallback(CServerWebapp::OnPingPing, this);
	Server()->SendHttp(pInfo, pRequest);
	delete[] pJson;

	m_LastPing = time_get();
}

void CServerWebapp::AddUpload(const char *pFilename, const char *pURL, const char *pUploadName, FHttpCallback pfnCallback, int64 StartTime)
{
	m_lUploads.add(CUpload(pFilename, pURL, pUploadName, pfnCallback, StartTime));
}

void CServerWebapp::UpdateMapVotes()
{
	dbg_msg("webapp", "updating map votes");

	// clear votes
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	GameServer()->m_pVoteOptionHeap->Reset();
	GameServer()->m_pVoteOptionFirst = 0;
	GameServer()->m_pVoteOptionLast = 0;
	GameServer()->m_NumVoteOptions = 0;

	if(g_Config.m_WaVoteHeaderFile[0])
		GameServer()->Console()->ExecuteFile(g_Config.m_WaVoteHeaderFile);

	for(int i = 0; i < m_pCurrentMapList->GetMapCount(); i++)
	{
		const CMapInfo *pMapInfo = m_pCurrentMapList->GetMap(i);
		if(pMapInfo->m_State == CMapInfo::MAPSTATE_COMPLETE)
		{
			char aVoteDescription[128];
			if(str_find(g_Config.m_WaVoteDescription, "%s"))
				str_format(aVoteDescription, sizeof(aVoteDescription), g_Config.m_WaVoteDescription, pMapInfo->m_aName);
			else
				str_format(aVoteDescription, sizeof(aVoteDescription), "change map to %s", pMapInfo->m_aName);

			char aCommand[128];
			str_format(aCommand, sizeof(aCommand), "sv_map %s", pMapInfo->m_aName);
			int Len = str_length(aCommand);

			// add the option
			++GameServer()->m_NumVoteOptions;
			CVoteOptionServer *pOption = (CVoteOptionServer *)GameServer()->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
			pOption->m_pNext = 0;
			pOption->m_pPrev = GameServer()->m_pVoteOptionLast;
			if(pOption->m_pPrev)
				pOption->m_pPrev->m_pNext = pOption;
			GameServer()->m_pVoteOptionLast = pOption;
			if(!GameServer()->m_pVoteOptionFirst)
				GameServer()->m_pVoteOptionFirst = pOption;

			str_copy(pOption->m_aDescription, aVoteDescription, sizeof(pOption->m_aDescription));
			mem_copy(pOption->m_aCommand, aCommand, Len+1);

			// inform clients about added option
			CNetMsg_Sv_VoteOptionAdd OptionMsg;
			OptionMsg.m_pDescription = pOption->m_aDescription;
			Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
		}
	}

	m_pCurrentMapList->m_Changed = false;
	m_LastMapVoteUpdate = time_get();
}

CServerWebapp::CUpload::CUpload(const char* pFilename, const char* pURL, const char* pUploadname, FHttpCallback pfnCallback, int64 StartTime)
	: m_pfnCallback(pfnCallback), m_StartTime(StartTime)
{
	str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
	str_copy(m_aURL, pURL, sizeof(m_aURL));
	str_copy(m_aUploadname, pUploadname, sizeof(m_aUploadname));
}

#endif
