/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H

#include "kernel.h"
#include "message.h"
#include <engine/shared/ghost.h>

class IServer : public IInterface
{
	MACRO_INTERFACE("server", 0)
protected:
	int m_CurrentGameTick;
	int m_TickSpeed;

public:
	/*
		Structure: CClientInfo
	*/
	struct CClientInfo
	{
		const char *m_pName;
		int m_Latency;
	};

	int Tick() const { return m_CurrentGameTick; }
	int TickSpeed() const { return m_TickSpeed; }

	virtual int MaxClients() const = 0;

	virtual class IStorage *Storage() = 0;

	virtual const char *ClientName(int ClientID) = 0;
	virtual const char *ClientClan(int ClientID) = 0;
	virtual int ClientCountry(int ClientID) = 0;
	virtual bool ClientIngame(int ClientID) = 0;
	virtual int GetClientInfo(int ClientID, CClientInfo *pInfo) = 0;
	virtual void GetClientAddr(int ClientID, char *pAddrStr, int Size) = 0;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID) = 0;

	virtual void SendHttp(class CRequestInfo *pInfo, class IRequest *pRequest) = 0;

	template<class T>
	int SendPackMsg(T *pMsg, int Flags, int ClientID)
	{
		CMsgPacker Packer(pMsg->MsgID());
		if(pMsg->Pack(&Packer))
			return -1;
		return SendMsg(&Packer, Flags, ClientID);
	}

	virtual void SetClientName(int ClientID, char const *pName) = 0;
	virtual void SetClientClan(int ClientID, char const *pClan) = 0;
	virtual void SetClientCountry(int ClientID, int Country) = 0;
	virtual void SetClientScore(int ClientID, int Score) = 0;

	virtual int SnapNewID() = 0;
	virtual void SnapFreeID(int ID) = 0;
	virtual void *SnapNewItem(int Type, int ID, int Size) = 0;

	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	enum
	{
		RCON_CID_SERV=-1,
		RCON_CID_VOTE=-2,
	};
	virtual void SetRconCID(int ClientID) = 0;
	virtual bool IsAuthed(int ClientID) = 0;
	virtual void Kick(int ClientID, const char *pReason) = 0;

	virtual void DemoRecorder_HandleAutoStart() = 0;
	virtual bool DemoRecorder_IsRecording() = 0;

#if defined(CONF_TEERACE)
	virtual const char* GetConfigFilename() = 0;
	virtual void ReloadMap() = 0;

	virtual void SetUserID(int ClientID, int UserID) = 0;
	virtual int GetUserID(int ClientID) = 0;
	virtual int GetPlayTicks(int ClientID) = 0;
	virtual void HigherPlayTicks(int ClientID) = 0;

	virtual void SetUserName(int ClientID, const char* pUsername) = 0;
	virtual const char* GetUserName(int ClientID) = 0;

	virtual char *GetMapName() = 0;

	virtual void SaveGhostDemo(int ClientID) = 0;

	virtual void StartRecord(int ClientID) = 0;
	virtual void StopRecord(int ClientID) = 0;
	virtual bool IsRecording(int ClientID) = 0;

	virtual void StaffAuth(int ClientID, int SendRconCmds) = 0;

	virtual void StartGhostRecord(int ClientID, const char* pSkinName, int UseCustomColor, int ColorBody, int ColorFeet) = 0;
	virtual void StopGhostRecord(int ClientID, float Time=0.0f) = 0;
	virtual bool IsGhostRecording(int ClientID) = 0;
	virtual void GhostAddInfo(int ClientID, IGhostRecorder::CGhostCharacter *pPlayer) = 0;
#endif
};

class IGameServer : public IInterface
{
	MACRO_INTERFACE("gameserver", 0)
protected:
public:
	virtual void OnInit() = 0;
	virtual void OnConsoleInit() = 0;
	virtual void OnShutdown() = 0;

	virtual void OnTick() = 0;
	virtual void OnPreSnap() = 0;
	virtual void OnSnap(int ClientID) = 0;
	virtual void OnPostSnap() = 0;

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) = 0;

	virtual void OnClientConnected(int ClientID) = 0;
	virtual void OnClientEnter(int ClientID) = 0;
	virtual void OnClientDrop(int ClientID, const char *pReason) = 0;
	virtual void OnClientDirectInput(int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientID, void *pInput) = 0;

	virtual bool IsClientReady(int ClientID) = 0;
	virtual bool IsClientPlayer(int ClientID) = 0;

#if defined(CONF_TEERACE)
	virtual void OnTeeraceAuth(int ClientID, const char *pStr, int SendRconCmds) = 0;
#endif

	virtual const char *GameType() = 0;
	virtual const char *Version() = 0;
	virtual const char *NetVersion() = 0;
};

extern IGameServer *CreateGameServer();
#endif
