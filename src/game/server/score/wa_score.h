#ifndef GAME_SERVER_WASCORE_H
#define GAME_SERVER_WASCORE_H

#include <base/tl/sorted_array.h>
#include "../score.h"

class CWebappScore : public IScore
{
	class CServerWebapp *m_pWebapp;
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;
	
	CServerWebapp *Webapp() { return m_pWebapp; }
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }
	
public:
	CWebappScore(CGameContext *pGameServer);
	~CWebappScore() {}
	
	void LoadScore(int ClientID) { LoadScore(ClientID, false); }
	void LoadScore(int ClientID, bool PrintRank);
	void SaveScore(int ClientID, float Time, float *pCpTime, bool NewRecord);
	
	void ShowTop5(int ClientID, int Debut=1);
	void ShowRank(int ClientID, const char *pName, bool Search=false);
};

#endif
