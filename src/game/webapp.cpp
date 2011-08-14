/* Webapp class by Sushi and Redix */

#include <base/math.h>

#include "http_con.h"
#include "webapp.h"

// TODO: upload

IWebapp::IWebapp(const char* WebappIp)
{
	char aBuf[512];
	int Port = 80;
	str_copy(aBuf, WebappIp, sizeof(aBuf));

	for(int k = 0; aBuf[k]; k++)
	{
		if(aBuf[k] == ':')
		{
			Port = str_toint(aBuf+k+1);
			aBuf[k] = 0;
			break;
		}
	}

	if(net_host_lookup(aBuf, &m_Addr, NETTYPE_IPV4) != 0)
		net_host_lookup("localhost", &m_Addr, NETTYPE_IPV4);
	m_Addr.port = Port;

	m_Connections.delete_all();
}

bool IWebapp::SendRequest(const char *pData, int Type, IStream *pResponse, CWebData *pUserData)
{
	CHttpConnection *pCon = new CHttpConnection();
	if(!pCon->Create(m_Addr, Type, pResponse))
		return false;
	pCon->SetRequest(pData, str_length(pData));
	if(pUserData)
		pCon->m_pUserData = pUserData;
	m_Connections.add(pCon);
	return true;
}

int IWebapp::Update()
{
	int Num = 0, Max = 3;
	for(int i = 0; i < min(m_Connections.size(), Max); i++)
	{
		int Result = m_Connections[i]->Update();
		if(Result != 0)
		{
			int StatusCode = 200;
			if(Result == 1)
				dbg_msg("webapp", "received response (type: %d)", m_Connections[i]->m_Type);
			else
			{
				if(m_Connections[i]->m_pResponse->IsFile())
					((CFileStream*)m_Connections[i]->m_pResponse)->RemoveFile();
				if(Result == -1)
				{
					dbg_msg("webapp", "connection error (type: %d)", m_Connections[i]->m_Type);
					m_Connections[i]->m_pResponse->Clear();
					StatusCode = -1;
				}
				else
				{
					dbg_msg("webapp", "error (status code: %d, type: %d)", -Result, m_Connections[i]->m_Type);
					StatusCode = -Result;
				}
			}

			OnResponse(m_Connections[i]->m_Type, m_Connections[i]->m_pResponse, m_Connections[i]->m_pUserData, StatusCode);
			
			delete m_Connections[i];
			m_Connections.remove_index_fast(i);
			Num++;
		}
	}
	return Num;
}
