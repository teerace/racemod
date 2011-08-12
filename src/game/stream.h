#ifndef GAME_STREAM_H
#define GAME_STREAM_H

#include <base/system.h>
#include <engine/storage.h>

// TODO: optimize this

class IStream
{
protected:
	int m_Size;

public:
	IStream() : m_Size(0) {}
	virtual ~IStream() {}

	virtual bool Write(char *pData, int Size) = 0;
	virtual char *GetData() { return ""; };
	virtual bool IsFile() { return false; }
	virtual void Clear() = 0;
	int Size() { return m_Size; }
};

class CFileStream : public IStream
{
	IOHANDLE m_File;
	IStorage *m_pStorage;
	char m_aFilename[512];

public:
	CFileStream(const char *pFilename, IStorage *pStorage) : m_pStorage(pStorage), m_File(0)
	{
		str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
		m_File = m_pStorage->OpenFile(m_aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	}
	~CFileStream() { Clear(); }

	bool IsFile() { return true; }
	const char *GetPath() { return m_aFilename; }
	const char *GetFilename()
	{
		char *pMapShort = m_aFilename;
		for(char *pMap = pMapShort; *pMap; pMap++)
		{
			if(*pMap == '/' || *pMap == '\\')
				pMapShort = pMap+1;
		}
		return pMapShort;
	}

	bool Write(char *pData, int Size)
	{
		if(!m_File)
			return false;
		io_write(m_File, pData, Size);
		m_Size += Size;
		return true;
	}

	void Clear()
	{
		if(m_File)
			io_close(m_File);
		m_File = 0;
		m_Size = 0;
	}

	void RemoveFile()
	{
		Clear();
		m_pStorage->RemoveFile(m_aFilename, IStorage::TYPE_SAVE);
	}
};

class CBufferStream : public IStream
{
	char *m_pData;

public:
	CBufferStream() : m_pData(0) {}
	~CBufferStream() { Clear(); }

	bool Write(char *pData, int Size)
	{
		if(Size > 0)
		{
			if(m_pData)
			{
				//m_pData = (char *)realloc(m_pData, m_Size + Size);
				// TODO: fixme
				char *pTmp = (char *)mem_alloc(m_Size + Size+1, 1);
				mem_copy(pTmp, m_pData, m_Size);
				mem_free(m_pData);
				m_pData = pTmp;
			}
			else
				m_pData = (char *)mem_alloc(Size+1, 1);
			mem_copy(m_pData+m_Size, pData, Size);
			m_Size += Size;
			m_pData[m_Size] = 0;
		}
		return true;
	}

	void Clear()
	{
		if(m_pData)
			mem_free(m_pData);
		m_pData = 0;
		m_Size = 0;
	}

	char *GetData() { return m_pData; }
};

#endif
