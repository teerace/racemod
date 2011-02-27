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
	str_format(aBuf, sizeof(aBuf), CWebapp::GET, "/api/1/maps/list/", pWebapp->ServerIP(), pWebapp->ApiKey());
	std::string Received = pWebapp->SendAndReceive(aBuf);
	pWebapp->Disconnect();
	
	Json::Value Maplist;
	Json::Reader Reader;
	bool ParsingSuccessful = Reader.parse(Received, Maplist);
	if(!ParsingSuccessful)
		return 0;
	
	COut *pOut = new COut(WEB_MAP_LIST);
	for(int i = 0; i < Maplist.size(); i++)
	{
		Json::Value Map = Maplist[i];
		pOut->m_MapList.add(Map["name"].asString());
	}
	pWebapp->AddOutput(pOut);
	return 1;
}

// TODO: optimize the downloading stuff (check for errors, ...)

int CWebMap::DownloadMaps(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	IStorage *pStorage = pData->m_pStorage;
	array<std::string> Maps = pData->m_DownloadList;
	delete pData;
	
	for(int i = 0; i < Maps.size(); i++)
	{
		if(!pWebapp->Connect())
			continue;
		
		char aBuf[512];
		char aURL[128];
		str_format(aURL, sizeof(aURL), "/api/1/maps/detail/%s/", Maps[i].c_str());
		str_format(aBuf, sizeof(aBuf), CWebapp::GET, aURL, pWebapp->ServerIP(), pWebapp->ApiKey());
		std::string Received = pWebapp->SendAndReceive(aBuf);
		
		// TODO: not really nice...
		pWebapp->Disconnect();
		if(!pWebapp->Connect())
			return 0;
		
		Json::Value Info;
		Json::Reader Reader;
		bool ParsingSuccessful = Reader.parse(Received, Info);
		if(!ParsingSuccessful)
			continue;
		
		const char *pURL = Info["get_download_url"].asCString();
		str_format(aBuf, sizeof(aBuf), CWebapp::DOWNLOAD, pURL, pWebapp->ServerIP());
		
		char aFilename[128];
		str_format(aFilename, sizeof(aFilename), "maps/teerace/%s.map", Maps[i].c_str());
		
		pWebapp->Send(aBuf, str_length(aBuf));
		
		std::cout << aBuf << std::endl;
		
		dbg_msg("wabapp", "downloading map: %s", Maps[i].c_str());
		int Size = 0;
		IOHANDLE File = 0;
		do
		{
			char aData[1024] = {0};
			Size = pWebapp->Recv(aData, sizeof(aData)-1);
			
			if(Size > 0)
			{
				char *pData = &aData[0];
				int Write = Size;
				if(!File)
				{
					// TODO: check the header
					while(pData && str_comp_num(pData, "\r\n\r\n", 4) != 0)
					{
						pData++;
						Write--;
					}
					if(str_comp_num(pData, "\r\n\r\n", 4) == 0)
					{
						pData += 4;
						Write -= 4;
					}
					dbg_msg("wabapp", "saving map to %s", aFilename);
					File = pStorage->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
					if(!File)
						break;
				}
				io_write(File, pData, Write);
			}
		} while(Size > 0);
		
		if(File)
		{
			dbg_msg("wabapp", "downloaded map: %s", Maps[i].c_str());
			COut *pOut = new COut(WEB_MAP_DOWNLOADED);
			pOut->m_MapList.add(Maps[i]);
			pWebapp->AddOutput(pOut);
			io_close(File);
		}
		else
		{
			dbg_msg("wabapp", "couldn't download map: %s", Maps[i].c_str());
		}
		
		pWebapp->Disconnect();
	}
	
	return 1;
}