/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_FLAG_H
#define GAME_SERVER_ENTITIES_FLAG_H

#include <game/server/entity.h>

class CFlag : public CEntity
{
public:
	static const int ms_PhysSize = 14;
	class CCharacter *m_pCarryingCharacter;
	vec2 m_Vel;
	
	int m_Team;
	
	CFlag(CGameWorld *pGameWorld, int Team, vec2 Pos, class CCharacter *pOwner);

	virtual void Reset();
	virtual void TickDeferedLate();
	virtual void Snap(int SnappingClient);
};
#endif
