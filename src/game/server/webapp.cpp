/* CWebapp class by Sushi */
#include <iostream>
#include <base/tl/array.h>
// TODO: replace crypto++ with another lib?
#include <engine/external/encrypt/cryptlib.h>
#include <engine/external/encrypt/osrng.h>
#include <engine/external/encrypt/files.h>
#include <engine/external/encrypt/base64.h>
#include <engine/external/encrypt/fltrimpl.h>
#include <engine/external/encrypt/rsa.h>
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>
#include <engine/shared/config.h>

#include "gamecontext.h"
#include "webapp.h"

using namespace CryptoPP;

//static LOCK gs_WebappLock = 0;

CWebapp::CWebapp(CGameContext *pGameServer)
: m_pGameServer(pGameServer)
{
	net_addr_from_str(&m_Addr, g_Config.m_SvWebappIp);
	m_lMapList.clear();
}

CWebapp::~CWebapp()
{
	Disconnect();
	m_lMapList.clear();
}

bool CWebapp::Connect()
{
	// connect to the server
	m_Socket = net_tcp_create(&m_Addr);
	if(m_Socket == NETSOCKET_INVALID)
		return false;
	
	return true;
}

void CWebapp::Disconnect()
{
	net_tcp_close(m_Socket);
}

std::string CWebapp::SendAndReceive(const char* pInString)
{
	// send the data
	net_tcp_connect(m_Socket, &m_Addr);
	//std::cout << pInString << std::endl;
	int DataSent = net_tcp_send(m_Socket, pInString, str_length(pInString));
	
	// receive the data
	int Received = 0;
	std::string Data = "";
	do
	{
		char aBuf[512] = {0};
		Received = net_tcp_recv(m_Socket, aBuf, 511);

		if(Received > 0)
			Data.append(aBuf);
	} while(Received > 0);
	
	std::cout << "---recv start---\n" << Data << "\n---recv end---\n" << std::endl;
	
	return Data;
}

bool CWebapp::PingServer()
{
	// connect to the server
	if(!Connect())
		return false;
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "GET /api/1/hello/\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\n\r\n", g_Config.m_SvApiKey);
	std::string Received = SendAndReceive(aBuf);
	Disconnect();
	
	//std::cout << "Recv: '" << Received << "'" << std::endl;
	
	if(!Received.compare("\"PONG\""))
		return true;
	
	return false;
}

void CWebapp::LoadMapList()
{
	// clear maplist
	m_lMapList.clear();
	
	if(!Connect())
		return;
		
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "GET /api/1/maps/list/\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\n\r\n", g_Config.m_SvApiKey);
	std::string Received = SendAndReceive(aBuf);
	Disconnect();
	
	// cutting out the maps fomr the Received data
	Json::Value Maplist;
	Json::Reader Reader;
	bool ParsingSuccessful = Reader.parse(Received, Maplist);
	if(!ParsingSuccessful)
		return;
	
	for(int i = 0; i < Maplist.size(); i++)
	{
		Json::Value Map = Maplist[i];
		m_lMapList.add(Map["name"].asString());
		dbg_msg("LoadedMap", "%s", m_lMapList[i].c_str());
	}
}

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

void CWebapp::UserAuth()
{
	if(!Connect())
		return;
	
	std::string username = "test";
	std::string password = "test";
	
	AutoSeededRandomPool rng;
	std::string cipher, cipher64;
	
	// RSA
	FileSource pubFile("public_key.pem", true, new PEMFilter(new Base64Decoder()));
    RSAES_OAEP_SHA_Encryptor pub(pubFile);
	
	StringSource(password, true,
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
	
	Json::Value Userdata;
	Json::FastWriter Writer;
	
	Userdata["username"] = username;
	Userdata["password"] = cipher64;
	
	std::string json = Writer.write(Userdata);
	
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "POST /api/1/users/auth/ HTTP/1.1\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", g_Config.m_SvApiKey, json.length(), json.c_str());
	std::string Received = SendAndReceive(aBuf);
	
	Disconnect();
	
	//std::cout << "Recv: '" << Received << "'" << std::endl;
}