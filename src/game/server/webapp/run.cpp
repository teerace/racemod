#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "run.h"

int CWebRun::Post(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	
	if(!pWebapp->Connect())
	{
		delete pData;
		return 0;
	}
	
	char aBuf[1024];
	Json::Value Run;
	Json::FastWriter Writer;
	
	Run["map_name"] = pWebapp->MapName();
	Run["user_id"] = pData->m_UserID;
	Run["nickname"] = pData->m_aName;
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
	mem_free(pReceived);
	
	if(Size < 0)
		dbg_msg("webapp", "error: %d (post run)", Size);
	
	return Size >= 0;
}