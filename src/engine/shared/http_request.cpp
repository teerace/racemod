#include <base/system.h>
#include <base/math.h>

#include "http.h"

#include <curl/curl.h>

IRequest::IRequest(int Method, const char *pURI) : m_pHeaderList(0), m_Method(Method), m_pMime(0)
{
	str_copy(m_aURI, pURI, sizeof(m_aURI));
}

IRequest::~IRequest()
{
	if(m_pHeaderList)
		curl_slist_free_all(m_pHeaderList);
	if(m_pMime)
		curl_mime_free(m_pMime);
}


void IRequest::InitHandle(CURL *pHandle)
{
	if(m_pHeaderList)
		curl_easy_setopt(pHandle, CURLOPT_HTTPHEADER, m_pHeaderList);

	if(m_Method == HTTP_GET)
	{
		curl_easy_setopt(pHandle, CURLOPT_WRITEDATA, this);
	}
	else if(m_Method == HTTP_POST)
	{
		curl_easy_setopt(pHandle, CURLOPT_POST, 1L);

		if(m_pMime)
		{
			curl_easy_setopt(pHandle, CURLOPT_MIMEPOST, m_pMime);
		}
		else
		{
			curl_easy_setopt(pHandle, CURLOPT_READDATA, this);
			curl_easy_setopt(pHandle, CURLOPT_READFUNCTION, ReadCallback);
			curl_easy_setopt(pHandle, CURLOPT_POSTFIELDSIZE, GetSize());
		}
	}
	else // HTTP_PUT
	{
		curl_easy_setopt(pHandle, CURLOPT_UPLOAD, 1L);

		curl_easy_setopt(pHandle, CURLOPT_READDATA, this);
		curl_easy_setopt(pHandle, CURLOPT_READFUNCTION, ReadCallback);
		curl_easy_setopt(pHandle, CURLOPT_INFILESIZE, GetSize());
	}
}

size_t IRequest::ReadCallback(char *pBuf, size_t Size, size_t Number, void *pUser)
{
	return ((IRequest*)pUser)->ReadData(pBuf, Size * Number);
}

void IRequest::AddField(const char *pKey, const char *pValue)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %s", pKey, pValue);
	m_pHeaderList = curl_slist_append(m_pHeaderList, aBuf);
}

void IRequest::AddField(const char *pKey, int Value)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %d", pKey, Value);
	m_pHeaderList = curl_slist_append(m_pHeaderList, aBuf);
}

CBufferRequest::CBufferRequest(int Method, const char *pURI) : IRequest(Method, pURI), m_pBody(0), m_BodySize(0), m_pCur(0) { }

CBufferRequest::~CBufferRequest()
{
	if(m_pBody)
		mem_free(m_pBody);
}

void CBufferRequest::SetBody(const char *pData, int Size, const char *pContentType)
{
	if(Size <= 0 || m_pBody || m_Method == HTTP_GET)
		return;

	AddField("Content-Type", pContentType);
	m_BodySize = Size;
	m_pBody = (char *)mem_alloc(m_BodySize, 1);
	mem_copy(m_pBody, pData, m_BodySize);
	m_pCur = m_pBody;
}

int CBufferRequest::ReadData(char *pBuf, int MaxSize)
{
	const char *pEnd = m_pBody + m_BodySize;
	int Size = min((int)(pEnd-m_pCur), MaxSize);
	mem_copy(pBuf, m_pCur, Size);
	m_pCur += Size;
	return Size;
}

CFileRequest::CFileRequest(const char *pURI) : IRequest(HTTP_POST, pURI), m_File(0) { }

CFileRequest::~CFileRequest()
{
	Finalize();
}

void CFileRequest::InitHandle(CURL *pHandle)
{
	m_pMime = curl_mime_init(pHandle);

	curl_mimepart *pField = curl_mime_addpart(m_pMime);
	curl_mime_name(pField, m_aMimeName);
	curl_mime_filename(pField, m_aFilename);
	curl_mime_data_cb(pField, GetSize(), ReadCallback, 0, 0, this);

	IRequest::InitHandle(pHandle);
}

void CFileRequest::SetFile(IOHANDLE File, const char *pFilename, const char *pUploadName)
{
	m_File = File;
	str_copy(m_aMimeName, pUploadName, sizeof(m_aMimeName));
	str_copy(m_aFilename, IHttp::GetFilename(pFilename), sizeof(m_aFilename));
}

int CFileRequest::ReadData(char *pBuf, int MaxSize)
{
	if(!m_File)
		return -1;
	return io_read(m_File, pBuf, MaxSize);
}

int CFileRequest::GetSize()
{
	return io_length(m_File);
}

void CFileRequest::Finalize()
{
	if(m_File)
	{
		io_close(m_File);
		m_File = 0;
	}
}
