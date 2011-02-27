#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "ping.h"

int CWebPing::Ping(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	
	if(!pWebapp->Connect())
	{
		delete pData;
		return 0;
	}
	
	Json::Value Data;
	Json::FastWriter Writer;
	
	int Num = 0;
	for(int i = 0; i < pData->m_Name.size(); i++)
	{
		int User = (pData->m_UserID[i] > 0);
		if(User)
		{
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "%d", pData->m_UserID[i]);
			Data["users"][aBuf] = pData->m_Name[i];
		}
		else
		{
			Data["anonymous"][Num] = pData->m_Name[i];
		}
		Num++;
		
	}
	Data["map"] = pWebapp->MapName();
	
	std::string Json = Writer.write(Data);
	delete pData;
	
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), CWebapp::POST, "/api/1/ping/", pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	std::string Received = pWebapp->SendAndReceive(aBuf);
	pWebapp->Disconnect();
	
	bool Online = !Received.compare("\"PONG\"");
	
	pWebapp->AddOutput(new COut(Online));
	return Online;
}