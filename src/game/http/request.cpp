#include <base/math.h>
#include <string.h>

#include "request.h"

// const char CServerWebapp::UPLOAD[] = "POST %s/%s HTTP/1.1\r\nHost: %s\r\nAPI-AUTH: %s\r\nContent-Type: multipart/form-data; boundary=frontier\r\nContent-Length: %d\r\n\r\n--frontier\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"file.file\"\r\nContent-Type: application/octet-stream\r\n\r\n";

CRequest::CRequest(const char *pHost, const char *pURI, int Method) : m_Method(Method),
	m_State(STATE_NONE), m_pBody(0), m_BodySize(0), m_pCur(0), m_pEnd(0)
{
	AddField("Host", pHost);
	AddField("Connection", "close");
	str_copy(m_aURI, pURI, sizeof(m_aURI));
}

CRequest::~CRequest()
{
	if(m_pBody)
		mem_free(m_pBody);
}

void CRequest::GenerateHeader()
{
	const char *pMethod = "GET"; // default
	if(m_Method == HTTP_POST)
		pMethod = "POST";
	else if(m_Method == HTTP_PUT)
		pMethod = "PUT";

	str_format(m_aHeader, sizeof(m_aHeader)-3, "%s %s%s HTTP/1.1\r\n", pMethod, m_aURI[0] == '/' ? "" : "/", m_aURI);

	for(int i = 0; i < m_Fields.size(); i++)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%s: %s\r\n", m_Fields[i].m_aKey, m_Fields[i].m_aValue);
		strncat(m_aHeader, aBuf, sizeof(m_aHeader)-str_length(m_aHeader)-3);
	}

	strncat(m_aHeader, "\r\n", sizeof(m_aHeader)-str_length(m_aHeader)-1);
}

bool CRequest::Finish()
{
	if(m_Method != HTTP_GET)
	{
		int ContentLength = 0;
		if(m_File)
		{
			// TODO: upload
			AddField("Content-Type", "multipart/form-data; boundary=frontier");
		}
		else if(m_pBody)
		{
			ContentLength = m_BodySize;
		}

		if(ContentLength == 0)
			return false;

		AddField("Content-Length", ContentLength);
	}
	GenerateHeader();
	m_Finish = true;
	return true;
}

bool CRequest::SetBody(const char *pData, int Size, const char *pContentType)
{
	if(Size <= 0 || m_pBody || m_Finish)
		return false;

	m_BodySize = Size;
	m_pBody = (char *)mem_alloc(m_BodySize, 1);
	mem_copy(m_pBody, pData, m_BodySize);
	AddField("Content-Type", pContentType);
	return true;
}

int CRequest::GetData(char *pBuf, int MaxSize)
{
	if(!m_Finish)
		return -1;

	int Size = -1;

	if(m_State == STATE_NONE)
	{
		m_pCur = m_aHeader;
		m_pEnd = m_aHeader+str_length(m_aHeader);
		m_State = STATE_HEADER;
	}

	if(m_State == STATE_HEADER)
	{
		if(m_pCur >= m_pEnd)
		{
			if(m_Method == HTTP_GET)
			{
				return 0;
			}
			else if(m_File)
			{
				m_pCur = 0; // TODO: upload
				m_pEnd = 0;
				m_State = STATE_FILE_START;
			}
			else
			{
				m_pCur = m_pBody;
				m_pEnd = m_pBody + m_BodySize;
				m_State = STATE_BODY;
			}
		}
	}
	else if(m_State == STATE_BODY)
	{
		if(m_pCur >= m_pEnd)
			return 0;
	}
	else if(m_State == STATE_FILE_START) // TODO: upload
	{
		if(m_pCur >= m_pEnd)
			m_State = STATE_FILE;
	}
	else if(m_State == STATE_FILE) // TODO: upload
	{
	}
	else if(m_State == STATE_FILE_END) // TODO: upload
	{
		if(m_pCur >= m_pEnd)
			return 0;
	}

	if(m_State != STATE_FILE)
	{
		Size = min(m_pEnd-m_pCur, MaxSize);
		mem_copy(pBuf, m_pCur, Size);
		m_pCur += Size;
	}

	if(Size == -1)
		CloseFiles();

	return Size;
}

void CRequest::MoveCursor(int Bytes)
{
	if(m_State != STATE_FILE)
	{
		m_pCur = min(m_pCur + Bytes, m_pEnd);
	}
	else
	{
		// TODO: upload
	}
}
