#if defined(CONF_TEERACE)

// TODO: replace crypto++ with another lib?
#include <game/server/webapp.h>
#include <engine/external/json/writer.h>

#include "upload.h"

int CWebUpload::UploadDemo(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	delete pData;
	
	// upload demo
	
	return 1;
}

int CWebUpload::UploadGhost(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	delete pData;
	
	// upload ghost
	
	return 1;
}

#endif
