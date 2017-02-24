#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H

#include <base/tl/array.h>
#include <base/tl/string.h>

#include <mysql.h>

// minimal C++ wrapper for mysql lib
// TODO: generalize this (interface for mysql, sqlite, ...)

struct CSqlConfig
{
	char m_aDatabase[32];
	char m_aUser[32];
	char m_aPass[32];
	char m_aIp[32];
	int m_Port;
};

class CSqlConnection
{
	friend class CSqlResultSet;

	MYSQL *m_pCon;
	bool m_Connected;

	CSqlConnection(const CSqlConnection& other);
	CSqlConnection &operator =(const CSqlConnection& other);

public:
	CSqlConnection() : m_pCon(NULL), m_Connected(false) { }
	~CSqlConnection() { if(m_pCon) Disconnect(); }

	bool Connect(const CSqlConfig *pConfig, bool SetDatabase = true);
	void Disconnect();

	bool Query(const char *pStr);
	CSqlResultSet *QueryWithResult(const char *pStr);
	CSqlResultSet *StoreResult();
};

class CSqlResultSet
{
	struct CField
	{
		string m_Name;
		int m_Index;
	};

	MYSQL_RES *m_pResult;
	MYSQL_ROW m_ppRow;
	array<CField> m_lFields;
	int m_NumFields;

	CSqlResultSet(const CSqlResultSet& other);
	CSqlResultSet &operator =(const CSqlResultSet& other);

public:
	CSqlResultSet(CSqlConnection *pCon);
	~CSqlResultSet();

	bool Next();

	void SetRowIndex(unsigned Index);
	int GetColumnIndex(const char *pName) const;

	const char *GetString(int Index) const;
	int GetInteger(int Index) const;

	const char *GetString(const char *pName) const { return GetString(GetColumnIndex(pName)); }
	int GetInteger(const char *pName) const { return GetInteger(GetColumnIndex(pName)); }
};

#endif
