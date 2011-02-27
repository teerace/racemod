#ifndef GAME_SERVER_WEBAPP_MAP_H
#define GAME_SERVER_WEBAPP_MAP_H

#include "data.h"

class CWebMap
{

public:
	class CParam : public IDataIn
	{
	public:
		array<std::string> m_DownloadList;
		class IStorage *m_pStorage;
	};
	
	class COut : public IDataOut
	{
	public:
		COut(int Type)
		{
			m_Type = Type;
			m_MapList.clear();
		}
		array<std::string> m_MapList;
	};
	
	static int LoadList(void *pUserData);
	static int DownloadMaps(void *pUserData);
};

#endif
