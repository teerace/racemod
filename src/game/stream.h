#ifndef GAME_STREAM_H
#define GAME_STREAM_H

#include <base/system.h>
#include <stdlib.h>

// TODO: optimize this

class IStream
{
protected:
	int m_Size;

public:
	IStream() : m_Size(0) {}
	virtual ~IStream() {}

	virtual bool Write(char *pData, int Size) = 0;
	int Size() { return m_Size; }
};

class CFileStream : public IStream
{
	IOHANDLE m_File;

public:
	CFileStream(IOHANDLE File) : m_File(File) {}
	~CFileStream()
	{
		if(m_File)
			io_close(m_File);
	}

	bool Write(char *pData, int Size)
	{
		if(!m_File)
			return false;
		io_write(m_File, pData, Size);
		m_Size += Size;
		return true;
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
