#include <stdio.h>
#include <string.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

int luaopen_RpcLua(lua_State *L);

#if defined(__cplusplus)
}
#endif

#include "mbuf.h"
#include "rpc_c.h"
#include "rpc_script.h"

#define RPC_METATABLE   "rpc"

#ifdef MS_WINDOWS
#define __func__    __FUNCTION__
#endif

static int packFromLua(lua_State *L)
{
    rpcObject *obj = (rpcObject*)luaL_checkudata(L, 1, RPC_METATABLE);
    int argc = lua_gettop(L);
    if (argc < 2) {
        fprintf(stderr, "%s packFromLua need args:rpcobj, pid, args\n", __func__);
        lua_pushnil(L);
        return 1;
    }

    rpc_pto_id_t pid = (rpc_pto_id_t)lua_tointeger(L, 2);
    rpcFunction *func = findFunctionByPid(obj->table->funcTable, pid);
    if (func == NULL) {
        fprintf(stderr, "PackError: pid:%d not exists!\n", pid);
        lua_pushnil(L);
        return 1;
    }

    if (argc > 2) {
        lua_settop(L, 3);
    }

    obj->luaState = L;
    obj->func = func;
    obj->packArgs = NULL;
    int ret = scriptPack(obj);
    if (ret) {
        lua_pushnil(L);
        return 1;
    }

    rpcMbuf *mbuf = obj->packBuf;
    const char  *s = rpcMbufPullup(mbuf);
    lua_pushlstring(L, s, mbuf->dataSize);
    return 1;
}

static int unpackToLua(lua_State *L)
{
    rpc_size_t plen = 0;
    size_t inlen = 0;
    int offset = 0;
    rpc_pto_id_t pid;
    rpcFunction *func;
    const char *input, *pinput;
    
    rpcObject *obj = (rpcObject*)luaL_checkudata(L, 1, RPC_METATABLE);
    if (lua_gettop(L) != 3) {
        fprintf(stderr, "%s unpack need: rpc, bytes, offset\n", __func__);
        lua_pushnil(L);
        return 1;
    }

    input = lua_tolstring(L, 2, &inlen);
    offset = lua_tointeger(L, 3);
    if (offset < 0) {
        fprintf(stderr, "offset must be unsigned integer(%d)\n", offset);
        goto error;
    }

    inlen -= offset;
    if (inlen < sizeof(rpc_pto_id_t)) {
        goto error;
    }

    pinput = input + offset;
    pid = *((rpc_pto_id_t*)pinput);
    func = findFunctionByPid(obj->table->funcTable, pid);
    if (func == NULL) {
        fprintf(stderr, "UnpackError: pid:%d not exists!\n", pid);
        goto error;
    }

    obj->func = func;
    pinput += sizeof(rpc_pto_id_t);
    // inlen -= sizeof(rpc_pto_id_t);
    rpcInputBuf *inbuf = obj->inBuf;
    rpcInputBufInit(inbuf, pinput, inlen);
    if (func->c_func != NULL) {
        c_func_implement(obj, &plen, &pid);
        goto error;
    }

    if (scriptUnpack(obj)) {
        goto error;
    }

    plen = (rpc_size_t)inbuf->offset + sizeof(rpc_pto_id_t);
    lua_pushinteger(L, pid);
    lua_pushinteger(L, plen);
    lua_insert(L, -3);
    lua_insert(L, -2);
    return 3;

error:
    printf("plen=%d\n", plen);
    lua_pushinteger(L, plen);
    lua_pushinteger(L, pid);
    lua_pushnil(L);
    return 3;
}

static int getPidTable(lua_State *L)
{
    rpcObject *obj = (rpcObject*)luaL_checkudata(L, 1, RPC_METATABLE);

    lua_createtable(L, 0, 0);
    rpcFunctionTable *funcTable = obj->table->funcTable;
    int i;
    for (i = 0; i < funcTable->elts; ++i) {
        rpcFunction *func = &funcTable->rpcFuncList[i];
        lua_pushinteger(L, (lua_Integer)func->pto_id);
        lua_pushstring(L, func->argClass.name);
        lua_rawset(L, -3);
    }

    return 1;
}

static void createClassTable(lua_State *L, rpcObject *obj)
{
    rpcClassTable *table = obj->table->classTable;
    int i, j;
    lua_createtable(L, 0, 0);

    for (i = 0; i < table->elts; ++i) {
        rpcClass *cls = &table->rpcClassList[i];
        lua_pushinteger(L, i + 1);
        lua_createtable(L, 0, 0);
        lua_pushstring(L, "name");
        lua_pushstring(L, cls->name);
        lua_rawset(L, -3);

        lua_pushstring(L, "c_imp");
        lua_pushinteger(L, cls->c_impl);
        lua_rawset(L, -3);

        lua_pushstring(L, "fields");
        lua_createtable(L, cls->fieldCount, 0);

        for (j = 0; j < cls->fieldCount; ++j) {
            rpcField *field = &cls->fields[j];
            lua_pushinteger(L, j + 1);
            lua_createtable(L, 0, 0);

            lua_pushstring(L, "name");
            lua_pushstring(L, field->name);
            lua_rawset(L, -3);

            lua_pushstring(L, "type");
            lua_pushinteger(L, field->type);
            lua_rawset(L, -3);

            lua_pushstring(L, "classIndex");
            lua_pushinteger(L, field->clsIndex);
            lua_rawset(L, -3);
            lua_rawset(L, -3);
        }

        lua_rawset(L, -3); //fields table
        lua_rawset(L, -3); //class item
    }
}

static void createFunctionTable(lua_State *L, rpcObject *obj)
{
    int i, j;
    rpcFunctionTable *table = obj->table->funcTable;
    lua_createtable(L, 0, 0);

    for (i = 0; i < table->elts; ++i) {
        rpcFunction *func = &table->rpcFuncList[i];
        lua_pushinteger(L, func->pto_id);
        lua_createtable(L, 0, 0);

        lua_pushstring(L, "name");
        lua_pushstring(L, func->argClass.name);
        lua_rawset(L, -3);

        lua_pushstring(L, "module");
        lua_pushstring(L, func->module);
        lua_rawset(L, -3);

        lua_pushstring(L, "c_imp");
        lua_pushinteger(L, func->argClass.c_impl);
        lua_rawset(L, -3);

        rpcClass *cls = &func->argClass;
        lua_pushstring(L, "fields");
        lua_createtable(L, cls->fieldCount, 0);
        for (j = 0; j < cls->fieldCount; ++j) {
            lua_pushinteger(L, j + 1);
            lua_createtable(L, 0, 0);
            rpcField *field = &cls->fields[j];

            lua_pushstring(L, "name");
            lua_pushstring(L, field->name);
            lua_rawset(L, -3);

            lua_pushstring(L, "type");
            lua_pushinteger(L, field->type);
            lua_rawset(L, -3);

            lua_pushstring(L, "classIndex");
            lua_pushinteger(L, field->clsIndex);
            lua_rawset(L, -3);

            lua_rawset(L, -3);
        }

        lua_rawset(L, -3);
        lua_rawset(L, -3);
    }
}

static int createRpcTable(lua_State *L)
{
    rpcObject *obj = (rpcObject*)luaL_checkudata(L, 1, RPC_METATABLE);
    lua_createtable(L, 0, 0);

    lua_pushstring(L, "class");
    createClassTable(L, obj);
    lua_rawset(L, -3);

    lua_pushstring(L, "function");
    createFunctionTable(L, obj);
    lua_rawset(L, -3);

    return 1;
}

static int gcObject(lua_State *L)
{
    rpcObject *obj = (rpcObject*)luaL_checkudata(L, 1, RPC_METATABLE);
    rpcObjectFree(obj);
    return 0;
}

static int createRpcObject(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc != 2) {
        fprintf(stderr, "%s must be pass rpc.cfg, rpc_metas\n", __func__);
        lua_pushnil(L);
        return 1;
    }

    const char *cfg_path = lua_tostring(L, 1);
    rpcMeta *meta = (rpcMeta*)lua_touserdata(L, 2);

    rpcObject *obj = (rpcObject*)lua_newuserdata(L, sizeof(rpcObject));
    if (NULL == rpcObjectInit(obj, cfg_path, meta->classMetaList, meta->funcMetaList)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }

    luaL_getmetatable(L, RPC_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

static int createRpcFromBuf(lua_State *L)
{
    size_t size;
    int argc = lua_gettop(L);
    if (argc != 2) {
        fprintf(stderr, "%s must be pass buf_string, rpc_metas\n", __func__);
        lua_pushnil(L);
        return 1;
    }

    const char *buf = lua_tolstring(L, 1, &size);
    rpcMeta *meta = (rpcMeta*)lua_touserdata(L, 2);

    rpcObject *obj = (rpcObject*)lua_newuserdata(L, sizeof(rpcObject));
    if (NULL == initRpcObjectFromBuf(obj, buf, size, meta->classMetaList, meta->funcMetaList)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }

    luaL_getmetatable(L, RPC_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

#define BYTES_TO_NUMBER(L, type) \
    size_t len; \
    int argType = lua_type(L, 1);\
    if (LUA_TSTRING != argType) {\
        fprintf(stderr, "%s error. arg not match. except:%s, given:%s\n", __func__, lua_typename(L, LUA_TSTRING), lua_typename(L, argType));\
        lua_pushnil(L);\
        return 1;\
    }\
    const char *str = luaL_checklstring(L, -1, &len);\
    if (len < sizeof(type)) {\
        lua_pushnil(L);\
        return 1;\
    }\
    lua_pushnumber(L, *((type*)str));\
    return 1;

static int bytes_to_int64(lua_State *L)
{
    BYTES_TO_NUMBER(L, int64_t)
}

static int bytes_to_int32(lua_State *L)
{
    BYTES_TO_NUMBER(L, int32_t)
}

static int bytes_to_int16(lua_State *L)
{
    BYTES_TO_NUMBER(L, int16_t)
}

static int bytes_to_int8(lua_State *L)
{
    BYTES_TO_NUMBER(L, int8_t)
}

#define NUMBER_TO_BYTES(L, type)\
    int arg_type = lua_type(L, 1);\
    if (LUA_TNUMBER != arg_type) {\
        lua_pushnil(L);\
        return 1;\
    }\
    type v = (type)lua_tonumber(L, -1);\
    lua_pushlstring(L, (const char*)(&v), sizeof(type));\
    return 1;


static int int64_to_bytes(lua_State *L)
{
    NUMBER_TO_BYTES(L, int64_t)
}

static int int32_to_bytes(lua_State *L)
{
    NUMBER_TO_BYTES(L, int32_t)
}

static int int16_to_bytes(lua_State *L)
{
    NUMBER_TO_BYTES(L, int16_t)
}

static int int8_to_bytes(lua_State *L)
{
    NUMBER_TO_BYTES(L, int8_t)
}

#define GEN_INT_GET_ATTR(key)\
static int get_##key(lua_State *L)\
{\
    rpcObject *obj = (rpcObject*)luaL_checkudata(L, 1, RPC_METATABLE);\
    lua_pushinteger(L, obj->key);\
    return 1;\
}

GEN_INT_GET_ATTR(cPackCnt)
GEN_INT_GET_ATTR(cUnpackCnt)
GEN_INT_GET_ATTR(scriptPackCnt)
GEN_INT_GET_ATTR(scriptUnpackCnt)


static const struct luaL_Reg rpc_obj_func[] = {
    {"pack", packFromLua},
    {"unpack", unpackToLua},
    {"get_pid_table", getPidTable},
    {"create_rpc_table", createRpcTable},
    {"get_cpack_cnt", get_cPackCnt},
    {"get_cunpack_cnt", get_cUnpackCnt},
    {"get_script_packcnt", get_scriptPackCnt},
    {"get_script_unpackcnt", get_scriptUnpackCnt},
    {"__gc", gcObject},
    {NULL, NULL}
};

static const struct luaL_Reg rpc_module_func[] = {
    {"new", createRpcObject},
    {"create_from_buf", createRpcFromBuf},
    {"bytes_to_int64", bytes_to_int64},
    {"bytes_to_int32", bytes_to_int32},
    {"bytes_to_int16", bytes_to_int16},
    {"bytes_to_int8", bytes_to_int8},
    {"int64_to_bytes", int64_to_bytes},
    {"int32_to_bytes", int32_to_bytes},
    {"int16_to_bytes", int16_to_bytes},
    {"int8_to_bytes", int8_to_bytes},
    {NULL, NULL}
};

int luaopen_RpcLua(lua_State *L)
{
    luaL_newmetatable(L, RPC_METATABLE);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_openlib(L, NULL, rpc_obj_func, 0);
    luaL_openlib(L, "RpcLua", rpc_module_func, 0);
    return 1;
}