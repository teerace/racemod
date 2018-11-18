#ifndef ENGINE_SHARED_HTTP_H
#define ENGINE_SHARED_HTTP_H

#include <base/tl/array.h>

#include <engine/engine.h>

#define CURL_NO_OLDIES

typedef struct Curl_easy CURL;
typedef struct Curl_multi CURLM;
typedef struct curl_mime_s curl_mime;
struct curl_slist;

enum
{
	HTTP_PRIORITY_HIGH,
	HTTP_PRIORITY_LOW
};

class IHttp
{
public:
	static const char *GetFilename(const char *pFilepath);
	static void EscapeUrl(char *pBuf, int Size, const char *pStr);
};

class IResponse
{
	friend class CHttpClient;

	int m_StatusCode;

	static size_t WriteCallback(char *pData, size_t Size, size_t Number, void *pUser);

	void InitHandle(CURL *pHandle);
	virtual int WriteData(const char *pData, int Len) = 0;

	virtual void Finalize() { };

protected:
	int m_Size;

	IResponse();

public:
	virtual ~IResponse();

	int Size() const { return m_Size; }
	int StatusCode() const { return m_StatusCode; }

	virtual bool IsFile() const = 0;
};

class CBufferResponse : public IResponse
{
	char *m_pData;
	int m_BufferSize;

	bool ResizeBuffer(int NeededSize);
	int WriteData(const char *pData, int Len);

public:
	CBufferResponse();
	virtual ~CBufferResponse();

	const char *GetBody() const { return m_pData; }

	bool IsFile() const { return false; }
};


class CFileResponse : public IResponse
{
	IOHANDLE m_File;
	unsigned m_Crc;
	char m_aFilename[512];

	int WriteData(const char *pData, int Len);
	void Finalize();

public:
	CFileResponse(IOHANDLE File, const char *pFilename);
	virtual ~CFileResponse();

	unsigned GetCrc() const { return m_Crc; }
	const char *GetPath() const { return m_aFilename; }

	bool IsFile() const { return true; }
};

typedef void(*FHttpCallback)(IResponse *pResponse, bool Error, void *pUserData);

class IRequest
{
	friend class CHttpClient;

	curl_slist *m_pHeaderList;

	char m_aURI[256];


	virtual int ReadData(char *pBuf, int MaxSize) = 0;
	virtual int GetSize() = 0;

	virtual void Finalize() { };

protected:
	int m_Method;
	curl_mime *m_pMime;

	static size_t ReadCallback(char *pBuf, size_t Size, size_t Number, void *pUser);

	IRequest(int Method, const char *pURI);

	virtual void InitHandle(CURL *pHandle);

public:
	enum
	{
		HTTP_GET = 0,
		HTTP_POST,
		HTTP_PUT
	};

	virtual ~IRequest();

	void AddField(const char *pKey, const char *pValue);
	void AddField(const char *pKey, int Value);
};

class CBufferRequest : public IRequest
{
	char *m_pBody;
	int m_BodySize;

	char *m_pCur;

	int ReadData(char *pBuf, int MaxSize);
	int GetSize() { return m_BodySize; }

public:
	CBufferRequest(int Method, const char *pURI);
	virtual ~CBufferRequest();

	void SetBody(const char *pData, int Size, const char *pContentType);
};

class CFileRequest : public IRequest
{
	IOHANDLE m_File;
	char m_aMimeName[128];
	char m_aFilename[128];

	void InitHandle(CURL *pHandle);
	int ReadData(char *pBuf, int MaxSize);
	int GetSize();
	void Finalize();

public:
	CFileRequest(const char *pURI);
	virtual ~CFileRequest();

	void SetFile(IOHANDLE File, const char *pFilename, const char *pUploadName);
};

class CRequestInfo
{
	friend class CHttpClient;

	char m_aAddr[256];

	CHostLookup m_Lookup;
	IRequest *m_pRequest;
	IResponse *m_pResponse;

	int m_Priority;

	FHttpCallback m_pfnCallback;
	void *m_pUserData;

	CURL *m_pHandle;
	curl_slist *m_pHostResolve;

public:
	CRequestInfo(const char *pAddr);
	CRequestInfo(const char *pAddr, IOHANDLE File, const char *pFilename);

	virtual ~CRequestInfo();

	void SetPriority(int Priority) { m_Priority = Priority; }
	void SetCallback(FHttpCallback pfnCallback, void *pUserData = 0);
	void ExecuteCallback(IResponse *pResponse, bool Error) const;
};

class CHttpClient
{
	enum
	{
		HTTP_MAX_ACTIVE_HANDLES = 4,
		HTTP_MAX_LOW_PRIORITY_HANDLES=2
	};

	CRequestInfo *m_apActiveHandles[HTTP_MAX_ACTIVE_HANDLES];
	array<CRequestInfo*> m_lPendingRequests;

	IEngine *m_pEngine;

	void StartHandle(CRequestInfo *pInfo);
	void FetchRequest(int Priority, int Max);
	int GetRequestInfo(CURL *pHandle) const;

	CURLM *m_pMultiHandle;

public:
	CHttpClient();
	virtual ~CHttpClient();

	void Init(IEngine *pEngine) { m_pEngine = pEngine; }

	void Send(CRequestInfo *pInfo, IRequest *pRequest);
	void Update();

	bool HasActiveConnection() const;
};

#endif
