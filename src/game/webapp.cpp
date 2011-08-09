/* CWebapp class by Sushi and Redix*/
#include <stdio.h>
#include <string.h>

#include <base/math.h>

#include "stream.h"
#include "webapp.h"

// TODO: non-blocking
// TODO: fix client

CWebapp::CWebapp(IStorage *pStorage, const char* WebappIp)
: m_pStorage(pStorage)
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
	{
		net_host_lookup("localhost", &m_Addr, NETTYPE_IPV4);
	}
	
	m_Addr.port = Port;
	
	// only one at a time
	m_JobPool.Init(1);
	m_Jobs.delete_all();
	
	m_OutputLock = lock_create();
	m_pFirst = 0;
	m_pLast = 0;
	
	m_Online = 0;
}

CWebapp::~CWebapp()
{
	// wait for the runnig jobs
	do
	{
		UpdateJobs();
	} while(m_Jobs.size() > 0);
	m_Jobs.delete_all();

	IDataOut *pNext;
	for(IDataOut *pItem = m_pFirst; pItem; pItem = pNext)
	{
		pNext = pItem->m_pNext;
		delete pItem;
	}

	lock_destroy(m_OutputLock);
}

void CWebapp::AddOutput(IDataOut *pOut)
{
	lock_wait(m_OutputLock);
	pOut->m_pNext = 0;
	if(m_pLast)
		m_pLast->m_pNext = pOut;
	else
		m_pFirst = pOut;
	m_pLast = pOut;
	lock_release(m_OutputLock);
}

bool CWebapp::Connect()
{
	// connect to the server
	m_Socket = net_tcp_create(m_Addr);
	if(m_Socket.type == NETTYPE_INVALID)
		return false;
	
	return true;
}

void CWebapp::Disconnect()
{
	net_tcp_close(m_Socket);
}

bool CWebapp::CHeader::Parse(char *pStr)
{
	char *pEnd = strstr(pStr, "\r\n\r\n");
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
		char *pLineEnd = strstr(pData, "\r\n");
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

bool CWebapp::SendRequest(const char *pInString, IStream *pResponse)
{
	net_tcp_connect(m_Socket, &m_Addr);
	net_tcp_send(m_Socket, pInString, str_length(pInString));

	CHeader Header;
	CBufferStream HeaderBuf;
	int Size;
	
	do
	{
		char aBuf[10] = {0};
		Size = net_tcp_recv(m_Socket, aBuf, sizeof(aBuf));
		if(Size <= 0 || !HeaderBuf.Write(aBuf, Size))
			return false;
	} while(!Header.Parse(HeaderBuf.GetData()));
	
	if(Header.m_Error)
		return false;
	
	if(!pResponse->Write(HeaderBuf.GetData()+Header.m_Size, HeaderBuf.Size()-Header.m_Size))
		return false;
	
	do
	{
		char aBuf[10] = {0};
		Size = net_tcp_recv(m_Socket, aBuf, sizeof(aBuf));
		if(!pResponse->Write(aBuf, Size))
			return false;
	} while(Size > 0);
	
	return (pResponse->Size() == Header.m_ContentLength);
}

CJob *CWebapp::AddJob(JOBFUNC pfnFunc, IDataIn *pUserData, bool NeedOnline)
{
	if(NeedOnline && !m_Online)
	{
		delete pUserData;
		return 0;
	}

	pUserData->m_pWebapp = this;
	int i = m_Jobs.add(new CJob());
	m_JobPool.Add(m_Jobs[i], pfnFunc, pUserData);
	return m_Jobs[i];
}

int CWebapp::UpdateJobs()
{
	int Num = 0;
	for(int i = 0; i < m_Jobs.size(); i++)
	{
		if(m_Jobs[i]->Status() == CJob::STATE_DONE)
		{
			delete m_Jobs[i];
			m_Jobs.remove_index_fast(i);
			Num++;
		}
	}
	return Num;
}
