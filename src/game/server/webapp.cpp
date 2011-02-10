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
: m_pGameServer(pGameServer),
  m_pServer(pGameServer->Server())
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
	
	//std::cout << "---recv start---\n" << Data << "\n---recv end---\n" << std::endl;
	
	// TODO: check the header
	int Start = Data.find("\r\n\r\n");
	Data = Data.substr(Start+4);
	
	return Data;
}

bool CWebapp::PingServer()
{
	// connect to the server
	if(!Connect())
		return false;
	
	// TODO: use /api/1/ping/
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "GET /api/1/hello/ HTTP/1.1\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\n\r\n", g_Config.m_SvApiKey);
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
	str_format(aBuf, sizeof(aBuf), "GET /api/1/maps/list/ HTTP/1.1\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\n\r\n", g_Config.m_SvApiKey);
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

bool CWebapp::PostRun(int ClientID, float Time, float *pCpTime)
{
	if(!Connect())
		return false;
	
	char aBuf[1024];
	Json::Value Run;
	Json::FastWriter Writer;
	
	Run["map_name"] = g_Config.m_SvMap;
	Run["user_id"] = GameServer()->m_apPlayers[ClientID]->m_UserID;
	Run["nickname"] = Server()->ClientName(ClientID);
	// TODO
	str_format(aBuf, sizeof(aBuf), "%.3f", Time); // damn ugly but the only way i know to do it
	double TimeToSend;
	sscanf(aBuf, "%lf", &TimeToSend);
	Run["time"] = TimeToSend;
	// TODO: not really nice
	str_format(aBuf, sizeof(aBuf), "%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f",
		pCpTime[0], pCpTime[1], pCpTime[2], pCpTime[3], pCpTime[4], pCpTime[5], pCpTime[6], pCpTime[7], pCpTime[8], pCpTime[9],
		pCpTime[10], pCpTime[11], pCpTime[12], pCpTime[13], pCpTime[14], pCpTime[15], pCpTime[16], pCpTime[17], pCpTime[18], pCpTime[19],
		pCpTime[20], pCpTime[21], pCpTime[22], pCpTime[23], pCpTime[24], pCpTime[25]);
	Run["checkpoints"] = aBuf;
	
	std::string Json = Writer.write(Run);
	//std::cout << "---json start---\n" << Json << "\n---json end---\n" << std::endl;
	
	str_format(aBuf, sizeof(aBuf), "POST /api/1/runs/new/ HTTP/1.1\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", g_Config.m_SvApiKey, Json.length(), Json.c_str());
	std::string Received = SendAndReceive(aBuf);
	Disconnect();
	
	// TODO check the status code
	return true;
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

int CWebapp::UserAuth(const char *pUsername, const char *pPassword)
{
	if(!Connect())
		return -1;
	
	AutoSeededRandomPool rng;
	std::string cipher, cipher64;
	
	// RSA
	try 
	{
		FileSource pubFile("public_key.pem", true, new PEMFilter(new Base64Decoder()));
		RSAES_OAEP_SHA_Encryptor pub(pubFile);
		
		StringSource(pPassword, true,
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
		return -1;
	}
	
	Json::Value Userdata;
	Json::FastWriter Writer;
	
	Userdata["username"] = pUsername;
	Userdata["password"] = cipher64;
	
	std::string Json = Writer.write(Userdata);
	
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "POST /api/1/users/auth/ HTTP/1.1\r\nAPI_AUTH: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", g_Config.m_SvApiKey, Json.length(), Json.c_str());
	std::string Received = SendAndReceive(aBuf);
	Disconnect();
	
	//std::cout << "Recv:\n" << Received << std::endl;
	
	// TODO: better solution?
	if(!Received.compare("false"))
		return -1;
	
	Json::Value User;
	Json::Reader Reader;
	bool ParsingSuccessful = Reader.parse(Received, User);
	if(!ParsingSuccessful)
		return -1;
	
	return User["id"].asInt();
}