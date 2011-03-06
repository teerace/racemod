#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>
#include <engine/storage.h>

#include "map.h"

int CWebMap::LoadList(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	delete pData;
	
	if(!pWebapp->Connect())
		return 0;
	
	char aBuf[512];
	char *pReceived = 0;
	str_format(aBuf, sizeof(aBuf), CWebapp::GET, "/api/1/maps/list/", pWebapp->ServerIP(), pWebapp->ApiKey());
	int Size = pWebapp->SendAndReceive(aBuf, &pReceived);
	pWebapp->Disconnect();
	
	if(Size < 0)
	{
		dbg_msg("webapp", "error: %d (map list)", Size);
		return 0;
	}
	
	Json::Value Maplist;
	Json::Reader Reader;
	if(!Reader.parse(pReceived, pReceived+Size, Maplist))
	{
		mem_free(pReceived);
		return 0;
	}
	
	mem_free(pReceived);
	
	COut *pOut = new COut(WEB_MAP_LIST);
	for(int i = 0; i < Maplist.size(); i++)
	{
		Json::Value Map = Maplist[i];
		pOut->m_MapList.add(Map["name"].asString());
		pOut->m_MapURL.add(Map["get_download_url"].asString());
	}
	pWebapp->AddOutput(pOut);
	return 1;
}

int CWebMap::DownloadMaps(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	array<std::string> Maps = pData->m_MapList;
	array<std::string> URL = pData->m_MapURL;
	delete pData;
	
	for(int i = 0; i < Maps.size(); i++)
	{
		const char *pMap = Maps[i].c_str();
		const char *pURL = URL[i].c_str();
		
		if(!pWebapp->Connect())
			return 0;
		
		char aFilename[128];
		str_format(aFilename, sizeof(aFilename), "maps/teerace/%s.map", pMap);
		
		dbg_msg("webapp", "downloading map: %s", pMap);
		
		if(pWebapp->Download(aFilename, pURL))
		{
			dbg_msg("webapp", "downloaded map: %s", pMap);
			COut *pOut = new COut(WEB_MAP_DOWNLOADED);
			pOut->m_MapList.add(pMap);
			pWebapp->AddOutput(pOut);
		}
		else
		{
			dbg_msg("webapp", "couldn't download map: %s", pMap);
		}
		
		pWebapp->Disconnect();
	}
	
	return 1;
}