#if defined(CONF_TEERACE)

// TODO: replace crypto++ with another lib?
#include <engine/storage.h>
#include <game/server/webapp.h>
#include <engine/external/json/writer.h>

#include "upload.h"

int CWebUpload::UploadDemo(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	int UserID = pData->m_UserID;
	char aFilename[256];
	str_copy(aFilename, pData->m_aFilename, sizeof(aFilename));
	delete pData;
	
	if(!pWebapp->Connect())
		return 0;
	
	char aHeader[512];
	char aURL[128];
	str_format(aURL, sizeof(aURL), "/api/1/demos/update/%d/%d/", UserID, pWebapp->CurrentMap()->m_ID);
	
	// load file
	IOHANDLE File = pWebapp->Storage()->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		// get file length
		int FileLength = (int)io_length(File);
		
		// send header
		str_format(aHeader, sizeof(aHeader), CWebapp::UPLOAD, aURL, pWebapp->ServerIP(), pWebapp->ApiKey(), FileLength+142, "demo_file"); // 142 = stuff around data
		if(pWebapp->SendUploadHeader(aHeader) < 0)
		{
			dbg_msg("webapp", "demo upload failed (sending header)");
			io_close(File);
			return 0;
		}

		unsigned char aData[512];
		while(1)
		{
			unsigned Bytes = io_read(File, aData, 512);
			if(Bytes <= 0)
				break;
			if(pWebapp->Upload(aData, Bytes) < 0)
			{
				dbg_msg("webapp", "demo upload failed (sending data)");
				io_close(File);
				return 0;
			}
		}
		
		io_close(File);
		
		if(pWebapp->SendUploadEnd() < 0)
		{
			dbg_msg("webapp", "demo upload failed (sending end)");
			return 0;
		}
		
		// delete the demo file
		pWebapp->Storage()->RemoveFile(aFilename, IStorage::TYPE_SAVE);
	}
	else
		dbg_msg("webapp", "failed to open file %s", aFilename);
	
	pWebapp->Disconnect();
	
	return 0;
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
