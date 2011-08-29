/* Webapp class by Sushi and Redix */

#include <base/math.h>

#include "http_con.h"
#include "webapp.h"

IWebapp::IWebapp(const char* WebappIp, IStorage *pStorage) : m_pStorage(pStorage)
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
}

bool IWebapp::SendRequest(const char *pData, int Type, IStream *pResponse, CWebData *pUserData, IOHANDLE File, int64 StartTime)
{
	CHttpConnection *pCon = new CHttpConnection();
	if(!pCon->Create(m_Addr, Type, pResponse))
		return false;
	pCon->SetRequest(pData, str_length(pData), File);
	pCon->m_pUserData = pUserData;
	pCon->m_StartTime = StartTime;
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
				if(Result == -1)
				{
					StatusCode = -1;
					dbg_msg("webapp", "connection error (type: %d)", m_Connections[i]->m_Type);
				}
				else
				{
					StatusCode = -Result;
					dbg_msg("webapp", "error (status code: %d, type: %d)", StatusCode, m_Connections[i]->m_Type);
				}
			}

			if(m_Connections[i]->m_pResponse->IsFile())
				((CFileStream*)m_Connections[i]->m_pResponse)->Clear();

			OnResponse(m_Connections[i]->m_Type, m_Connections[i]->m_pResponse, m_Connections[i]->m_pUserData, StatusCode);
			
			delete m_Connections[i];
			m_Connections.remove_index_fast(i);
			Num++;
		}
	}
	return Num;
}
