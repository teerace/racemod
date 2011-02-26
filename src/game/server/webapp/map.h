#ifndef GAME_SERVER_WEBAPP_MAP_H
#define GAME_SERVER_WEBAPP_MAP_H

#include "data.h"

class CWebMap
{

public:
	class CParam : public IDataIn
	{
	};
	
	class COut : public IDataOut
	{
	public:
		COut()
		{
			m_Type = WEB_MAP_LIST;
			m_MapList.clear();
		}
		array<std::string> m_MapList;
	};
	
	static int LoadList(void *pUserData);
};

#endif
