// TODO: replace crypto++ with another lib?
#include <game/server/webapp.h>
#include <engine/external/encrypt/cryptlib.h>
#include <engine/external/encrypt/osrng.h>
#include <engine/external/encrypt/files.h>
#include <engine/external/encrypt/base64.h>
#include <engine/external/encrypt/fltrimpl.h>
#include <engine/external/encrypt/rsa.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "user.h"

using namespace CryptoPP;

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
		pWebapp->AddOutput(new COut(ClientID));
		delete pData;
		return 0;
	}
	
	AutoSeededRandomPool rng;
	std::string cipher, cipher64;
	
	// RSA
	try 
	{
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
		pWebapp->AddOutput(new COut(ClientID));
		delete pData;
		return 0;
	}
	
	Json::Value Userdata;
	Json::FastWriter Writer;
	
	Userdata["username"] = pData->m_aUsername;
	Userdata["password"] = cipher64;
	
	std::string Json = Writer.write(Userdata);
	delete pData;
	
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "POST /api/1/users/auth/ HTTP/1.1\r\nHost: %s\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
		pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	std::string Received = pWebapp->SendAndReceive(aBuf);
	pWebapp->Disconnect();
	
	if(!Received.compare("false"))
	{
		pWebapp->AddOutput(new COut(ClientID));
		return 0;
	}
	
	Json::Value User;
	Json::Reader Reader;
	bool ParsingSuccessful = Reader.parse(Received, User);
	
	COut *pOut = new COut(ClientID);
	if(ParsingSuccessful)
		pOut->m_UserID = User["id"].asInt();
	pWebapp->AddOutput(pOut);
	return ParsingSuccessful;
}

int CWebUser::AuthToken(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	int ClientID = pData->m_ClientID;
	
	if(!pWebapp->Connect())
	{
		pWebapp->AddOutput(new COut(ClientID));
		delete pData;
		return 0;
	}
	
	Json::Value Userdata;
	Json::FastWriter Writer;
	
	Userdata["api_token"] = pData->m_aToken;
	
	std::string Json = Writer.write(Userdata);
	delete pData;
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "POST /api/1/users/auth_token/ HTTP/1.1\r\nHost: %s\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
		pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	std::string Received = pWebapp->SendAndReceive(aBuf);
	pWebapp->Disconnect();
	
	if(!Received.compare("false"))
	{
		pWebapp->AddOutput(new COut(ClientID));
		return 0;
	}
	
	Json::Value User;
	Json::Reader Reader;
	bool ParsingSuccessful = Reader.parse(Received, User);
	
	COut *pOut = new COut(ClientID);
	if(ParsingSuccessful)
		pOut->m_UserID = User["id"].asInt();
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
	delete pData;
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "PUT /api/1/users/skin/%d/ HTTP/1.1\r\nHost: %s\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
		pData->m_UserID, pWebapp->ServerIP(), pWebapp->ApiKey(), Json.length(), Json.c_str());
	std::string Received = pWebapp->SendAndReceive(aBuf);
	pWebapp->Disconnect();

	// TODO check the status code
	return 1;
}
