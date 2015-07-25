#include "lmpack.h"
#define LMPACK_NAME "lua_msgpack"

void mp_encode_lua_type(lua_State *L, mp_buf *buf, int level) {
    int t = lua_type(L,-1);
    if (t == LUA_TTABLE && level == LUACMSGPACK_MAX_NESTING) t = LUA_TNIL;
    switch(t) {
    case LUA_TSTRING:// mp_encode_lua_string(L,buf); break;
    case LUA_TBOOLEAN: //mp_encode_lua_bool(L,buf); break;
    case LUA_TNUMBER:
    case LUA_TTABLE: //mp_encode_lua_table(L,buf,level); break;
    case LUA_TUSERDATA:
    default:// mp_encode_lua_null(L,buf); break;
        break;
    }
    lua_pop(L,1);
}

/*  */
int _pack(lua_State *L) {
    int nargs = lua_gettop(L);
    int i;
    mp_buf *buf = (struct mp_buf *)luaL_checkudata(L, 1, LMPACK_NAME);
    for(i = 2; i <= nargs; i++) {
        lua_pushvalue(L, i);
        mp_encode_lua_type(L,buf,0);
    }
    mp_buf_free(buf);
    return 1;
}

int _gc(lua_State *L) {
    struct mp_buf *mp = (struct mp_buf *)luaL_checkudata (L, 1, LMPACK_NAME);
    mp_buf_free(mp);
    return 0;
}

int _create(struct lua_State *L) {
    struct mp_buf *mp = (struct mp_buf*)lua_newuserdata(L, sizeof(*mp));
    memset(mp, 0, sizeof(*mp));
    if (luaL_newmetatable(L, LMPACK_NAME)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _pack);
        lua_setfield(L, -2, "pack");/*
        lua_pushcfunction(L, _mpack_packPacket);
        lua_setfield(L, -2, "packPacket");
        lua_pushcfunction(L, _mpack_unpack);
        lua_setfield(L, -2, "unpack");
        lua_pushcfunction(L, _mpack_unpackPacket);
        lua_setfield(L, -2, "unpackPacket");*/
    }
    lua_setmetatable(L, -2);
    return 1;
}

int luaopen_lmpack(struct lua_State *L) {
    struct luaL_reg driver[] = {
        {"mp_create", _create},
        {NULL, NULL},
    };
    luaL_openlib(L, "LUA_MSGPACK", driver, 0);
    return 1;
}