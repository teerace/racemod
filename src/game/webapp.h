/* Webapp Class by Sushi and Redix */
#ifndef GAME_WEBAPP_H
#define GAME_WEBAPP_H

#include <base/tl/array.h>

class IWebapp
{
	NETADDR m_Addr;
	array<class CHttpConnection*> m_Connections;
	class IStorage *m_pStorage;

	virtual void RegisterFields(class CRequest *pRequest, bool Api) = 0;
	virtual void OnResponse(class CHttpConnection *pCon) = 0;
	
	bool Send(CRequest *pRequest, class CResponse *pResponse, int Type, CWebData *pUserData);

public:
	IWebapp(IStorage *pStorage);
	virtual ~IWebapp() { m_Connections.delete_all(); };

	CRequest *CreateRequest(const char *pURI, int Method, bool Api = true);
	bool SendRequest(CRequest *pRequest, int Type, class CWebData *pUserData = 0);
	bool Download(const char *pFilename, const char *pURI, int Type, class CWebData *pUserData = 0);
	bool Upload(const char *pFilename, const char *pURI, int Type, class CWebData *pUserData = 0);

	void Update();

	IStorage *Storage() { return m_pStorage; }
};

#endif
