/* Webapp Class by Sushi and Redix */
#ifndef GAME_WEBAPP_H
#define GAME_WEBAPP_H

#include <base/tl/array.h>

class IWebapp
{
	NETADDR m_Addr;
	array<class CHttpConnection*> m_Connections;
	class IStorage *m_pStorage;

public:
	IWebapp(IStorage *pStorage);
	virtual ~IWebapp() { m_Connections.delete_all(); };

	IStorage *Storage() { return m_pStorage; }

	const char *ServerIP();
	const char *ApiPath();
	
	int Update();
	virtual bool SendRequest(const char *pInString, int Type, class IStream *pResponse, class CWebData *pUserData = 0, IOHANDLE File = 0, const char *pFilename = 0, int64 StartTime = -1); // TODO: rework the creating of requests
	
	virtual void OnResponse(class CHttpConnection *pCon) = 0;
};

#endif
