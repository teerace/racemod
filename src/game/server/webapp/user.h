#ifndef GAME_SERVER_WEBAPP_USER_H
#define GAME_SERVER_WEBAPP_USER_H

#include "data.h"

class CWebUser
{
public:
	class CData : public IDataIn
	{
	public:
		char m_aUsername[32];
		char m_aPassword[32];
		char m_aToken[32];
	};
	
	class COut : public IDataOut
	{
	public:
		COut(int ClientID)
		{
			m_ClientID = ClientID;
			m_Type = WEB_USER_AUTH;
			m_UserID = -1;
		}
		int m_UserID;
	};
	
	static int Auth(void *pUserData);
	//static int AuthToken(void *pUserData);
};

#endif
