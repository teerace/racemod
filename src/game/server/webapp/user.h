#ifndef GAME_SERVER_WEBAPP_USER_H
#define GAME_SERVER_WEBAPP_USER_H

#include "data.h"

class CWebUser
{
public:
	class CParam : public IDataIn
	{
	public:
		// auth
		char m_aUsername[32];
		char m_aPassword[32];
		// auth token
		char m_aToken[32];
		// skin
		int m_UserID; // rank
		char m_SkinName[64];
		int m_ColorBody;
		int m_ColorFeet;
		// rank
		bool m_PrintRank;
	};
	
	class COut : public IDataOut
	{
	public:
		COut(int Type, int ClientID)
		{
			m_Type = Type;
			m_ClientID = ClientID;
			m_UserID = -1;
		}
		int m_ClientID;
		int m_UserID;
		char m_aUsername[32];
		int m_GlobalRank;
		int m_MapRank;
		bool m_PrintRank;
	};
	
	//static int Auth(void *pUserData);
	static int AuthToken(void *pUserData);
	
	static int UpdateSkin(void *pUserData);
	
	static int GetRank(void *pUserData);
};

#endif
