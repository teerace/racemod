#ifndef GAME_SERVER_WEBAPP_DATA_H
#define GAME_SERVER_WEBAPP_DATA_H

// TODO: is this data exchange really a good idea? is there a better way to do this?

enum
{
	WEB_USER_AUTH = 0,
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
	int m_ClientID;
};

#endif
