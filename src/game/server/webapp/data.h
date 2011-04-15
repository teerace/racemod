#ifndef GAME_SERVER_WEBAPP_DATA_H
#define GAME_SERVER_WEBAPP_DATA_H

enum
{
	WEB_USER_AUTH = 0,
	WEB_USER_RANK,
	WEB_USER_TOP,
	WEB_PING_PING,
	WEB_MAP_LIST,
	WEB_MAP_DOWNLOADED,
	WEB_RUN,
	
	UPLOAD_DEMO = 0,
	UPLOAD_GHOST
};

class IDataIn
{
public:
	class CWebapp *m_pWebapp;
	int m_ClientID;
};

class IDataOut
{
public:
	IDataOut *m_pNext;
	int m_Type;
};


class CUpload
{
public:
	CUpload(int Type) { m_Type = Type; }
	int m_Type;
	int m_ClientID;
	int m_UserID;
	char m_aFilename[256];
};
	
#endif
