#include <zlib.h>
#include <base/system.h>

#include "http.h"

#include <curl/curl.h>

IResponse::IResponse() : m_StatusCode(-1), m_Size(0) { }

IResponse::~IResponse() { }

void IResponse::InitHandle(CURL *pHandle)
{
	curl_easy_setopt(pHandle, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
}

size_t IResponse::WriteCallback(char *pData, size_t Size, size_t Number, void *pUser)
{
	return ((IResponse*)pUser)->WriteData(pData, Size * Number);
}

CBufferResponse::CBufferResponse() : IResponse(), m_pData(0), m_BufferSize(0)
{
	ResizeBuffer(1024);
}

CBufferResponse::~CBufferResponse()
{
	if(m_pData)
		mem_free(m_pData);
}

int CBufferResponse::WriteData(const char *pData, int Len)
{
	if(m_Size + Len > m_BufferSize)
		ResizeBuffer(m_Size + Len);
	mem_copy(m_pData + m_Size, pData, Len);
	m_Size += Len;
	return Len;
}

bool CBufferResponse::ResizeBuffer(int NeededSize)
{
	if(NeededSize < m_BufferSize || NeededSize <= 0)
		return false;
	else if(NeededSize == m_BufferSize)
		return true;
	m_BufferSize = NeededSize;

	if(m_pData)
	{
		char *pTmp = (char *)mem_alloc(m_BufferSize, 1);
		mem_copy(pTmp, m_pData, m_Size);
		mem_free(m_pData);
		m_pData = pTmp;
	}
	else
		m_pData = (char *)mem_alloc(m_BufferSize, 1);
	return true;
}

CFileResponse::CFileResponse(IOHANDLE File, const char *pFilename) : IResponse(), m_File(File), m_Crc(0)
{
	str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
}

CFileResponse::~CFileResponse()
{
	Finalize();
}

int CFileResponse::WriteData(const char *pData, int Len)
{
	if(!m_File)
		return -1;
	int Bytes = io_write(m_File, pData, Len);
	m_Crc = crc32(m_Crc, (const Bytef*)pData, Bytes);
	m_Size += Bytes;
	return Bytes;
}

void CFileResponse::Finalize()
{
	if(m_File)
	{
		io_close(m_File);
		m_File = 0;
	}
}
