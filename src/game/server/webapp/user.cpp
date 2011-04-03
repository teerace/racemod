#if defined(CONF_TEERACE)

// TODO: replace crypto++ with another lib?
#include <game/server/webapp.h>
/*#include <engine/external/encrypt/cryptlib.h>
#include <engine/external/encrypt/osrng.h>
#include <engine/external/encrypt/files.h>
#include <engine/external/encrypt/base64.h>
#include <engine/external/encrypt/fltrimpl.h>
#include <engine/external/encrypt/rsa.h>*/
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "user.h"

// not needed at the moment
/*using namespace CryptoPP;

class PEMFilter : public Unflushable<Filter>
{
public:
	PEMFilter(BufferedTransformation *attachment = NULL) : m_Count(0)
	{
		Detach(attachment);
	}
	
	size_t Put2(const byte *begin, size_t length, int messageEnd, bool blocking)
	{
		FILTER_BEGIN;
		while(m_inputPosition < length)
		{
			if(begin[m_inputPosition++] == '-')
				m_Count++;
			if(m_Count)
			{
				if(m_Count == 10)
					m_Count = 0;
				continue;
			}
			else
				FILTER_OUTPUT(1,begin+m_inputPosition-1,1,0);
		}
		if(messageEnd)
			FILTER_OUTPUT(2,0,0,messageEnd);
		FILTER_END_NO_MESSAGE_END;
	}
	
private:
	int m_Count;
};

int CWebUser::Auth(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	int ClientID = pData->m_ClientID;
	
	if(!pWebapp->Connect())
	{
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		delete pData;
		return 0;
	}
	
	AutoSeededRandomPool rng;
	std::string cipher, cipher64;
	
	try
	{
		// RSA
		FileSource pubFile("public_key.pem", true, new PEMFilter(new Base64Decoder()));
		RSAES_OAEP_SHA_Encryptor pub(pubFile);
		
		StringSource(pData->m_aPassword, true,
			new PK_EncryptorFilter(rng, pub,
				new StringSink(cipher)
		   )
		);
		
		// Base64
		StringSource(cipher, true,
			new Base64Encoder(
				new StringSink(cipher64),
			false)
		);
	}
	catch(Exception const& e)
	{
		dbg_msg("CryptoPP", "error: %s", e.what());
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		delete pData;
		return 0;
	}
	
	Json::Value Userdata;
	Json::FastWriter Writer;
	
	Userdata["username"] = pData->m_aUsername;
	Userdata["password"] = cipher64;
	
	std::string Json = Writer.write(Userdata);
	delete pData;
	
	char *pReceived = 0;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), CWebapp::POST, "/api/1/users/auth/", pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	int Size = pWebapp->SendAndReceive(aBuf, &pReceived);
	pWebapp->Disconnect();
	
	if(Size < 0)
	{
		dbg_msg("webapp", "error: %d (user auth)", Size);
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		return 0;
	}
	
	if(str_comp(pReceived, "false") == 0)
	{
		mem_free(pReceived);
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		return 0;
	}
	
	Json::Value User;
	Json::Reader Reader;
	bool ParsingSuccessful = Reader.parse(pReceived, pReceived+Size, User);
	mem_free(pReceived);
	
	COut *pOut = new COut(WEB_USER_AUTH, ClientID);
	if(ParsingSuccessful)
	{
		str_copy(pOut->m_aUsername, User["username"].asCString(), sizeof(pOut->m_aUsername));
		pOut->m_UserID = User["id"].asInt();
	}
	pWebapp->AddOutput(pOut);
	return ParsingSuccessful;
}*/

int CWebUser::AuthToken(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	int ClientID = pData->m_ClientID;
	
	if(!pWebapp->Connect())
	{
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		delete pData;
		return 0;
	}
	
	Json::Value Userdata;
	Json::FastWriter Writer;
	
	Userdata["api_token"] = pData->m_aToken;
	
	std::string Json = Writer.write(Userdata);
	delete pData;
	
	char *pReceived = 0;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), CWebapp::POST, "/api/1/users/auth_token/", pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	int Size = pWebapp->SendAndReceive(aBuf, &pReceived);
	pWebapp->Disconnect();
	
	if(Size < 0)
	{
		dbg_msg("webapp", "error: %d (user auth token)", Size);
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		return 0;
	}
	
	if(str_comp(pReceived, "false") == 0)
	{
		mem_free(pReceived);
		pWebapp->AddOutput(new COut(WEB_USER_AUTH, ClientID));
		return 0;
	}
	
	Json::Value User;
	Json::Reader Reader;
	bool ParsingSuccessful = Reader.parse(pReceived, pReceived+Size, User);
	mem_free(pReceived);
	
	COut *pOut = new COut(WEB_USER_AUTH, ClientID);
	if(ParsingSuccessful)
	{
		pOut->m_UserID = User["id"].asInt();
		str_copy(pOut->m_aUsername, User["username"].asCString(), sizeof(pOut->m_aUsername));
	}
	pWebapp->AddOutput(pOut);
	return ParsingSuccessful;
}

// TODO: rework this
float HueToRgb(float v1, float v2, float h)
{
   if(h < 0.0f) h += 1;
   if(h > 1.0f) h -= 1;
   if((6.0f * h) < 1.0f) return v1 + (v2 - v1) * 6.0f * h;
   if((2.0f * h) < 1.0f) return v2;
   if((3.0f * h) < 2.0f) return v1 + (v2 - v1) * ((2.0f/3.0f) - h) * 6.0f;
   return v1;
}

int HslToRgb(int v)
{
	vec3 HSL = vec3(((v>>16)&0xff)/255.0f, ((v>>8)&0xff)/255.0f, 0.5f+(v&0xff)/255.0f*0.5f);
	vec3 RGB;
	if(HSL.s == 0.0f)
		RGB = vec3(HSL.l, HSL.l, HSL.l);
	else
	{
		float v2 = HSL.l < 0.5f ? HSL.l * (1.0f + HSL.s) : (HSL.l+HSL.s) - (HSL.s*HSL.l);
		float v1 = 2.0f * HSL.l - v2;

		RGB = vec3(HueToRgb(v1, v2, HSL.h + (1.0f/3.0f)), HueToRgb(v1, v2, HSL.h), HueToRgb(v1, v2, HSL.h - (1.0f/3.0f)));
	}
	
	RGB = RGB*255;
	return (((int)RGB.r)<<16)|(((int)RGB.g)<<8)|(int)RGB.b;
}

int CWebUser::UpdateSkin(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	
	if(!pWebapp->Connect())
	{
		delete pData;
		return 0;
	}
	
	Json::Value Userdata;
	Json::FastWriter Writer;
	
	Userdata["skin_name"] = pData->m_SkinName;
	Userdata["body_color"] = HslToRgb(pData->m_ColorBody);
	Userdata["feet_color"] = HslToRgb(pData->m_ColorFeet);
	
	std::string Json = Writer.write(Userdata);
	
	char *pReceived = 0;
	char aBuf[512];
	char aURL[128];
	str_format(aURL, sizeof(aURL), "/api/1/users/skin/%d/", pData->m_UserID);
	str_format(aBuf, sizeof(aBuf), CWebapp::PUT, aURL, pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	int Size = pWebapp->SendAndReceive(aBuf, &pReceived);
	pWebapp->Disconnect();
	mem_free(pReceived);
	
	delete pData;
	
	if(Size < 0)
		dbg_msg("webapp", "error: %d (skin update)", Size);
	
	return Size >= 0;
}

int CWebUser::GetRank(void *pUserData) // TODO: get clan here too
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	int UserID = pData->m_UserID;
	int ClientID = pData->m_ClientID;
	bool PrintRank = pData->m_PrintRank;
	bool GetBestRun = pData->m_GetBestRun;
	char aName[64];
	str_copy(aName, pData->m_aName, sizeof(aName));
	delete pData;
	
	int Size = 0;
	
	int GlobalRank = 0;
	int MapRank = 0;
	
	if(!pWebapp->Connect())
		return 0;
	
	char *pReceived = 0;
	char aBuf[512];
	char aURL[128];
	
	// get userid if there is none
	if(!UserID)
	{
		Json::Value PostUser;
		Json::FastWriter Writer;

		PostUser["username"] = aName;

		std::string Json = Writer.write(PostUser);
	
		str_format(aURL, sizeof(aURL), "/api/1/users/get_by_name/");
		str_format(aBuf, sizeof(aBuf), CWebapp::POST, aURL, pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
		Size = pWebapp->SendAndReceive(aBuf, &pReceived);
		pWebapp->Disconnect();
		
		if(Size < 0)
		{
			dbg_msg("webapp", "error: %d (user global rank)", Size);
			return 0;
		}
		
		Json::Value User;
		Json::Reader Reader;
		if(!Reader.parse(pReceived, pReceived+Size, User))
		{
			mem_free(pReceived);
			return 0;
		}
		
		mem_free(pReceived);
		
		UserID = User["id"].asInt();
		// no user found
		if(!UserID)
		{
			COut *pOut = new COut(WEB_USER_RANK, ClientID);
			pOut->m_PrintRank = PrintRank;
			pOut->m_MatchFound = 0;
			str_copy(pOut->m_aUsername, aName, sizeof(pOut->m_aUsername));
			pWebapp->AddOutput(pOut);
			return 1;
		}
		str_copy(aName, User["username"].asCString(), sizeof(aName));
		
		if(!pWebapp->Connect())
			return 0;
	}
	
	// global rank
	str_format(aURL, sizeof(aURL), "/api/1/users/rank/%d/", UserID);
	str_format(aBuf, sizeof(aBuf), CWebapp::GET, aURL, pWebapp->ServerIP(), pWebapp->ApiKey());
	Size = pWebapp->SendAndReceive(aBuf, &pReceived);
	pWebapp->Disconnect();
	
	if(Size < 0)
	{
		dbg_msg("webapp", "error: %d (user global rank)", Size);
		return 0;
	}
	
	GlobalRank = str_toint(pReceived);
	mem_free(pReceived);
	
	if(!pWebapp->Connect())
		return 0;
	
	str_format(aURL, sizeof(aURL), "/api/1/users/map_rank/%d/%d/", UserID, pWebapp->CurrentMap()->m_ID);
	str_format(aBuf, sizeof(aBuf), CWebapp::GET, aURL, pWebapp->ServerIP(), pWebapp->ApiKey());
	Size = pWebapp->SendAndReceive(aBuf, &pReceived);
	pWebapp->Disconnect();
	
	if(Size < 0)
	{
		dbg_msg("webapp", "error: %d (user map rank)", Size);
		return 0;
	}
	
	Json::Value Rank;
	Json::Reader Reader;
	if(!Reader.parse(pReceived, pReceived+Size, Rank))
	{
		mem_free(pReceived);
		return 0;
	}
	
	mem_free(pReceived);
		
	MapRank = Rank["position"].asInt();
	CPlayerData Run;
	if(MapRank)
	{
		// getting times
		if(!pWebapp->DefaultScoring())
		{
			float Time = str_tofloat(Rank["bestrun"]["time"].asCString());
			float aCheckpointTimes[25] = {0.0f};
			Json::Value Checkpoint = Rank["bestrun"]["checkpoints_list"];
			for(unsigned int i = 0; i < Checkpoint.size(); i++)
				aCheckpointTimes[i] = str_tofloat(Checkpoint[i].asCString());
			Run.Set(Time, aCheckpointTimes);
		}
	}
	
	COut *pOut = new COut(WEB_USER_RANK, ClientID);
	pOut->m_GlobalRank = GlobalRank;
	pOut->m_MapRank = MapRank;
	pOut->m_BestRun = Run;
	pOut->m_UserID = UserID;
	pOut->m_PrintRank = PrintRank;
	pOut->m_GetBestRun = GetBestRun;
	str_copy(pOut->m_aUsername, aName, sizeof(pOut->m_aUsername));
	pWebapp->AddOutput(pOut);
	return 1;
}

#endif
