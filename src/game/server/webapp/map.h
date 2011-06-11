#ifndef GAME_SERVER_WEBAPP_MAP_H
#define GAME_SERVER_WEBAPP_MAP_H

#include "../score.h"
#include <game/data.h>

class CWebMap
{
public:
	class CParam : public IDataIn
	{
	public:
		bool m_CrcCheck;
		array<std::string> m_lMapName;
		array<std::string> m_lMapURL;
	};
	
	class COut : public IDataOut
	{
	public:
		bool m_CrcCheck;
		COut(int Type) { m_Type = Type; }
		array<int> m_lMapRunCount;
		array<int> m_lMapID;
		array<std::string> m_lMapCrc;
		array<std::string> m_lMapName;
		array<std::string> m_lMapURL;
		array<std::string> m_lMapAuthor;
		array<CPlayerData> m_lMapRecord;
	};
	
	static int LoadList(void *pUserData);
	static int DownloadMaps(void *pUserData);
};

#endif
