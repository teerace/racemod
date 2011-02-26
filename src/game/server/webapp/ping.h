#ifndef GAME_SERVER_WEBAPP_PING_H
#define GAME_SERVER_WEBAPP_PING_H

#include "data.h"

class CWebPing
{

public:
	class CParam : public IDataIn
	{
	public:
		array<std::string> m_Name;
		array<int> m_UserID;
	};
	
	class COut : public IDataOut
	{
	public:
		COut(bool Online)
		{
			m_Online = Online;
			m_Type = WEB_PING_PING;
		}
		bool m_Online;
	};
	
	static int Ping(void *pUserData);
};

#endif
