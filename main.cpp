#define GAME_DLL
#include "plugin.h"
#include "baselib.h"
#include "tier1.h"
#include "threadtools.h"
#include "http.h"
#include "lua.h"

DECLARE_PLUGIN(CSDKHTTP)
	virtual bool Load(CreateInterfaceFn,CreateInterfaceFn);
	virtual void GameFrame(bool);
	virtual bool LuaInit(lua_State*);

	void ProcessResponse(CHTTPAsyncQuery*);

	lua_State* L;
	CThreadFastMutex m_Lock;
	CUtlVector<CHTTPAsyncQuery*> m_Responses;
END_PLUGIN(CSDKHTTP,"GLua9 HTTP");

void FinishQuery(CHTTPAsyncQuery* query)
{
	//s_CSDKHTTP.m_Lock.Lock();
	s_CSDKHTTP.m_Responses.AddToTail(query);
	//s_CSDKHTTP.m_Lock.Unlock();

	DevMsg("Query %p finished\n",query);
}

bool CSDKHTTP::Load(CreateInterfaceFn,CreateInterfaceFn)
{
	return true;
}

void CSDKHTTP::GameFrame(bool bSimulating)
{
	//m_Lock.Lock();
	for(int i = 0; i < m_Responses.Count();)
	{
		CHTTPAsyncQuery* query = m_Responses[i];
		DevMsg("MainThrd catch query %p\n",query);
		ProcessResponse(query);
		delete query;
		m_Responses.Remove(i);
	}
	//m_Lock.Unlock();
}

bool CSDKHTTP::LuaInit(lua_State* _L)
{
	L = _L;
	CLuaLibrary::Init(L);
	return true;
}

int _aterror(lua_State* L)
{
	return g_pLua502->GetLuaCallbacks()->OnLuaError(L);
}

void CSDKHTTP::ProcessResponse(CHTTPAsyncQuery* query)
{
	lua_pushcfunction(L,_aterror);
	int iHandler = lua_gettop(L);

	if(query->m_iCurlCode == CURLE_OK)
	{
		lua_pushref(L,query->m_iLuaRefOk);
		if(!lua_isfunction(L,-1))
		{
			lua_pop(L,2);
			lua_unrefobj(L,query->m_iLuaRefOk);
			lua_unrefobj(L,query->m_iLuaRefFail);
			return;
		}

		lua_pushlstring(L,(const char*)query->m_Mem.m_pMem,
			query->m_Mem.m_uSize);
		lua_pushnumber(L,query->m_iHttpCode);
		lua_pcall(L,2,0,iHandler);
	}
	else
	{
		lua_pushref(L,query->m_iLuaRefFail);
		if(!lua_isfunction(L,-1))
		{
			lua_pop(L,2);
			lua_unrefobj(L,query->m_iLuaRefOk);
			lua_unrefobj(L,query->m_iLuaRefFail);
			return;
		}

		lua_pushnumber(L,query->m_iCurlCode);
		lua_pcall(L,1,0,iHandler);
	}

	lua_unrefobj(L,query->m_iLuaRefOk);
	lua_unrefobj(L,query->m_iLuaRefFail);
	lua_pop(L,1); //Remove handler
}