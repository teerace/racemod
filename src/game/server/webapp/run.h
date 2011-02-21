#ifndef GAME_SERVER_WEBAPP_RUN_H
#define GAME_SERVER_WEBAPP_RUN_H

#include <game/server/score.h>

#include "data.h"

class CWebRun
{
public:
	class CData : public IDataIn
	{
	public:
		int m_UserID;
		char m_aName[MAX_NAME_LENGTH];
		float m_Time;
		float m_aCpTime[NUM_CHECKPOINTS];
	};
	
	static int Post(void *pUserData);
};

#endif
