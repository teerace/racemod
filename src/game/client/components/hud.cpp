/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/demo.h>
#include <engine/shared/config.h>
#include <engine/serverbrowser.h>

#include <game/generated/protocol.h>
#include <game/generated/client_data.h>
#include <game/layers.h>
#include <game/teerace.h>
#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include <game/client/render.h>

#include "controls.h"
#include "camera.h"
#include "hud.h"
#include "voting.h"
#include "binds.h"
#include "menus.h"
#include "race_demo.h"

CHud::CHud()
{
	// won't work if zero
	m_AverageFPS = 1.0f;
	
	OnReset();
}

void CHud::OnReset()
{
	m_CheckpointTick = -1;
	m_CheckpointDiff = 0;
	m_RaceTime = 0;
	m_ServerRecord = 0;
}

void CHud::RenderGameTimer()
{
	float Half = 300.0f*Graphics()->ScreenAspect()/2.0f;

	if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_SUDDENDEATH))
	{
		char Buf[32];
		int Time = 0;
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0)
		{
			Time = m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit*60 - ((Client()->GameTick()-m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick)/Client()->GameTickSpeed());

			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
				Time = 0;
		}
		else
			Time = (Client()->GameTick()-m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick)/Client()->GameTickSpeed();

		int RaceTime = (Client()->GameTick()+m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)*10/Client()->GameTickSpeed();
		if(g_Config.m_ClRaceReplaceGametime && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_RACETIME)
			str_format(Buf, sizeof(Buf), "%02d:%02d.%d", RaceTime/(60*10), (RaceTime/10)%60, RaceTime%10);
		else if(g_Config.m_ClRaceReplaceGametime && m_RaceTime)
			str_format(Buf, sizeof(Buf), "%02d:%02d", m_RaceTime/60, m_RaceTime%60);
		else
			str_format(Buf, sizeof(Buf), "%d:%02d", Time/60, Time%60);
		float FontSize = 10.0f;
		float w = TextRender()->TextWidth(0, FontSize, Buf, -1);
		// last 60 sec red, last 10 sec blink
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && Time <= 60 && m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0)
		{
			float Alpha = Time <= 10 && (2*time_get()/time_freq()) % 2 ? 0.5f : 1.0f;
			TextRender()->TextColor(1.0f, 0.25f, 0.25f, Alpha);
		}
		TextRender()->Text(0, Half-w/2, 2, FontSize, Buf, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void CHud::RenderPauseNotification()
{
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED &&
		!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
	{
		const char *pText = Localize("Game paused");
		float FontSize = 20.0f;
		float w = TextRender()->TextWidth(0, FontSize,pText, -1);
		TextRender()->Text(0, 150.0f*Graphics()->ScreenAspect()+-w/2.0f, 50.0f, FontSize, pText, -1);

		if(m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)
		{
			char Buf[256];
			int Seconds = m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer/SERVER_TICK_SPEED;
			if(Seconds < 5)
				str_format(Buf, sizeof(Buf), "%d.%d", Seconds, (m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer*10/SERVER_TICK_SPEED)%10);
			else
				str_format(Buf, sizeof(Buf), "%d", Seconds);
			w = TextRender()->TextWidth(0, FontSize, Buf, -1);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect()+-w/2, 75, FontSize, Buf, -1);
		}
	}
}

void CHud::RenderSuddenDeath()
{
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_SUDDENDEATH)
	{
		float Half = 300.0f*Graphics()->ScreenAspect()/2.0f;
		const char *pText = Localize("Sudden Death");
		float FontSize = 12.0f;
		float w = TextRender()->TextWidth(0, FontSize, pText, -1);
		TextRender()->Text(0, Half-w/2, 2, FontSize, pText, -1);
	}
}

void CHud::RenderScoreHud()
{
	// render small score hud

	if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
	{
		int GameFlags = m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags;
		float Whole = 300*Graphics()->ScreenAspect();
		float StartY = 229.0f;

		if(GameFlags&GAMEFLAG_TEAMS && m_pClient->m_Snap.m_pGameDataObj)
		{
			char aScoreTeam[2][32];
			str_format(aScoreTeam[TEAM_RED], sizeof(aScoreTeam)/2, "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed);
			str_format(aScoreTeam[TEAM_BLUE], sizeof(aScoreTeam)/2, "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue);
			float aScoreTeamWidth[2] = { TextRender()->TextWidth(0, 14.0f, aScoreTeam[TEAM_RED], -1), TextRender()->TextWidth(0, 14.0f, aScoreTeam[TEAM_BLUE], -1) };
			int FlagCarrier[2] = { m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed, m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue };
			float ScoreWidthMax = max(max(aScoreTeamWidth[TEAM_RED], aScoreTeamWidth[TEAM_BLUE]), TextRender()->TextWidth(0, 14.0f, "100", -1));
			float Split = 3.0f;
			float ImageSize = GameFlags&GAMEFLAG_FLAGS ? 16.0f : Split;

			for(int t = 0; t < 2; t++)
			{
				// draw box
				Graphics()->BlendNormal();
				Graphics()->TextureSet(-1);
				Graphics()->QuadsBegin();
				if(t == 0)
					Graphics()->SetColor(1.0f, 0.0f, 0.0f, 0.25f);
				else
					Graphics()->SetColor(0.0f, 0.0f, 1.0f, 0.25f);
				RenderTools()->DrawRoundRectExt(Whole-ScoreWidthMax-ImageSize-2*Split, StartY+t*20, ScoreWidthMax+ImageSize+2*Split, 18.0f, 5.0f, CUI::CORNER_L);
				Graphics()->QuadsEnd();

				// draw score
				TextRender()->Text(0, Whole-ScoreWidthMax+(ScoreWidthMax-aScoreTeamWidth[t])/2-Split, StartY+t*20, 14.0f, aScoreTeam[t], -1);

				if(GameFlags&GAMEFLAG_FLAGS)
				{
					int BlinkTimer = (m_pClient->m_FlagDropTick[t] != 0 &&
										(Client()->GameTick()-m_pClient->m_FlagDropTick[t])/Client()->GameTickSpeed() >= 25) ? 10 : 20;
					if(FlagCarrier[t] == FLAG_ATSTAND || (FlagCarrier[t] == FLAG_TAKEN && ((Client()->GameTick()/BlinkTimer)&1)))
					{
						// draw flag
						Graphics()->BlendNormal();
						Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
						Graphics()->QuadsBegin();
						RenderTools()->SelectSprite(t==0?SPRITE_FLAG_RED:SPRITE_FLAG_BLUE);
						IGraphics::CQuadItem QuadItem(Whole-ScoreWidthMax-ImageSize, StartY+1.0f+t*20, ImageSize/2, ImageSize);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
						Graphics()->QuadsEnd();
					}
					else if(FlagCarrier[t] >= 0)
					{
						// draw name of the flag holder
						int ID = FlagCarrier[t]%MAX_CLIENTS;
						const char *pName = m_pClient->m_aClients[ID].m_aName;
						float w = TextRender()->TextWidth(0, 8.0f, pName, -1);
						TextRender()->Text(0, min(Whole-w-1.0f, Whole-ScoreWidthMax-ImageSize-2*Split), StartY+(t+1)*20.0f-3.0f, 8.0f, pName, -1);

						// draw tee of the flag holder
						CTeeRenderInfo Info = m_pClient->m_aClients[ID].m_RenderInfo;
						Info.m_Size = 18.0f;
						RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, EMOTE_NORMAL, vec2(1,0),
							vec2(Whole-ScoreWidthMax-Info.m_Size/2-Split, StartY+1.0f+Info.m_Size/2+t*20));
					}
				}
				StartY += 8.0f;
			}
		}
		else
		{
			int Local = -1;
			int aPos[2] = { 1, 2 };
			const CNetObj_PlayerInfo *apPlayerInfo[2] = { 0, 0 };
			int i = 0;
			for(int t = 0; t < 2 && i < MAX_CLIENTS && m_pClient->m_Snap.m_paInfoByScore[i]; ++i)
			{
				if(m_pClient->m_Snap.m_paInfoByScore[i]->m_Team != TEAM_SPECTATORS)
				{
					apPlayerInfo[t] = m_pClient->m_Snap.m_paInfoByScore[i];
					if(apPlayerInfo[t]->m_ClientID == m_pClient->m_Snap.m_LocalClientID)
						Local = t;
					++t;
				}
			}
			// search local player info if not a spectator, nor within top2 scores
			if(Local == -1 && m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
			{
				for(; i < MAX_CLIENTS && m_pClient->m_Snap.m_paInfoByScore[i]; ++i)
				{
					if(m_pClient->m_Snap.m_paInfoByScore[i]->m_Team != TEAM_SPECTATORS)
						++aPos[1];
					if(m_pClient->m_Snap.m_paInfoByScore[i]->m_ClientID == m_pClient->m_Snap.m_LocalClientID)
					{
						apPlayerInfo[1] = m_pClient->m_Snap.m_paInfoByScore[i];
						Local = 1;
						break;
					}
				}
			}
			char aScore[2][32];
			for(int t = 0; t < 2; ++t)
			{
				if(apPlayerInfo[t])
					str_format(aScore[t], sizeof(aScore)/2, "%d", apPlayerInfo[t]->m_Score);
				else
					aScore[t][0] = 0;
			}
			float aScoreWidth[2] = {TextRender()->TextWidth(0, 14.0f, aScore[0], -1), TextRender()->TextWidth(0, 14.0f, aScore[1], -1)};
			float ScoreWidthMax = max(max(aScoreWidth[0], aScoreWidth[1]), TextRender()->TextWidth(0, 14.0f, "10", -1));
			float Split = 3.0f, ImageSize = 16.0f, PosSize = 16.0f;

			for(int t = 0; t < 2; t++)
			{
				// draw box
				Graphics()->BlendNormal();
				Graphics()->TextureSet(-1);
				Graphics()->QuadsBegin();
				if(t == Local)
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
				else
					Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.25f);
				RenderTools()->DrawRoundRectExt(Whole-ScoreWidthMax-ImageSize-2*Split-PosSize, StartY+t*20, ScoreWidthMax+ImageSize+2*Split+PosSize, 18.0f, 5.0f, CUI::CORNER_L);
				Graphics()->QuadsEnd();

				// draw score
				TextRender()->Text(0, Whole-ScoreWidthMax+(ScoreWidthMax-aScoreWidth[t])/2-Split, StartY+t*20, 14.0f, aScore[t], -1);

				if(apPlayerInfo[t])
 				{
					// draw name
					int ID = apPlayerInfo[t]->m_ClientID;
					const char *pName = m_pClient->m_aClients[ID].m_aName;
					float w = TextRender()->TextWidth(0, 8.0f, pName, -1);
					TextRender()->Text(0, min(Whole-w-1.0f, Whole-ScoreWidthMax-ImageSize-2*Split-PosSize), StartY+(t+1)*20.0f-3.0f, 8.0f, pName, -1);

					// draw tee
					CTeeRenderInfo Info = m_pClient->m_aClients[ID].m_RenderInfo;
 					Info.m_Size = 18.0f;
 					RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, EMOTE_NORMAL, vec2(1,0),
 						vec2(Whole-ScoreWidthMax-Info.m_Size/2-Split, StartY+1.0f+Info.m_Size/2+t*20));
				}

				// draw position
				char aBuf[32];
				str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
				TextRender()->Text(0, Whole-ScoreWidthMax-ImageSize-Split-PosSize, StartY+2.0f+t*20, 10.0f, aBuf, -1);

				StartY += 8.0f;
			}
		}
	}
}

void CHud::RenderWarmupTimer()
{
	// render warmup timer
	if(m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer > 0
		&& !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED)
		&& !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_RACETIME))
	{
		char Buf[256];
		float FontSize = 20.0f;
		float w = TextRender()->TextWidth(0, FontSize, Localize("Warmup"), -1);
		TextRender()->Text(0, 150*Graphics()->ScreenAspect()+-w/2, 50, FontSize, Localize("Warmup"), -1);

		int Seconds = m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer/SERVER_TICK_SPEED;
		if(Seconds < 5)
			str_format(Buf, sizeof(Buf), "%d.%d", Seconds, (m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer*10/SERVER_TICK_SPEED)%10);
		else
			str_format(Buf, sizeof(Buf), "%d", Seconds);
		w = TextRender()->TextWidth(0, FontSize, Buf, -1);
		TextRender()->Text(0, 150*Graphics()->ScreenAspect()+-w/2, 75, FontSize, Buf, -1);
	}
}

void CHud::MapscreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup)
{
	float Points[4];
	RenderTools()->MapscreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX/100.0f, pGroup->m_ParallaxY/100.0f,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), 1.0f, Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CHud::RenderFps()
{
	if(g_Config.m_ClShowfps)
	{
		// calculate avg. fps
		float FPS = 1.0f / Client()->RenderFrameTime();
		m_AverageFPS = (m_AverageFPS*(1.0f-(1.0f/m_AverageFPS))) + (FPS*(1.0f/m_AverageFPS));
		char Buf[512];
		str_format(Buf, sizeof(Buf), "%d", (int)m_AverageFPS);
		TextRender()->Text(0, m_Width-5-TextRender()->TextWidth(0,8,Buf,-1), 2, 8, Buf, -1);
	}
}

void CHud::RenderConnectionWarning()
{
	if(Client()->ConnectionProblems())
	{
		const char *pText = Localize("Connection Problems...");
		float w = TextRender()->TextWidth(0, 24, pText, -1);
		TextRender()->Text(0, 150*Graphics()->ScreenAspect()-w/2, 50, 24, pText, -1);
	}
}

void CHud::RenderTeambalanceWarning()
{
	// render prompt about team-balance
	bool Flash = time_get()/(time_freq()/2)%2 == 0;
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
	{
		int TeamDiff = m_pClient->m_Snap.m_aTeamSize[TEAM_RED]-m_pClient->m_Snap.m_aTeamSize[TEAM_BLUE];
		if (g_Config.m_ClWarningTeambalance && (TeamDiff >= 2 || TeamDiff <= -2))
		{
			const char *pText = Localize("Please balance teams!");
			if(Flash)
				TextRender()->TextColor(1,1,0.5f,1);
			else
				TextRender()->TextColor(0.7f,0.7f,0.2f,1.0f);
			TextRender()->Text(0x0, 5, 50, 6, pText, -1);
			TextRender()->TextColor(1,1,1,1);
		}
	}
}


void CHud::RenderVoting()
{
	if(!m_pClient->m_pVoting->IsVoting() || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.40f);
	RenderTools()->DrawRoundRect(-10, 60-2, 100+10+4+5, 46, 5.0f);
	Graphics()->QuadsEnd();

	TextRender()->TextColor(1,1,1,1);

	CTextCursor Cursor;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), Localize("%ds left"), m_pClient->m_pVoting->SecondsLeft());
	float tw = TextRender()->TextWidth(0x0, 6, aBuf, -1);
	TextRender()->SetCursor(&Cursor, 5.0f+100.0f-tw, 60.0f, 6.0f, TEXTFLAG_RENDER);
	TextRender()->TextEx(&Cursor, aBuf, -1);

	TextRender()->SetCursor(&Cursor, 5.0f, 60.0f, 6.0f, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = 100.0f-tw;
	Cursor.m_MaxLines = 3;
	TextRender()->TextEx(&Cursor, m_pClient->m_pVoting->VoteDescription(), -1);

	// reason
	str_format(aBuf, sizeof(aBuf), "%s %s", Localize("Reason:"), m_pClient->m_pVoting->VoteReason());
	TextRender()->SetCursor(&Cursor, 5.0f, 79.0f, 6.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = 100.0f;
	TextRender()->TextEx(&Cursor, aBuf, -1);

	CUIRect Base = {5, 88, 100, 4};
	m_pClient->m_pVoting->RenderBars(Base, false);

	const char *pYesKey = m_pClient->m_pBinds->GetKey("vote yes");
	const char *pNoKey = m_pClient->m_pBinds->GetKey("vote no");
	str_format(aBuf, sizeof(aBuf), "%s - %s", pYesKey, Localize("Vote yes"));
	Base.y += Base.h+1;
	UI()->DoLabel(&Base, aBuf, 6.0f, -1);

	str_format(aBuf, sizeof(aBuf), "%s - %s", Localize("Vote no"), pNoKey);
	UI()->DoLabel(&Base, aBuf, 6.0f, 1);
}

void CHud::RenderCursor()
{
	if(!m_pClient->m_Snap.m_pLocalCharacter || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	MapscreenToGroup(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y, Layers()->GameGroup());
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();

	// render cursor
	RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[m_pClient->m_Snap.m_pLocalCharacter->m_Weapon%NUM_WEAPONS].m_pSpriteCursor);
	float CursorSize = 64;
	RenderTools()->DrawSprite(m_pClient->m_pControls->m_TargetPos.x, m_pClient->m_pControls->m_TargetPos.y, CursorSize);
	Graphics()->QuadsEnd();
}

void CHud::RenderHealthAndAmmo(const CNetObj_Character *pCharacter)
{
	if(!pCharacter)
		return;

	//mapscreen_to_group(gacenter_x, center_y, layers_game_group());

	float x = 5;
	float y = 5;

	// render ammo count
	// render gui stuff

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);

	Graphics()->QuadsBegin();

	// if weaponstage is active, put a "glow" around the stage ammo
	RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[pCharacter->m_Weapon%NUM_WEAPONS].m_pSpriteProj);
	IGraphics::CQuadItem Array[10];
	int i;
	for (i = 0; i < min(pCharacter->m_AmmoCount, 10); i++)
		Array[i] = IGraphics::CQuadItem(x+i*12,y+24,10,10);
	Graphics()->QuadsDrawTL(Array, i);
	Graphics()->QuadsEnd();

	Graphics()->QuadsBegin();
	int h = 0;

	// render health
	RenderTools()->SelectSprite(SPRITE_HEALTH_FULL);
	for(; h < min(pCharacter->m_Health, 10); h++)
		Array[h] = IGraphics::CQuadItem(x+h*12,y,10,10);
	Graphics()->QuadsDrawTL(Array, h);

	i = 0;
	RenderTools()->SelectSprite(SPRITE_HEALTH_EMPTY);
	for(; h < 10; h++)
		Array[i++] = IGraphics::CQuadItem(x+h*12,y,10,10);
	Graphics()->QuadsDrawTL(Array, i);

	// render armor meter
	h = 0;
	RenderTools()->SelectSprite(SPRITE_ARMOR_FULL);
	for(; h < min(pCharacter->m_Armor, 10); h++)
		Array[h] = IGraphics::CQuadItem(x+h*12,y+12,10,10);
	Graphics()->QuadsDrawTL(Array, h);

	i = 0;
	RenderTools()->SelectSprite(SPRITE_ARMOR_EMPTY);
	for(; h < 10; h++)
		Array[i++] = IGraphics::CQuadItem(x+h*12,y+12,10,10);
	Graphics()->QuadsDrawTL(Array, i);
	Graphics()->QuadsEnd();
}

void CHud::RenderSpectatorHud()
{
	// draw the box
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.4f);
	RenderTools()->DrawRoundRectExt(m_Width-180.0f, m_Height-15.0f, 180.0f, 15.0f, 5.0f, CUI::CORNER_TL);
	Graphics()->QuadsEnd();

	// draw the text
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Spectate"), m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW ?
		m_pClient->m_aClients[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID].m_aName : Localize("Free-View"));
	TextRender()->Text(0, m_Width-174.0f, m_Height-13.0f, 8.0f, aBuf, -1);
}

void CHud::RenderSpeedmeter()
{
	if(!g_Config.m_ClRenderSpeedmeter)
		return;
		
	// We calculate the speed instead of getting it from character.velocity cause it's buggy when
	// walking in front of a wall or when using the ninja sword
	static float Speed;
	static vec2 OldPos;
	static const int SMOOTH_TABLE_SIZE = 16;
	static const int ACCEL_THRESHOLD = 25;
	static float SmoothTable[SMOOTH_TABLE_SIZE];
	static int SmoothIndex = 0;

	SmoothTable[SmoothIndex] = distance(m_pClient->m_LocalCharacterPos, OldPos)/Client()->RenderFrameTime();
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		float Mult = DemoPlayer()->BaseInfo()->m_Speed;
		SmoothTable[SmoothIndex] /= Mult;
	}
	SmoothIndex = (SmoothIndex + 1) % SMOOTH_TABLE_SIZE;
	OldPos = m_pClient->m_LocalCharacterPos;
	Speed = 0;
	for(int i = 0; i < SMOOTH_TABLE_SIZE; i++)
		Speed += SmoothTable[i];
	Speed /= SMOOTH_TABLE_SIZE;

	int GameFlags = m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_FLAGS;
	int LastIndex = SmoothIndex - 1;
	if(LastIndex < 0)
		LastIndex = SMOOTH_TABLE_SIZE - 1;

	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	if(g_Config.m_ClSpeedmeterAccel && Speed - SmoothTable[LastIndex] > ACCEL_THRESHOLD)
		Graphics()->SetColor(0.6f, 0.1f, 0.1f, 0.25f);
	else if(g_Config.m_ClSpeedmeterAccel && Speed - SmoothTable[LastIndex] < -ACCEL_THRESHOLD)
		Graphics()->SetColor(0.1f, 0.6f, 0.1f, 0.25f);
	else
		Graphics()->SetColor(0.1, 0.1, 0.1, 0.25);
	
	if(GameFlags&GAMEFLAG_TEAMS)
	{
		char aScoreTeam[2][32];
		str_format(aScoreTeam[TEAM_RED], sizeof(aScoreTeam)/2, "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed);
		str_format(aScoreTeam[TEAM_BLUE], sizeof(aScoreTeam)/2, "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue);
		float aScoreTeamWidth[2] = {TextRender()->TextWidth(0, 14.0f, aScoreTeam[TEAM_RED], -1), TextRender()->TextWidth(0, 14.0f, aScoreTeam[TEAM_BLUE], -1)};
		float ScoreWidthMax = max(max(aScoreTeamWidth[TEAM_RED], aScoreTeamWidth[TEAM_BLUE]), TextRender()->TextWidth(0, 14.0f, "100", -1));
		float Split = 3.0f;
		float ImageSize = GameFlags&GAMEFLAG_FLAGS ? 16.0f : Split;
		
		RenderTools()->DrawRoundRectExt(m_Width-ScoreWidthMax-ImageSize-2*Split, 225.0f, ScoreWidthMax+ImageSize+2*Split, 18.0f, 5.0f, CUI::CORNER_L);
	}
	else
	{
		const CNetObj_PlayerInfo *apPlayerInfo[2] = { 0, 0 };
		int i = 0;
		for(int j = 0; j < 2 && i < MAX_CLIENTS && m_pClient->m_Snap.m_paInfoByScore[i]; ++i)
		{
			if(m_pClient->m_Snap.m_paInfoByScore[i]->m_Team != TEAM_SPECTATORS)
			{
				apPlayerInfo[j] = m_pClient->m_Snap.m_paInfoByScore[i];
				++j;
			}
		}

		char aScore[2][32];
		for(int j = 0; j < 2; ++j)
		{
			if(apPlayerInfo[j])
				str_format(aScore[j], sizeof(aScore)/2, "%d", apPlayerInfo[j]->m_Score);
			else
				aScore[j][0] = 0;
		}
		float aScoreWidth[2] = {TextRender()->TextWidth(0, 14.0f, aScore[0], -1), TextRender()->TextWidth(0, 14.0f, aScore[1], -1)};
		float ScoreWidthMax = max(max(aScoreWidth[0], aScoreWidth[1]), TextRender()->TextWidth(0, 14.0f, "10", -1));
		
		RenderTools()->DrawRoundRectExt(m_Width-ScoreWidthMax-38.0f, 225.0f, ScoreWidthMax+38.0f, 18.0f, 5.0f, CUI::CORNER_L);
	}
 	Graphics()->QuadsEnd();

	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%.0f", Speed/10);
	TextRender()->Text(0, m_Width-5-TextRender()->TextWidth(0,12,aBuf,-1), 226, 12, aBuf, -1);
}

void CHud::RenderRaceTime()
{
	if(g_Config.m_ClRaceReplaceGametime)
		return;

	char aBuf[32];
	int RaceTime = (Client()->GameTick()+m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)*10/Client()->GameTickSpeed();
	bool RaceFlag = m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_RACETIME;

	if(RaceFlag)
		str_format(aBuf, sizeof(aBuf), "%02d:%02d.%d", RaceTime/(60*10), (RaceTime/10)%60, RaceTime%10);
	else if(m_RaceTime)
		str_format(aBuf, sizeof(aBuf), "%02d:%02d", m_RaceTime/60, m_RaceTime%60);
	else
		return;

	float Half = 300.0f*Graphics()->ScreenAspect()/2.0f;
	float w = TextRender()->TextWidth(0, 12, aBuf, -1);
	TextRender()->Text(0, Half-w/2, 20, 12, aBuf, -1);
}

void CHud::RenderCheckpoint()
{
	int TicksSinceCheckpoint = Client()->GameTick() - m_CheckpointTick;
	if(m_CheckpointTick != -1 && TicksSinceCheckpoint < Client()->GameTickSpeed()*4)
	{
		char aBuf[64];
		IRace::FormatTimeDiff(aBuf, sizeof(aBuf), m_CheckpointDiff, false);
			
		// calculate alpha (2 sec 1 than get lower the next 2 sec)
		float a = 1.0f;
		if(TicksSinceCheckpoint > Client()->GameTickSpeed()*2)
		{
			// lower the alpha slowly to blend text out
			a = (Client()->GameTickSpeed()*4 - TicksSinceCheckpoint) / (float)(Client()->GameTickSpeed()*2);
		}
			
		if(m_CheckpointDiff > 0)
			TextRender()->TextColor(1.0f,0.5f,0.5f,a); // red
		else if(m_CheckpointDiff < 0)
			TextRender()->TextColor(0.5f,1.0f,0.5f,a); // green
		else if(!m_CheckpointDiff)
			TextRender()->TextColor(1,1,1,a); // white

		float Half = 300.0f*Graphics()->ScreenAspect()/2.0f;
		float w = TextRender()->TextWidth(0, 10, aBuf, -1);
		TextRender()->Text(0, Half-w/2, 33, 10, aBuf, -1);

		TextRender()->TextColor(1,1,1,1);
	}
}

void CHud::RenderRecord()
{
	// TODO: fix this
	if(m_ServerRecord && g_Config.m_ClShowServerRecord)
	{
		char aBuf[64];
		char aTime[32];
		IRace::FormatTimeShort(aTime, sizeof(aTime), m_ServerRecord, true);
		str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Server best"), aTime);
		TextRender()->Text(0, 5, 40, 6, aBuf, -1);
	}
}

void CHud::OnRender()
{
	if(!m_pClient->m_Snap.m_pGameInfoObj)
		return;

	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);
	bool Race = IsRace(&ServerInfo);

	m_Width = 300.0f*Graphics()->ScreenAspect();
	m_Height = 300.0f;
	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);

	if(g_Config.m_ClShowhud)
	{
		if(m_pClient->m_Snap.m_pLocalCharacter && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
		{
			RenderHealthAndAmmo(m_pClient->m_Snap.m_pLocalCharacter);
			RenderSpeedmeter();
			RenderRaceTime();
			if(Race && g_Config.m_ClShowCheckpointDiff) RenderCheckpoint();
		}
		else if(m_pClient->m_Snap.m_SpecInfo.m_Active)
		{
			if(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
				RenderHealthAndAmmo(&m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID].m_Cur);
			RenderSpectatorHud();
		}

		RenderGameTimer();
		RenderPauseNotification();
		RenderSuddenDeath();
		if(!Race) RenderScoreHud();
		RenderWarmupTimer();
		RenderFps();
		if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
			RenderConnectionWarning();
		if(!Race) RenderTeambalanceWarning();
		RenderVoting();
		if(Race) RenderRecord();
	}
	RenderCursor();
}

void CHud::OnMessage(int MsgType, void *pRawMsg)
{
	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);

	if(MsgType == NETMSGTYPE_SV_RACETIME)
	{
		CNetMsg_Sv_RaceTime *pMsg = (CNetMsg_Sv_RaceTime *)pRawMsg;
		if(!IsDDNet(&ServerInfo))
			m_RaceTime = pMsg->m_Time;
		
		if(pMsg->m_Check)
		{
			m_CheckpointDiff = pMsg->m_Check * 10;
			m_CheckpointTick = Client()->GameTick();
		}
	}
	else if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		if(pMsg->m_Victim == m_pClient->m_Snap.m_LocalClientID)
		{
			m_CheckpointTick = -1;
			m_RaceTime = 0;
		}
	}
	else if(MsgType == NETMSGTYPE_SV_RECORD)
	{
		CNetMsg_Sv_Record *pMsg = (CNetMsg_Sv_Record *)pRawMsg;
		if(IsDDNet(&ServerInfo)) pMsg->m_Time *= 10;
		m_ServerRecord = pMsg->m_Time;
	}
}
