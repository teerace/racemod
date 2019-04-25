#include <base/system.h>

#include <engine/shared/config.h>
#include <game/version.h>

#include "http.h"

#include <curl/curl.h>

const char *IHttp::GetFilename(const char *pFilepath)
{
	const char *pShort = pFilepath;
	for(const char *pCur = pShort; *pCur; pCur++)
	{
		if(*pCur == '/' || *pCur == '\\')
			pShort = pCur + 1;
	}
	return pShort;
}

void IHttp::EscapeUrl(char *pBuf, int Size, const char *pStr)
{
	char *pEsc = curl_easy_escape(0, pStr, 0);
	str_copy(pBuf, pEsc, Size);
	curl_free(pEsc);
}

CRequestInfo::CRequestInfo() :
	m_pRequest(0),
	m_pResponse(new CBufferResponse()),
	m_Priority(HTTP_PRIORITY_HIGH),
	m_pfnCallback(0),
	m_pUserData(0),
	m_pHandle(0),
	m_pHostResolve(0)
{
}

CRequestInfo::CRequestInfo(IOHANDLE File, const char *pFilename) :
	m_pRequest(0),
	m_pResponse(new CFileResponse(File, pFilename)),
	m_Priority(HTTP_PRIORITY_HIGH),
	m_pfnCallback(0),
	m_pUserData(0),
	m_pHandle(0),
	m_pHostResolve(0)
{
}

CRequestInfo::~CRequestInfo()
{
	if(m_pRequest)
		delete m_pRequest;
	if(m_pResponse)
		delete m_pResponse;
	if(m_pHandle)
		curl_easy_cleanup(m_pHandle);
	if(m_pHostResolve)
		curl_slist_free_all(m_pHostResolve);
}

void CRequestInfo::SetCallback(FHttpCallback pfnCallback, void *pUserData)
{
	m_pfnCallback = pfnCallback;
	m_pUserData = pUserData;
}

void CRequestInfo::ExecuteCallback(IResponse *pResponse, bool Error) const
{
	if(m_pfnCallback)
		m_pfnCallback(pResponse, Error, m_pUserData);
}

CHttpClient::CHttpClient()
{
	curl_version_info_data *pInfo = curl_version_info(CURLVERSION_NOW);
	m_CurlAsyncDNS = pInfo->features&CURL_VERSION_ASYNCHDNS;

	m_pMultiHandle = curl_multi_init();

	for(int i = 0; i < HTTP_MAX_ACTIVE_HANDLES; i++)
		m_apActiveHandles[i] = 0;
}

CHttpClient::~CHttpClient()
{
	curl_multi_cleanup(m_pMultiHandle);
}

void CHttpClient::Send(CRequestInfo *pInfo, IRequest *pRequest)
{
	if(g_Config.m_Debug)
		dbg_msg("http", "curl async dns: %s", m_CurlAsyncDNS ? "yes" : "no");

	pInfo->m_pRequest = pRequest;

	if(!m_CurlAsyncDNS)
	{
		const char *pHostName = pInfo->m_pRequest->m_aURL;
		const char *pStart = str_find(pHostName, "://");
		if(pStart)
			pHostName = pStart+3;

		char aHostName[128];
		str_copy(aHostName, pHostName, sizeof(aHostName));

		for(char *pStr = aHostName; *pStr; pStr++)
			if(*pStr == '/')
			{
				*pStr = 0;
				break;
			}

		m_pEngine->HostLookup(&pInfo->m_Lookup, aHostName, NETTYPE_IPV4);
	}

	m_lPendingRequests.add(pInfo);
}

void CHttpClient::StartHandle(CRequestInfo *pInfo)
{
	pInfo->m_pHandle = curl_easy_init();

	if(!m_CurlAsyncDNS)
	{
		const char *pHostName = pInfo->m_Lookup.m_aHostname;

		char aBuf[128];
		char aResolve[512];
		str_copy(aResolve, pHostName, sizeof(aResolve));

		int Port = pInfo->m_Lookup.m_Addr.port;
		if(Port == 0)
		{
			const char *pURL = pInfo->m_pRequest->m_aURL;
			Port = (str_comp_nocase_num(pURL, "https://", 8) == 0) ? 443 : 80;
			str_format(aBuf, sizeof(aBuf), ":%d", Port);
			str_append(aResolve, aBuf, sizeof(aResolve));
		}

		str_append(aResolve, ":", sizeof(aResolve));

		net_addr_str(&pInfo->m_Lookup.m_Addr, aBuf, sizeof(aBuf), 0);
		str_append(aResolve, aBuf, sizeof(aResolve));

		if(g_Config.m_Debug)
			dbg_msg("http", "resolve: %s", aResolve);

		pInfo->m_pHostResolve = curl_slist_append(NULL, aResolve);
		curl_easy_setopt(pInfo->m_pHandle, CURLOPT_RESOLVE, pInfo->m_pHostResolve);
	}

	curl_easy_setopt(pInfo->m_pHandle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
	curl_easy_setopt(pInfo->m_pHandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(pInfo->m_pHandle, CURLOPT_MAXREDIRS, 4L);
	curl_easy_setopt(pInfo->m_pHandle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(pInfo->m_pHandle, CURLOPT_USERAGENT, "Teeworlds " GAME_VERSION " (" CONF_PLATFORM_STRING ", " CONF_ARCH_STRING ")");
	curl_easy_setopt(pInfo->m_pHandle, CURLOPT_ACCEPT_ENCODING, "");

	if(g_Config.m_DbgCurl)
		curl_easy_setopt(pInfo->m_pHandle, CURLOPT_VERBOSE, 1L);

	pInfo->m_pRequest->InitHandle(pInfo->m_pHandle);
	pInfo->m_pResponse->InitHandle(pInfo->m_pHandle);

	curl_multi_add_handle(m_pMultiHandle, pInfo->m_pHandle);
}

void CHttpClient::FetchRequest(int Priority, int Max)
{
	int Num = 0;
	for(int j = 0; j < HTTP_MAX_ACTIVE_HANDLES; j++)
	{
		CRequestInfo *pInfo = m_apActiveHandles[j];
		if(pInfo && pInfo->m_Priority == Priority)
			Num++;
	}

	for(int k = 0; k < HTTP_MAX_ACTIVE_HANDLES && Num < Max; k++)
	{
		if(m_apActiveHandles[k])
			continue;

		for(int i = 0; i < m_lPendingRequests.size(); i++)
		{
			CRequestInfo *pInfo = m_lPendingRequests[i];
			if(pInfo->m_Priority == Priority && (m_CurlAsyncDNS ||
				(pInfo->m_Lookup.m_Job.Status() == CJob::STATE_DONE && pInfo->m_Lookup.m_Job.Result() == 0)))
			{
				m_lPendingRequests.remove_index(i--);
				m_apActiveHandles[k] = pInfo;

				StartHandle(pInfo);

				Num++;
				break;
			}
		}
	}
}

int CHttpClient::GetRequestInfo(CURL *pHandle) const
{
	for(int i = 0; i < HTTP_MAX_ACTIVE_HANDLES; i++)
	{
		if(m_apActiveHandles[i] && m_apActiveHandles[i]->m_pHandle == pHandle)
			return i;
	}

	return -1;
}

void CHttpClient::Update()
{
	for(int i = 0; i < m_lPendingRequests.size(); i++)
	{
		CRequestInfo *pInfo = m_lPendingRequests[i];
		if(pInfo->m_Lookup.m_Job.Status() == CJob::STATE_DONE && pInfo->m_Lookup.m_Job.Result() != 0)
		{
			pInfo->ExecuteCallback(0, true);
			delete pInfo;
			m_lPendingRequests.remove_index(i--);
		}
	}

	FetchRequest(HTTP_PRIORITY_HIGH, HTTP_MAX_ACTIVE_HANDLES);
	FetchRequest(HTTP_PRIORITY_LOW, HTTP_MAX_LOW_PRIORITY_HANDLES);

	int Active = 0;
	for(int i = 0; i < HTTP_MAX_ACTIVE_HANDLES; i++)
	{
		if(m_apActiveHandles[i])
			Active++;
	}
	
	int Running = 0;
	curl_multi_perform(m_pMultiHandle, &Running);

	if(Running < Active)
	{
		CURLMsg *pMsg;
		int MsgCount = 0;

		while((pMsg = curl_multi_info_read(m_pMultiHandle, &MsgCount)))
		{
			int Slot = GetRequestInfo(pMsg->easy_handle);
			if(Slot == -1)
			{
				curl_multi_remove_handle(m_pMultiHandle, pMsg->easy_handle);
				curl_easy_cleanup(pMsg->easy_handle);
			}

			CRequestInfo *pInfo = m_apActiveHandles[Slot];
			
			bool Success = pMsg->msg == CURLMSG_DONE && pMsg->data.result == CURLE_OK;
			curl_easy_getinfo(pInfo->m_pHandle, CURLINFO_RESPONSE_CODE, &pInfo->m_pResponse->m_StatusCode);

			pInfo->m_pRequest->Finalize();
			pInfo->m_pResponse->Finalize();

			if(!Success || g_Config.m_Debug)
				dbg_msg("http", "result: %s", curl_easy_strerror(pMsg->data.result));
			pInfo->ExecuteCallback(pInfo->m_pResponse, !Success);

			curl_multi_remove_handle(m_pMultiHandle, pInfo->m_pHandle);
			delete pInfo;
			m_apActiveHandles[Slot] = 0;
		}
	}
}

bool CHttpClient::HasActiveConnection() const
{
	if(m_lPendingRequests.size() > 0)
		return true;

	for(int i = 0; i < HTTP_MAX_ACTIVE_HANDLES; i++)
	{
		if(m_apActiveHandles[i])
			return true;
	}

	return false;
}
