#if defined(CONF_SQL)

#include <base/system.h>

#include "mysql.h"

bool CSqlConnection::Connect(const CSqlConfig *pConfig, bool SetDatabase)
{
	if(m_pCon != NULL)
		return false;

	m_pCon = mysql_init(NULL);
	if(m_pCon == NULL)
	{
		dbg_msg("SQL", mysql_error(m_pCon));
		return false;
	}

	if(mysql_real_connect(m_pCon, pConfig->m_aIp, pConfig->m_aUser, pConfig->m_aPass,
		SetDatabase ? pConfig->m_aDatabase : NULL, pConfig->m_Port, NULL, 0) == NULL)
	{
		dbg_msg("SQL", mysql_error(m_pCon));
		mysql_close(m_pCon);
		m_pCon = NULL;
		return false;
	}

	m_Connected = true;
	return true;
}

void CSqlConnection::Disconnect()
{
	if(m_pCon == NULL)
	{
		dbg_msg("SQL", "ERROR: No SQL connection");
		return;
	}

	mysql_close(m_pCon);
	m_pCon = NULL;
	m_Connected = false;
}

bool CSqlConnection::Query(const char *pStr)
{
	if(!m_Connected)
		return false;
	if(mysql_query(m_pCon, pStr))
	{
		mysql_error(m_pCon);
		return false;
	}
	return true;
}

CSqlResultSet *CSqlConnection::StoreResult()
{
	if(m_pCon == NULL)
		return 0;
	return new CSqlResultSet(this);
}

CSqlResultSet *CSqlConnection::QueryWithResult(const char *pStr)
{
	if(!Query(pStr))
		return 0;
	return StoreResult();
}

CSqlResultSet::CSqlResultSet(CSqlConnection *pCon) : m_ppRow(NULL), m_NumFields(0)
{
	m_pResult = mysql_store_result(pCon->m_pCon);
	if(m_pResult != NULL)
	{
		m_NumFields = mysql_num_fields(m_pResult);
		MYSQL_FIELD *pFields = mysql_fetch_fields(m_pResult);

		for(int i = 0; i < m_NumFields; i++)
		{
			CField Field;
			Field.m_Name = pFields[i].name;
			Field.m_Index = i;
			m_lFields.add(Field);
		}
	}
}

CSqlResultSet::~CSqlResultSet()
{
	if(m_pResult != NULL)
		mysql_free_result(m_pResult);
}

bool CSqlResultSet::Next()
{
	if(m_pResult == NULL)
		return false;
	m_ppRow = mysql_fetch_row(m_pResult);
	return m_ppRow != NULL;
}

void CSqlResultSet::SetRowIndex(unsigned Index)
{
	if(m_pResult == NULL)
		mysql_data_seek(m_pResult, Index);
}

int CSqlResultSet::GetColumnIndex(const char *pName) const
{
	for(int i = 0; i < m_NumFields; i++)
		if(str_comp(m_lFields[i].m_Name, pName) == 0)
			return i;
	return -1;
}

const char *CSqlResultSet::GetString(int Index) const
{
	if(m_ppRow && Index >= 0 && Index < m_NumFields)
		return m_ppRow[Index];
	return NULL;
}

int CSqlResultSet::GetInteger(int Index) const
{
	if(m_ppRow && Index >= 0 && Index < m_NumFields)
		return str_toint(m_ppRow[Index]);
	return 0;
}

#endif
