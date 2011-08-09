#if defined(CONF_TEERACE)

#include <game/stream.h>
#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "top.h"

int CWebTop::GetTop5(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CServerWebapp *pWebapp = (CServerWebapp*)pData->m_pWebapp;
	int Start = pData->m_Start;
	int ClientID = pData->m_ClientID;
	delete pData;
	
	if(!pWebapp->Connect())
		return 0;
	
	char aBuf[512];
	char aURL[128];
	str_format(aURL, sizeof(aURL), "maps/rank/%d/%d/", pWebapp->CurrentMap()->m_ID, Start);
	str_format(aBuf, sizeof(aBuf), CServerWebapp::GET, pWebapp->ApiPath(), aURL, pWebapp->ServerIP(), pWebapp->ApiKey());
	CBufferStream Buf;
	bool Check = pWebapp->SendRequest(aBuf, &Buf);
	pWebapp->Disconnect();
	
	if(!Check)
	{
		dbg_msg("webapp", "error (top5)");
		return 0;
	}
	
	Json::Value Top;
	Json::Reader Reader;
	if(!Reader.parse(Buf.GetData(), Buf.GetData()+Buf.Size(), Top))
		return 0;
	
	COut *pOut = new COut(WEB_USER_TOP);
	pOut->m_ClientID = ClientID;
	pOut->m_Start = Start;
	for(unsigned int i = 0; i < Top.size(); i++)
	{
		Json::Value Run = Top[i];
		CUserRank UserRank = CUserRank(Run["run"]["user"]["username"].asCString(),
								str_tofloat(Run["run"]["time"].asCString()));
		pOut->m_lUserRanks.add(UserRank);
	}
	
	pWebapp->AddOutput(pOut);
	return 1;
}

#endif
