/* Webapp class by Sushi and Redix */

#include <base\math.h>

#include <stdio.h>
#include "webapp.h"

// TODO: upload
// TODO: rank

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

bool IWebapp::SendRequest(const char *pData, int Type, IStream *pResponse, void *pUserData)
{
	CHttpConnection *pCon = new CHttpConnection();
	if(!pCon->Create(m_Addr, Type, pResponse))
		return false;
	if(!pCon->Send(pData, str_length(pData)))
	{
		pCon->Close();
		return false;
	}
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

// TODO: own file
// TODO: support for chunked transfer-encoding?
// TODO: error handling
bool CHttpConnection::CHeader::Parse(char *pStr)
{
	char *pEnd = (char*)str_find(pStr, "\r\n\r\n");
	if(!pEnd)
		return false;
	
	*(pEnd+2) = 0;
	char *pData = pStr;
	
	if(sscanf(pData, "HTTP/%*d.%*d %d %*s\r\n", &this->m_StatusCode) != 1)
	{
		m_Error = true;
		return false;
	}
	
	while(sscanf(pData, "Content-Length: %ld\r\n", &this->m_ContentLength) != 1)
	{
		char *pLineEnd = (char*)str_find(pData, "\r\n");
		if(!pLineEnd)
		{
			m_Error = true;
			return false;
		}
		pData = pLineEnd + 2;
	}
	
	m_Size = (pEnd-pStr)+4;
	return true;
}

CHttpConnection::~CHttpConnection()
{
	if(m_pResponse)
		delete m_pResponse;
	if(m_pUserData)
		mem_free(m_pUserData);
	Close();
}

bool CHttpConnection::Create(NETADDR Addr, int Type, IStream *pResponse)
{
	m_Socket = net_tcp_create(Addr);
	if(m_Socket.type == NETTYPE_INVALID)
		return false;
	if(net_tcp_connect(m_Socket, &Addr) != 0) // TODO: non-blocking
	{
		Close();
		return false;
	}
	m_Connected = true;
	
	net_set_non_blocking(m_Socket);
	
	m_Type = Type;
	m_pResponse = pResponse;
	return true;
}

void CHttpConnection::Close()
{
	net_tcp_close(m_Socket);
	m_Connected = false;
}

bool CHttpConnection::Send(const char *pData, int Size)
{
	while(m_Connected)
	{
		int Send = net_tcp_send(m_Socket, pData, Size);
		if(Send < 0)
			return false;

		if(Send >= Size)
			return true;

		pData += Send;
		Size -= Send;
	}
	return false;
}

int CHttpConnection::Update()
{
	if(!m_Connected)
		return -1;
	
	char aBuf[1024] = {0};
	int Bytes = net_tcp_recv(m_Socket, aBuf, sizeof(aBuf));

	if(Bytes > 0)
	{
		if(m_Header.m_Size == -1)
		{
			m_HeaderBuffer.Write(aBuf, Bytes);
			if(m_Header.Parse(m_HeaderBuffer.GetData()))
			{
				if(m_Header.m_Error)
					return -1;
				else
					m_pResponse->Write(m_HeaderBuffer.GetData()+m_Header.m_Size, m_HeaderBuffer.Size()-m_Header.m_Size);
			}
		}
		else
		{
			if(!m_pResponse->Write(aBuf, Bytes))
				return -1;
		}
	}
	else if(Bytes < 0)
	{
		if(net_would_block()) // no data received
			return 0;
		
		return -1;
	}
	else
	{
		if(m_Header.m_StatusCode != 0 && m_Header.m_StatusCode != 200)
			return -m_Header.m_StatusCode;
		return m_pResponse->Size() == m_Header.m_ContentLength ? 1 : -1;
	}
	
	return 0;
}
