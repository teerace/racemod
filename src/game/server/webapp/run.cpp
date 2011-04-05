#if defined(CONF_TEERACE)

#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "run.h"

int CWebRun::Post(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	int ClientID = pData->m_ClientID;
	int Tick = pData->m_Tick;
	
	if(!pWebapp->Connect())
	{
		delete pData;
		return 0;
	}
	
	char aBuf[1024];
	Json::Value Run;
	Json::FastWriter Writer;
	
	Run["map_id"] = pWebapp->CurrentMap()->m_ID;
	Run["user_id"] = pData->m_UserID;
	// TODO: take this out after 0.6 release
	str_sanitize_strong(pData->m_aName);
	Run["nickname"] = pData->m_aName;
	if(pData->m_aClan[0])
		Run["clan"] = pData->m_aClan;
	str_format(aBuf, sizeof(aBuf), "%.3f", pData->m_Time);
	Run["time"] = aBuf;
	float *pCpTime = pData->m_aCpTime;
	str_format(aBuf, sizeof(aBuf), "%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f",
		pCpTime[0], pCpTime[1], pCpTime[2], pCpTime[3], pCpTime[4], pCpTime[5], pCpTime[6], pCpTime[7], pCpTime[8], pCpTime[9],
		pCpTime[10], pCpTime[11], pCpTime[12], pCpTime[13], pCpTime[14], pCpTime[15], pCpTime[16], pCpTime[17], pCpTime[18], pCpTime[19],
		pCpTime[20], pCpTime[21], pCpTime[22], pCpTime[23], pCpTime[24], pCpTime[25]);
	Run["checkpoints"] = aBuf;
	
	std::string Json = Writer.write(Run);
	delete pData;
	
	char *pReceived = 0;
	str_format(aBuf, sizeof(aBuf), CWebapp::POST, "/api/1/runs/new/", pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	int Size = pWebapp->SendAndReceive(aBuf, &pReceived);
	pWebapp->Disconnect();
	
	if(Size < 0)
	{
		dbg_msg("webapp", "error: %d (run)", Size);
		return 0;
	}
	
	Json::Reader Reader;
	if(!Reader.parse(pReceived, pReceived+Size, Run))
	{
		mem_free(pReceived);
		return 0;
	}
	
	mem_free(pReceived);
	
	COut *pOut = new COut(WEB_RUN);
	pOut->m_RunID = Run["id"].asInt();
	pOut->m_Tick = Tick;
	pOut->m_ClientID = ClientID;
	pWebapp->AddOutput(pOut);
	
	return Size >= 0;
}

#endif
