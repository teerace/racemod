#ifndef GAME_SERVER_WEBAPP_USER_H
#define GAME_SERVER_WEBAPP_USER_H

#include "../score.h"
#include <game/data.h>

class CWebUser
{
public:
	class CParam : public IDataIn
	{
	public:
		CParam()
		{
			m_PrintRank = 1;
			m_GetBestRun = 0;
		}
		int m_ClientID;
		char m_aName[64];
		// auth token
		char m_aToken[32];
		// skin
		int m_UserID; // rank
		int m_PlayTime;
		char m_SkinName[64];
		bool m_UseCustomColor;
		int m_ColorBody;
		int m_ColorFeet;
		// rank
		bool m_PrintRank;
		bool m_GetBestRun;
		// auth
		class CUnpacker m_Unpacker;
	};
	
	class COut : public IDataOut
	{
	public:
		COut(int Type, int ClientID)
		{
			m_Type = Type;
			m_ClientID = ClientID;
			m_UserID = 0;
			m_GlobalRank = 0;
			m_MatchFound = 1;
			m_IsStaff = 0;
		}
		int m_ClientID;
		int m_UserID;
		char m_aUsername[32];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_GlobalRank;
		int m_MapRank;
		CPlayerData m_BestRun;
		bool m_GetBestRun;
		bool m_PrintRank;
		bool m_MatchFound;
		bool m_IsStaff;
		// auth
		class CUnpacker m_Unpacker;
	};
	
	//static int Auth(void *pUserData);
	static int AuthToken(void *pUserData);
	
	static int UpdateSkin(void *pUserData);
	
	static int GetRank(void *pUserData);
	
	static int PlayTime(void *pUserData);
};

#endif
