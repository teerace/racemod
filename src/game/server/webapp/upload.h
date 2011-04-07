#ifndef GAME_SERVER_WEBAPP_UPLOAD_H
#define GAME_SERVER_WEBAPP_UPLOAD_H

#include "data.h"

class CWebUpload
{
public:
	
	class CParam : public IDataIn
	{
	public:
		int m_RunID;
		char m_aFilename[256];
	};
	
	static int UploadDemo(void *pUserData);
	static int UploadGhost(void *pUserData);
};

#endif
