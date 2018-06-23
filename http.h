#ifndef __HTTP_H
#define __HTTP_H
#include "tier1.h"
#include "utlstring.h"
#include "utlvector.h"
#include "threadtools.h"
#include "platform.h"
#include "lcurl.h"

class CHTTPAsyncQuery
{
public:
	enum {
		HTTP_GET,
		HTTP_POST,
		HTTP_POSTMULTIPART
	};
	int m_iType;
	
	CUtlString m_Url;
	CUtlString m_Args;

	CUtlString m_UserAgent;
	CUtlString m_Proxy;

	typedef struct {
		CUtlString m_Name;
		size_t m_uSize;
		void* m_pData;
	} formdata_t;

	typedef struct {
		CUtlString m_Name;
		CUtlString m_FileName;
		size_t m_uSize;
		void* m_pData;
	} filedata_t;

	CUtlVector<formdata_t> m_PostForms;
	CUtlVector<filedata_t> m_PostFiles;

	CMemPool m_Mem;
	int m_iCurlCode;
	int m_iHttpCode;

	int m_iLuaRefOk;
	int m_iLuaRefFail;
};

void AddQuery(CHTTPAsyncQuery*);
void FinishQuery(CHTTPAsyncQuery*);

#endif