#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "map.h"

int CWebMap::LoadList(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	delete pData;
	
	if(!pWebapp->Connect())
		return 0;
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "GET /api/1/maps/list/ HTTP/1.1\r\nHost: %s\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\n\r\n", pWebapp->ServerIP(), pWebapp->ApiKey());
	std::string Received = pWebapp->SendAndReceive(aBuf);
	pWebapp->Disconnect();
	
	Json::Value Maplist;
	Json::Reader Reader;
	bool ParsingSuccessful = Reader.parse(Received, Maplist);
	if(!ParsingSuccessful)
		return 0;
	
	COut *pOut = new COut();
	for(int i = 0; i < Maplist.size(); i++)
	{
		Json::Value Map = Maplist[i];
		pOut->m_MapList.add(Map["name"].asString());
	}
	pWebapp->AddOutput(pOut);
	return 1;
}