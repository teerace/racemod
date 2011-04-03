#if defined(CONF_TEERACE)

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
	for(int i = 0; i < pData->m_lName.size(); i++)
	{
		int User = (pData->m_lUserID[i] > 0);
		if(User)
		{
			// TODO: send clan
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "%d", pData->m_lUserID[i]);
			Data["users"][aBuf] = pData->m_lName[i];
		}
		else
		{
			Data["anonymous"][Num] = pData->m_lName[i];
			Num++;
		}		
	}
	Data["map"] = pWebapp->MapName();
	
	std::string Json = Writer.write(Data);
	delete pData;
	
	char aBuf[1024];
	char *pReceived = 0;
	str_format(aBuf, sizeof(aBuf), CWebapp::POST, "/api/1/ping/", pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	int Size = pWebapp->SendAndReceive(aBuf, &pReceived);
	pWebapp->Disconnect();
	
	if(Size < 0)
	{
		dbg_msg("webapp", "error: %d (ping)", Size);
		return 0;
	}
	
	bool Online = str_comp(pReceived, "\"PONG\"") == 0;
	mem_free(pReceived);
	
	pWebapp->AddOutput(new COut(Online));
	return Online;
}

#endif
