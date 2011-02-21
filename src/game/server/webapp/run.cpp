#include <game/server/webapp.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "run.h"

int CWebRun::Post(void *pUserData)
{
	CData *pData = (CData*)pUserData;
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
	// TODO: not really nice
	float *pCpTime = pData->m_aCpTime;
	str_format(aBuf, sizeof(aBuf), "%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f",
		pCpTime[0], pCpTime[1], pCpTime[2], pCpTime[3], pCpTime[4], pCpTime[5], pCpTime[6], pCpTime[7], pCpTime[8], pCpTime[9],
		pCpTime[10], pCpTime[11], pCpTime[12], pCpTime[13], pCpTime[14], pCpTime[15], pCpTime[16], pCpTime[17], pCpTime[18], pCpTime[19],
		pCpTime[20], pCpTime[21], pCpTime[22], pCpTime[23], pCpTime[24], pCpTime[25]);
	Run["checkpoints"] = aBuf;
	
	std::string Json = Writer.write(Run);
	//std::cout << "---json start---\n" << Json << "\n---json end---\n" << std::endl;
	
	str_format(aBuf, sizeof(aBuf), "POST /api/1/runs/new/ HTTP/1.1\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", pWebapp->ApiKey(), Json.length(), Json.c_str());
	std::string Received = pWebapp->SendAndReceive(aBuf);
	pWebapp->Disconnect();
	
	// TODO check the status code
	delete pData;
	return 1;
}