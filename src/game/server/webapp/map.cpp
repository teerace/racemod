#if defined(CONF_TEERACE)

#include <game/stream.h>
#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>
#include <engine/storage.h>

#include "map.h"

int CWebMap::LoadList(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CServerWebapp *pWebapp = (CServerWebapp*)pData->m_pWebapp;
	bool CrcCheck = pData->m_CrcCheck;
	delete pData;
	
	if(!pWebapp->Connect())
		return 0;
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), CServerWebapp::GET, pWebapp->ApiPath(), "maps/list/", pWebapp->ServerIP(), pWebapp->ApiKey());
	CBufferStream Buf;
	bool Check = pWebapp->SendRequest(aBuf, &Buf);
	pWebapp->Disconnect();
	
	if(!Check)
	{
		dbg_msg("webapp", "error (map list)");
		return 0;
	}
	
	Json::Value Maplist;
	Json::Reader Reader;
	if(!Reader.parse(Buf.GetData(), Buf.GetData()+Buf.Size(), Maplist))
		return 0;
	
	COut *pOut = new COut(WEB_MAP_LIST);
	pOut->m_CrcCheck = CrcCheck;
	for(unsigned int i = 0; i < Maplist.size(); i++)
	{
		Json::Value Map = Maplist[i];
		pOut->m_lMapName.add(Map["name"].asString());
		pOut->m_lMapCrc.add(Map["crc"].asString());
		pOut->m_lMapURL.add(Map["get_download_url"].asString());
		pOut->m_lMapAuthor.add(Map["author"].asString());
		pOut->m_lMapRunCount.add(Map["run_count"].asInt());
		pOut->m_lMapID.add(Map["id"].asInt());
		
		// getting times
		CPlayerData MapRecord;
		if(!pWebapp->DefaultScoring() && Map["get_best_score"].type())
		{
			float Time = str_tofloat(Map["get_best_score"]["time"].asCString());
			float aCheckpointTimes[25] = {0.0f};
			Json::Value Checkpoint = Map["get_best_score"]["checkpoints_list"];
			for(unsigned int j = 0; j < Checkpoint.size(); j++)
				aCheckpointTimes[j] = str_tofloat(Checkpoint[j].asCString());
			MapRecord.Set(Time, aCheckpointTimes);
			
			pOut->m_lMapRecord.add(MapRecord);
		}
		else
			pOut->m_lMapRecord.add(MapRecord);
	}
	pWebapp->AddOutput(pOut);
	return 1;
}

int CWebMap::DownloadMaps(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CServerWebapp *pWebapp = (CServerWebapp*)pData->m_pWebapp;
	array<std::string> Maps = pData->m_lMapName;
	array<std::string> URL = pData->m_lMapURL;
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
			pOut->m_lMapName.add(pMap);
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

#endif
