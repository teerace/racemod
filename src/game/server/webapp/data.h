#ifndef GAME_SERVER_WEBAPP_DATA_H
#define GAME_SERVER_WEBAPP_DATA_H

enum
{
	WEB_USER_AUTH = 0,
	WEB_PING_PING,
	WEB_MAP_LIST
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

#endif
