/* Webapp Class by Sushi and Redix */
#ifndef GAME_HTTP_CON_H
#define GAME_HTTP_CON_H

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
		STATE_RECV,
	};
	
	NETSOCKET m_Socket;
	NETADDR m_Addr;
	int m_State;
	CBufferStream m_HeaderBuffer;
	CHeader m_Header;

	char *m_pRequest;
	int m_RequestSize;
	IOHANDLE m_RequestFile;
	char *m_pRequestCur;
	char *m_pRequestEnd;

	int64 m_LastActionTime;
	
public:
	class CWebData *m_pUserData;
	class IStream *m_pResponse;
	int m_Type;
	int64 m_StartTime;
	
	CHttpConnection() : m_State(STATE_NONE), m_pRequest(0), m_RequestSize(0), m_RequestFile(0), m_pRequestCur(0),
		m_pRequestEnd(0), m_LastActionTime(-1), m_pUserData(0), m_pResponse(0), m_Type(-1), m_StartTime(-1)  {}
	~CHttpConnection();
	
	bool Create(NETADDR Addr, int Type, IStream *pResponse);
	void Close();
	
	void SetRequest(const char *pData, int Size, IOHANDLE RequestFile = 0);
	int Update();
};

#endif
