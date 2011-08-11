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
	
	NETSOCKET m_Socket;
	CBufferStream m_HeaderBuffer;
	CHeader m_Header;
	bool m_Connected;
	
public:
	void *m_pUserData;
	class IStream *m_pResponse;
	int m_Type;
	
	CHttpConnection() : m_Connected(false), m_pResponse(0), m_Type(-1), m_pUserData(0) {}
	~CHttpConnection();
	
	bool Create(NETADDR Addr, int Type, IStream *pResponse);
	void Close();
	
	bool Send(const char *pData, int Size);
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
