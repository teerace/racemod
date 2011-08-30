/* CClientWebapp Class by Sushi and Redix*/
#include <engine/shared/config.h>

#include <game/data.h>
#include <game/http_con.h>

#include "gameclient.h"
#include "webapp.h"

const char CClientWebapp::POST[] = "POST %s/%s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s";

CClientWebapp::CClientWebapp(CGameClient *pGameClient)
: IWebapp(g_Config.m_ClWebappIp, pGameClient->Storage()),
  m_pClient(pGameClient)
{
	m_ApiTokenError = false;
	m_ApiTokenRequested = false;
}

void CClientWebapp::OnResponse(CHttpConnection *pCon)
{
	int Type = pCon->m_Type;
	IStream *pData = pCon->m_pResponse;
	bool Error = pCon->State() == CHttpConnection::STATE_ERROR || pCon->StatusCode() != 200;

	// TODO: add event listener (server and client)
	if(Type == WEB_API_TOKEN)
	{
		// TODO: better error messages
		if(Error || str_comp(pData->GetData(), "false") == 0 || pData->Size() != 24+2)
			m_ApiTokenError = true;
		else
		{
			m_ApiTokenError = false;
			str_copy(g_Config.m_ClApiToken, pData->GetData()+1, 24+1);
		}
		m_ApiTokenRequested = false;
	}
}

const char* CClientWebapp::ServerIP()
{
	return g_Config.m_ClWebappIp;
}

const char* CClientWebapp::ApiPath()
{
	return g_Config.m_ClApiPath;
}
