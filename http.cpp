#include "threadtools.h"
#include "http.h"
#include "baselib.h"
#include "tier1.h"

//class CHTTPAsyncQueue : public CWorkerThread
//{
//public:
//	CHTTPAsyncQueue() : CWorkerThread()
//	{
//		SetName("HTTPAsyncQueue");
//	}
//
//	enum {
//		CALL_FUNC,
//		EXIT
//	};
//
//	void AddRequest(CHTTPAsyncQuery* query)
//	{
//		m_pQuery = query;
//		CallWorker(CALL_FUNC);
//	}
//
//	int Run()
//	{
//		unsigned nCall;
//		while(WaitForCall(&nCall))
//		{
//			if(nCall == EXIT)
//			{
//				Reply(1);
//				break;
//			}
//
//			CHTTPAsyncQuery* query = m_pQuery;
//			m_pQuery = NULL;
//			Reply(1);
//			DoHTTPAsyncQuery(query);
//		}
//		return 0;
//	}
//
//	static void DoHTTPAsyncQuery(CHTTPAsyncQuery*);
//
//	CHTTPAsyncQuery* m_pQuery;
//} g_HTTPAsyncQueue;

class CHTTPAsyncQueue : public CThread
{
public:
	CHTTPAsyncQueue() : CThread()
	{
		SetName("HTTPAsyncQueue");
	}

	void AddRequest(CHTTPAsyncQuery* query)
	{
		m_Lock.Lock();
		m_Queue.AddToTail(query);
		m_Lock.Unlock();
		m_Event.Set();
	}

	int Run()
	{
		while(true)
		{
			m_Event.Wait();
			m_Lock.Lock();
			while(m_Queue.Count())
			{
				m_Lock.Unlock();
				DoHTTPAsyncQuery(m_Queue[0]);
				m_Lock.Lock();
				m_Queue.Remove(0);
			}
			m_Lock.Unlock();
		}
	}

	static void DoHTTPAsyncQuery(CHTTPAsyncQuery*);

	CThreadEvent m_Event;
	CThreadFastMutex m_Lock;
	CUtlVector<CHTTPAsyncQuery*> m_Queue;
} g_HTTPAsyncQueue;

void CHTTPAsyncQueue::DoHTTPAsyncQuery(CHTTPAsyncQuery* query)
{
	CCURL curl;
	if(!curl.Open())
	{
		query->m_iCurlCode = -1;
		FinishQuery(query);
		return;
	}

	if(query->m_iType == CHTTPAsyncQuery::HTTP_GET)
		curl.SetOpt(CURLOPT_URL,query->m_Url.Get());
	else if(query->m_iType == CHTTPAsyncQuery::HTTP_POST)
	{
		curl.SetOpt(CURLOPT_URL,query->m_Url.Get());
		curl.SetOpt(CURLOPT_POSTFIELDS,query->m_Args.Get());
	}
	else if(query->m_iType == CHTTPAsyncQuery::HTTP_POSTMULTIPART)
	{
		curl.SetOpt(CURLOPT_URL,query->m_Url.Get());
		curl_httppost* form = NULL,*last = NULL;
		for(int i = 0; i < query->m_PostForms.Count(); i++)
		{
			CHTTPAsyncQuery::formdata_t& data = query->m_PostForms[i];
			curl_formadd(&form,&last,CURLFORM_COPYNAME,data.m_Name.Get(),
				CURLFORM_COPYCONTENTS,data.m_pData,CURLFORM_CONTENTSLENGTH,
					data.m_uSize,CURLFORM_END);
		}
		for(int i = 0; i < query->m_PostFiles.Count(); i++)
		{
			CHTTPAsyncQuery::filedata_t& data = query->m_PostFiles[i];
			/*curl_formadd(&form,&last,CURLFORM_COPYNAME,data.m_Name.Get(),
				CURLFORM_COPYCONTENTS,data.m_pData,CURLFORM_CONTENTSLENGTH,
					data.m_uSize,CURLFORM_END);*/
			curl_formadd(&form,&last,CURLFORM_COPYNAME,data.m_Name.Get(),
				CURLFORM_BUFFER,data.m_FileName.Get(),
				CURLFORM_BUFFERPTR,data.m_pData,CURLFORM_BUFFERLENGTH,
					data.m_uSize,CURLFORM_END);
		}
		curl.SetOpt(CURLOPT_HTTPPOST,form);
	}

	if(!query->m_UserAgent.IsEmpty())
		curl.SetOpt(CURLOPT_USERAGENT,query->m_UserAgent.Get());
	if(!query->m_Proxy.IsEmpty())
		curl.SetOpt(CURLOPT_PROXY,query->m_Proxy.Get());

	if((query->m_iCurlCode=curl.Perform(&query->m_Mem))==CURLE_OK)
		query->m_iHttpCode = curl.GetInfo(CURLINFO_RESPONSE_CODE);
	curl.Close();
	if(query->m_iType == CHTTPAsyncQuery::HTTP_POSTMULTIPART)
	{
		for(int i = 0; i < query->m_PostForms.Count(); i++)
			delete[] (char*)(query->m_PostForms[i].m_pData);
		for(int i = 0; i < query->m_PostFiles.Count(); i++)
			delete[] (char*)(query->m_PostFiles[i].m_pData);
	}
	FinishQuery(query);
}

void AddQuery(CHTTPAsyncQuery* query)
{
	if(!g_HTTPAsyncQueue.IsAlive())
	{
		g_HTTPAsyncQueue.Start();
		//g_HTTPAsyncQueue.SetPriority(THREAD_MODE_BACKGROUND_BEGIN);
	}
	if(!g_HTTPAsyncQueue.IsAlive())
	{
		Warning("[sdk_http] FATAL ERROR - g_HTTPAsyncQueue is dead\n");
		return;
	}

	g_HTTPAsyncQueue.AddRequest(query);
	DevMsg("Added query %p to %d worker thread\n",query,
		g_HTTPAsyncQueue.GetThreadId());
}

DECLARE_LIBRARY("http")
DECLARE_TABLE(http);

DECLARE_FUNCTION(http,Get)
{
	luaL_checktype(L,1,LUA_TTABLE);
	luaL_checktype(L,2,LUA_TFUNCTION);
	luaL_checktype(L,3,LUA_TFUNCTION);

	CHTTPAsyncQuery* query = new CHTTPAsyncQuery;

	lua_pushvalue(L,2);
	query->m_iLuaRefOk = lua_refobj(L);
	lua_pushvalue(L,3);
	query->m_iLuaRefFail = lua_refobj(L);

	lua_pushstring(L,"url");
	lua_gettable(L,1);
	if(!lua_isstring(L,-1))
	{
		delete query;
		lua_pop(L,1);
		luaL_error(L,"url must be specifed in table!");
		return 0;
	}
	query->m_Url.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	lua_pushstring(L,"useragent");
	lua_gettable(L,1);
	if(lua_isstring(L,-1))
		query->m_UserAgent.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	lua_pushstring(L,"proxy");
	lua_gettable(L,1);
	if(lua_isstring(L,-1))
		query->m_Proxy.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	query->m_iType = CHTTPAsyncQuery::HTTP_GET;
	AddQuery(query);
	return 0;
}

DECLARE_FUNCTION(http,Post)
{
	luaL_checktype(L,1,LUA_TTABLE);
	luaL_checktype(L,2,LUA_TFUNCTION);
	luaL_checktype(L,3,LUA_TFUNCTION);

	CHTTPAsyncQuery* query = new CHTTPAsyncQuery;

	lua_pushvalue(L,2);
	query->m_iLuaRefOk = lua_refobj(L);
	lua_pushvalue(L,3);
	query->m_iLuaRefFail = lua_refobj(L);

	lua_pushstring(L,"url");
	lua_gettable(L,1);
	if(!lua_isstring(L,-1))
	{
		delete query;
		lua_pop(L,1);
		luaL_error(L,"url must be specifed in table!");
		return 0;
	}
	query->m_Url.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	lua_pushstring(L,"args");
	lua_gettable(L,1);
	if(!lua_isstring(L,-1))
	{
		delete query;
		lua_pop(L,1);
		luaL_error(L,"args must be specifed in table!");
		return 0;
	}
	query->m_Args.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	lua_pushstring(L,"useragent");
	lua_gettable(L,1);
	if(lua_isstring(L,-1))
		query->m_UserAgent.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	lua_pushstring(L,"proxy");
	lua_gettable(L,1);
	if(lua_isstring(L,-1))
		query->m_Proxy.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	query->m_iType = CHTTPAsyncQuery::HTTP_POST;
	AddQuery(query);
	return 0;
}

DECLARE_FUNCTION(http,PostMultipart)
{
	luaL_checktype(L,1,LUA_TTABLE);
	luaL_checktype(L,2,LUA_TFUNCTION);
	luaL_checktype(L,3,LUA_TFUNCTION);

	CHTTPAsyncQuery* query = new CHTTPAsyncQuery;

	lua_pushvalue(L,2);
	query->m_iLuaRefOk = lua_refobj(L);
	lua_pushvalue(L,3);
	query->m_iLuaRefFail = lua_refobj(L);

	lua_pushstring(L,"url");
	lua_gettable(L,1);
	if(!lua_isstring(L,-1))
	{
		delete query;
		lua_pop(L,1);
		luaL_error(L,"url must be specifed in table!");
		return 0;
	}
	query->m_Url.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	lua_pushstring(L,"form");
	lua_gettable(L,1);
	if(lua_istable(L,-1))
	{
		lua_pushnil(L);
		while(lua_next(L,-2))
		{
			// value -1, key -2, table -3
			CHTTPAsyncQuery::formdata_t form;
			int iLen = lua_strlen(L,-1);

			char* pBuf = new char[iLen];
			memcpy(pBuf,lua_tostring(L,-1),iLen);

			form.m_pData = pBuf;
			form.m_uSize = iLen;
			form.m_Name.Set(lua_tostring(L,-2));
			query->m_PostForms.AddToTail(form);
			lua_pop(L,1);
		}
	}
	lua_pop(L,1);

	lua_pushstring(L,"file");
	lua_gettable(L,1);
	if(lua_istable(L,-1))
	{
		lua_pushnil(L);
		while(lua_next(L,-2))
		{
			// value -1, key -2, table -3
			//Value is a table {NAME,FILENAME,BUF}
			CHTTPAsyncQuery::filedata_t file;
			
			lua_rawgeti(L,-1,1);
			file.m_Name.Set(lua_tostring(L,-1));
			lua_rawgeti(L,-2,2);
			file.m_FileName.Set(lua_tostring(L,-1));
			lua_rawgeti(L,-3,3);

			int iLen = lua_strlen(L,-1);
			char* pBuf = new char[iLen];
			memcpy(pBuf,lua_tostring(L,-1),iLen);
			file.m_pData = pBuf;
			file.m_uSize = iLen;

			query->m_PostFiles.AddToTail(file);
			lua_pop(L,4);
		}
	}
	lua_pop(L,1);

	lua_pushstring(L,"useragent");
	lua_gettable(L,1);
	if(lua_isstring(L,-1))
		query->m_UserAgent.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	lua_pushstring(L,"proxy");
	lua_gettable(L,1);
	if(lua_isstring(L,-1))
		query->m_Proxy.Set(lua_tostring(L,-1));
	lua_pop(L,1);

	query->m_iType = CHTTPAsyncQuery::HTTP_POSTMULTIPART;
	AddQuery(query);
	return 0;
}