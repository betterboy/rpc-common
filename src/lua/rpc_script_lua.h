#ifndef _RPC_SCRIPT_LUA_H_
#define _RPC_SCRIPT_LUA_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#if defined(__cplusplus)
}
#endif

#include "rpc.h"

#define SCRIPT_DECREF(obj, value)
#define SCRIPT_INCREF(obj, value)
#define SCRIPT_INT_CHECK(obj, value) lua_isnumber((lua_State*)(obj->luaState), -1)
#define SCRIPT_INT64_CHECK(obj, value) lua_isnumber((lua_State*)(obj->luaState), -1)
#define SCRIPT_FLOAT_CHECK(obj, value) lua_isnumber((lua_State*)(obj->luaState), -1)
#define SCRIPT_DOUBLE_CHECK(obj, value) lua_isnumber((lua_State*)(obj->luaState), -1)
#define SCRIPT_STRING_CHECK(obj, value) lua_isstring((lua_State*)(obj->luaState), -1)
#define SCRIPT_DICT_CHECK(obj, value) lua_istable((lua_State*)(obj->luaState), -1)
#define SCRIPT_LIST_CHECK(obj, value) lua_istable((lua_State*)(obj->luaState), -1)
#define SCRIPT_TUPLE_CHECK(obj, value) lua_istable((lua_State*)(obj->luaState), -1)

inline static int SCRIPT_TO_INT(rpcObject *obj, void *value)
{
    int i = (int)lua_tointeger((lua_State*)obj->luaState, -1);
    lua_pop((lua_State*)obj->luaState, 1);
    return i;
}

inline static int SCRIPT_TO_INT64(rpcObject *obj, void *value)
{
    int i = (int)lua_tointeger((lua_State*)obj->luaState, -1);
    lua_pop((lua_State*)obj->luaState, 1);
    return i;
}

inline static float SCRIPT_TO_FLOAT(rpcObject *obj, void *value)
{
    float i = (float)lua_tonumber((lua_State*)obj->luaState, -1);
    lua_pop((lua_State*)obj->luaState, 1);
    return i;
}

inline static double SCRIPT_TO_DOUBLE(rpcObject *obj, void *value)
{
    double i = (double)lua_tonumber((lua_State*)obj->luaState, -1);
    lua_pop((lua_State*)obj->luaState, 1);
    return i;
}

#define SCRIPT_STRING_LENGTH(obj, value) lua_strlen((lua_State*)(obj->luaState), -1)
#define SCRIPT_LIST_LENGTH(obj, value) lua_objlen((lua_State*)(obj->luaState), -1)
#define SCRIPT_TUPLE_LENGTH(obj, value) lua_objlen((lua_State*)(obj->luaState), -1)

static void *SCRIPT_LIST_GET_ITEM(rpcObject *obj, void *list, int i)
{
    lua_pushinteger((lua_State*)obj->luaState, i + 1);
    lua_rawget((lua_State*)obj->luaState, -2);
    return (void*)obj;
}

static void SCRIPT_LIST_SET_ITEM(rpcObject *obj, void *list, int i, void *value)
{
    lua_pushinteger((lua_State*)obj->luaState, i + 1);
    lua_insert((lua_State*)obj->luaState, -2);
    lua_rawset((lua_State*)obj->luaState, -3);
}

static void *SCRIPT_TUPLE_GET_ITEM(rpcObject *obj, void *tuple, int i)
{
    return SCRIPT_LIST_GET_ITEM(obj, tuple, i);
}

static void SCRIPT_TUPLE_SET_ITEM(rpcObject *obj, void *tuple, int i, void *node)
{
    SCRIPT_LIST_SET_ITEM(obj, tuple, i, node);
}

static void *SCRIPT_DICT_GET_BY_STRING(rpcObject *obj, void *dict, const char *key)
{
    lua_pushstring((lua_State*)obj->luaState, key);
    lua_rawget((lua_State*)obj->luaState, -2);
    return (void*)obj;
}

static void SCRIPT_DICT_SET_ITEM_STRING(rpcObject *obj, void *dict, const char *key, void *value)
{
    lua_pushstring((lua_State*)obj->luaState, key);
    lua_insert((lua_State*)obj->luaState, -2);
    lua_rawset((lua_State*)obj->luaState, -3);
}

#define SCRIPT_CREATE_INT(obj, i) (lua_pushinteger((lua_State*)(obj->luaState), (lua_Integer)i), obj)
#define SCRIPT_CREATE_INT64(obj, i) (lua_pushnumber((lua_State*)(obj->luaState), (lua_Number)i), obj)
#define SCRIPT_CREATE_STRING(obj, s) (lua_pushstring((lua_State*)(obj->luaState), s), obj)
#define SCRIPT_CREATE_STRING_SIZE(obj, s, size) (lua_pushlstring((lua_State*)(obj->luaState), s, size), obj)
#define SCRIPT_CREATE_TUPLE(obj, len) (lua_createtable((lua_State*)(obj->luaState), len, 0), obj)
#define SCRIPT_CREATE_LIST(obj, len) SCRIPT_CREATE_TUPLE(obj, len)
#define SCRIPT_CREATE_DICT(obj, len) (lua_createtable((lua_State*)(obj->luaState), 0, len), obj)
#define SCRIPT_AFTER_PACK_ARRAY(obj, list) lua_pop((lua_State*)(obj->luaState), 1)
#define SCRIPT_AFTER_PACK_CLASS(obj, list) lua_pop((lua_State*)(obj->luaState), 1)

#define getScriptObjectType(obj, value) (luaL_typename((lua_State*)(obj->luaState), -1))

static const char *RPC_APPEND_STRING(rpcMbuf *mbuf, rpcObject *obj, void *value)
{
    rpc_size_t len = (rpc_size_t)SCRIPT_STRING_LENGTH(obj, value);
    rpcMbufEnqType(mbuf, &len, rpc_size_t);
    const char *s = lua_tostring((lua_State*)(obj->luaState), -1);
    lua_pop((lua_State*)(obj->luaState), 1);
    rpcMbufEnq(mbuf, s, len);
    return s;
}

#endif