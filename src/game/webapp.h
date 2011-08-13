/* Webapp Class by Sushi and Redix */
#ifndef GAME_WEBAPP_H
#define GAME_WEBAPP_H

#include <base/tl/array.h>
#include "stream.h"

class CHttpConnection
{
	class CHeader
	{
	public:
		int m_Size;
		int m_StatusCode;
		long m_ContentLength;
		bool m_Error;
		
		CHeader() : m_Size(-1), m_StatusCode(0), m_ContentLength(-1), m_Error(false) {}
		bool Parse(char *pStr);
	};

	enum
	{
		STATE_NONE = 0,
		STATE_CONNECT,
		STATE_WAIT,
		STATE_SEND,
		STATE_RECV
	};
	
	NETSOCKET m_Socket;
	NETADDR m_Addr;
	int m_State;
	CBufferStream m_HeaderBuffer;
	CHeader m_Header;
	char *m_pRequest;
	int m_RequestSize;
	int m_RequestOffset;
	
public:
	void *m_pUserData;
	class IStream *m_pResponse;
	int m_Type;
	
	CHttpConnection() : m_State(STATE_NONE), m_pResponse(0), m_Type(-1), m_pUserData(0), m_pRequest(0), m_RequestSize(0), m_RequestOffset(0)  {}
	~CHttpConnection();
	
	bool Create(NETADDR Addr, int Type, IStream *pResponse);
	void Close();
	
	void SetRequest(const char *pData, int Size);
	int Update();
};

class IWebapp
{
	NETADDR m_Addr;
	array<CHttpConnection*> m_Connections;

public:
	IWebapp(const char* WebappIp);
	virtual ~IWebapp() {};
	
	int Update();
	virtual bool SendRequest(const char *pInString, int Type, class IStream *pResponse, void *pUserData = 0);
	
	virtual void OnResponse(int Type, IStream *pData, void *pUserData, int StatusCode) = 0;
};

#endif
