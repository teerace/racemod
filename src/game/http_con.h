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
	
	NETSOCKET m_Socket;
	NETADDR m_Addr;
	int m_State;
	CBufferStream m_HeaderBuffer;
	CHeader m_Header;

	char *m_pRequest;
	int m_RequestSize;
	IOHANDLE m_RequestFile;
	char m_aRequestFilename[512];
	char *m_pRequestCur;
	char *m_pRequestEnd;

	int64 m_StartTime;
	int64 m_LastActionTime;

	int SetState(int State, const char *pMsg = 0);
	
public:
	enum
	{
		STATE_NONE = 0,
		STATE_CONNECT,
		STATE_WAIT,
		STATE_SEND,
		STATE_RECV,
		STATE_DONE,
		STATE_ERROR
	};

	class CWebData *m_pUserData;
	class IStream *m_pResponse;
	int m_Type;
	
	CHttpConnection() : m_State(STATE_NONE), m_pRequest(0), m_RequestSize(0), m_RequestFile(0), m_pRequestCur(0),
		m_pRequestEnd(0), m_StartTime(-1), m_LastActionTime(-1), m_pUserData(0), m_pResponse(0), m_Type(-1)  {}
	~CHttpConnection();
	
	bool Create(NETADDR Addr, int Type, IStream *pResponse, int64 StartTime = -1);
	void Clear();
	void Close();

	void SetRequest(const char *pData, int Size, IOHANDLE RequestFile = 0, const char *pFilename = 0);
	int Update();

	int State() { return m_State; }
	int StatusCode() { return m_Header.m_StatusCode; }
	const char *GetPath() { return m_aRequestFilename; }
	const char *GetFilename()
	{
		char *pShort = m_aRequestFilename;
		for(char *pCur = pShort; *pCur; pCur++)
		{
			if(*pCur == '/' || *pCur == '\\')
				pShort = pCur+1;
		}
		return pShort;
	}
};

#endif
