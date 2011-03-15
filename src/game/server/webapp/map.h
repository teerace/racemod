#ifndef GAME_SERVER_WEBAPP_MAP_H
#define GAME_SERVER_WEBAPP_MAP_H

#include "data.h"

class CWebMap
{
public:
	class CParam : public IDataIn
	{
	public:
		array<std::string> m_MapList;
		array<std::string> m_MapURL;
	};
	
	class COut : public IDataOut
	{
	public:
		COut(int Type) { m_Type = Type; }
		array<std::string> m_MapList;
		array<std::string> m_MapURL;
		array<int> m_MapID;
	};
	
	static int LoadList(void *pUserData);
	static int DownloadMaps(void *pUserData);
};

#endif
