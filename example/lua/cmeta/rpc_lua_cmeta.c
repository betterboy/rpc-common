#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "rpc.h"
#include "rpc_meta.h"

extern rpcMeta auto_rpc_metas;

static int getRpcMetas(lua_State *L)
{
    lua_pushlightuserdata(L, (void*)(&auto_rpc_metas));
    return 1;
}

static luaL_Reg libmetas[] = {
    {"getRpcMetas", getRpcMetas},
    {NULL, NULL}
};

int luaopen_RpcMeta(lua_State *L)
{
    luaL_openlib(L, "RpcMeta", libmetas, 0);
    return 1;
}