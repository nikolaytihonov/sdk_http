#include "baselib.h"
#include "json.h"

DECLARE_LIBRARY("json")
DECLARE_TABLE(json)

#include "tier0/dbg.h"

void _CheckNull(void* pPtr,const char* pName,const char* pFile,int iLine)
{
	if(!pPtr)
	{
		static char szBuf[256] = {0};
		sprintf(szBuf,"%s:%d - !%s",
			pFile,iLine,pName);
		Error(szBuf);
	}
}

#define CheckNull(a) _CheckNull(a,#a,__FILE__,__LINE__)

void JsonToTable(lua_State* L,json_value* json);
void PushJsonValue(lua_State* L,json_value* json);

void JsonArrayToTable(lua_State* L,json_value* json)
{
	lua_newtable(L);
	CheckNull(json->u.array.values);
	for(unsigned int i = 0; i < json->u.array.length; i++)
	{
		json_value* val = json->u.array.values[i];
		CheckNull(val);
		PushJsonValue(L,val);
		lua_rawseti(L,-2,i+1);
	}
}

inline void PushJsonValue(lua_State* L,json_value* json)
{
	switch(json->type)
	{
	case json_integer:
		lua_pushnumber(L,json->u.integer);
		break;
	case json_double:
		lua_pushnumber(L,json->u.dbl);
		break;
	case json_boolean:
		lua_pushboolean(L,json->u.boolean);
		break;
	case json_string:
		lua_pushlstring(L,json->u.string.ptr,
			json->u.string.length);
		break;
	case json_object:
		JsonToTable(L,json);
		break;
	case json_array:
		JsonArrayToTable(L,json);
		break;
	default:
		lua_pushnil(L);
	}
}

void JsonToTable(lua_State* L,json_value* json)
{
	DevMsg("json %p json->type %d\n",json,json->type);
	lua_newtable(L);
	CheckNull(json->u.object.values);
	for(unsigned int i = 0; i < json->u.object.length; i++)
	{
		json_object_entry& obj = json->u.object.values[i];
		CheckNull(obj.name);
		lua_pushlstring(L,obj.name,obj.name_length);
		if(obj.value->type == json_object)
		{
			if(!obj.value)
				lua_pushnil(L);
			else
			{
				CheckNull(obj.value);
				JsonToTable(L,obj.value);
			}
		}
		else if(obj.value->type == json_array)
		{
			if(!obj.value->u.array.values)
				lua_pushnil(L);
			else
			{
				lua_newtable(L);
				//CheckNull(obj.value->u.array.values);
				for(unsigned int i = 0; i < obj.value->u.array.length; i++)
				{
					json_value* value = obj.value->u.array.values[i];
					PushJsonValue(L,value);
					lua_rawseti(L,-2,i+1);
				}
			}
		}
		else
		{
			CheckNull(obj.value);
			PushJsonValue(L,obj.value);
		}
		lua_settable(L,-3);
	}
}

DECLARE_FUNCTION(json,ToTable)
{
	luaL_checktype(L,1,LUA_TSTRING);
	json_value* json = json_parse(lua_tostring(L,1),
		lua_strlen(L,1));
	if(!json) return 0;
	JsonToTable(L,json);
	json_value_free(json);
	return 1;
}