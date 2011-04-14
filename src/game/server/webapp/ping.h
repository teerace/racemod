#ifndef GAME_SERVER_WEBAPP_PING_H
#define GAME_SERVER_WEBAPP_PING_H

#include "data.h"

class CWebPing
{
public:
	class CParam : public IDataIn
	{
	public:
		CParam()
		{
			m_CrcCheck = false;
		}
		bool m_CrcCheck;
		array<std::string> m_lName;
		array<std::string> m_lClan;
		array<int> m_lUserID;
	};
	
	class COut : public IDataOut
	{
	public:
		COut(bool Online, bool CrcCheck)
		{
			m_Online = Online;
			m_CrcCheck = CrcCheck;
			m_Type = WEB_PING_PING;
		}
		bool m_Online;
		bool m_CrcCheck;
	};
	
	static int Ping(void *pUserData);
};

#endif
