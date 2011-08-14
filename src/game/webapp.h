/* Webapp Class by Sushi and Redix */
#ifndef GAME_WEBAPP_H
#define GAME_WEBAPP_H

#include <base/tl/array.h>

class IWebapp
{
	NETADDR m_Addr;
	array<class CHttpConnection*> m_Connections;

public:
	IWebapp(const char* WebappIp);
	virtual ~IWebapp() {};
	
	int Update();
	virtual bool SendRequest(const char *pInString, int Type, class IStream *pResponse, class CWebData *pUserData = 0);
	
	virtual void OnResponse(int Type, IStream *pData, class CWebData *pUserData, int StatusCode) = 0;
};

#endif
