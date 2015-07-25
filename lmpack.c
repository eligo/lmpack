#include "lmpack.h"
#define LMPACK_NAME "lua_msgpack" 
/* Allows a preprocessor directive to override MAX_NESTING */
#ifndef LUACMSGPACK_MAX_NESTING
    #define LUACMSGPACK_MAX_NESTING  16 /* Max tables nesting. */
#endif
/* Check if float or double can be an integer without loss of precision */
#define IS_INT_TYPE_EQUIVALENT(x, T) (!isinf(x) && (T)(x) == (x))
#define IS_INT64_EQUIVALENT(x) IS_INT_TYPE_EQUIVALENT(x, int64_t)
#define IS_INT_EQUIVALENT(x) IS_INT_TYPE_EQUIVALENT(x, int)
struct mp_buf_t {
    mp_buf * mp;
};

void mp_encode_lua_type(lua_State *L, mp_buf *buf, int idx) {
    int t = lua_type(L, idx);
    switch(t) {
        case LUA_TSTRING: {
            size_t len;
            const char *s = lua_tolstring(L, idx, &len);
            mp_encode_bytes(buf,(const unsigned char*)s,len);
            break;
        }
        case LUA_TBOOLEAN: {
            unsigned char b = lua_toboolean(L, idx) ? 0xc3 : 0xc2;
            mp_buf_append(buf, &b, 1);
            break;
        }
        case LUA_TNUMBER: {
            lua_Number n = lua_tonumber(L, idx);
            if (IS_INT64_EQUIVALENT(n)) {
                mp_encode_int(buf, (int64_t)n);
            } else {
                mp_encode_double(buf,(double)n);
            }
            break;
        }
        case LUA_TUSERDATA: {
            struct mp_buf *buf2 = (struct mp_buf *)luaL_checkudata(L, idx, LMPACK_NAME);
            if (buf2 == NULL) luaL_error(L, "expected mpacket in params %d", idx);
            mp_encode_mp(buf, buf2);
            break;
        }
        case LUA_TTABLE: //mp_encode_lua_table(L,buf,level); break;
        default: {// mp_encode_lua_null(L,buf); break; 
            //TODO 
           break;
        }
    }
}


#define mp_cur_consume(_buf,_len) do { _buf->cur += _len; } while(0)
#define lua_pushunsigned(L, n) lua_pushnumber(L, n)
#define mp_cur_need(_buf,_len) do { \
    if (_buf->len - _buf->cur < _len) { \
        luaL_error(L, "Value less than expected"); \
        return 0; \
    } \
} while(0)
int mp_decode_int(mp_buf *c, int64_t *value, lua_State *L) {
     mp_cur_need(c, 1);
     unsigned char *p = c->b + c->cur;
     switch(p[0]) {
         case 0xcc:  /* uint 8 */
            mp_cur_need(c,2);
            *value = p[1];
            mp_cur_consume(c,2);
            break;
        case 0xd0:  /* int 8 */
            mp_cur_need(c,2);
            *value = (signed char)p[1];
            mp_cur_consume(c,2);
            break;
        case 0xcd:  /* uint 16 */
            mp_cur_need(c,3);
            *value =
                (p[1] << 8) |
                 p[2];
            mp_cur_consume(c,3);
            break;
        case 0xd1:  /* int 16 */
            mp_cur_need(c,3);
            *value = (int16_t)
                (p[1] << 8) |
                 p[2];
            mp_cur_consume(c,3);
            break;
        case 0xce:  /* uint 32 */
            mp_cur_need(c,5);
            *value =
                ((uint32_t)p[1] << 24) |
                ((uint32_t)p[2] << 16) |
                ((uint32_t)p[3] << 8) |
                 (uint32_t)p[4];
            mp_cur_consume(c,5);
            break;
        case 0xd2:  /* int 32 */
            mp_cur_need(c,5);
            *value =
                ((int32_t)p[1] << 24) |
                ((int32_t)p[2] << 16) |
                ((int32_t)p[3] << 8) |
                 (int32_t)p[4];
            mp_cur_consume(c,5);
            break;
        case 0xcf:  /* uint 64 */
            mp_cur_need(c,9);
            *value =
                ((uint64_t)p[1] << 56) |
                ((uint64_t)p[2] << 48) |
                ((uint64_t)p[3] << 40) |
                ((uint64_t)p[4] << 32) |
                ((uint64_t)p[5] << 24) |
                ((uint64_t)p[6] << 16) |
                ((uint64_t)p[7] << 8) |
                 (uint64_t)p[8];
            mp_cur_consume(c,9);
            break;
        case 0xd3:  /* int 64 */
            mp_cur_need(c,9);
            *value =
                ((int64_t)p[1] << 56) |
                ((int64_t)p[2] << 48) |
                ((int64_t)p[3] << 40) |
                ((int64_t)p[4] << 32) |
                ((int64_t)p[5] << 24) |
                ((int64_t)p[6] << 16) |
                ((int64_t)p[7] << 8) |
                 (int64_t)p[8];
            mp_cur_consume(c,9);
            break;
        case 0xc0:  /* nil */
        case 0xc3:  /* true */
        case 0xc2:  /* false */
        case 0xc4:  /* mk */
        case 0xca:  /* float */
        case 0xcb:  /* double */
        case 0xd9:  /* raw 8 */
        case 0xda:  /* raw 16 */
        case 0xdb:  /* raw 32 */
        case 0xdc:  /* array 16 */
        case 0xdd:  /* array 32 */
        case 0xde:  /* map 16 */
        case 0xdf:  /* map 32 */
            return 0;
        default:   {/* types that can't be idenitified by first byte value. */
            if ((p[0] & 0x80) == 0) {   /* positive fixnum */
                *value = p[0];
                mp_cur_consume(c,1);
            } else if ((p[0] & 0xe0) == 0xe0) {  /* negative fixnum */
                *value = (signed char)p[0];
                mp_cur_consume(c,1);
            } else if ((p[0] & 0xe0) == 0xa0) {  /* fix raw */
                return 0;
            } else if ((p[0] & 0xf0) == 0x90) {  /* fix map */
               return 0;
            } else if ((p[0] & 0xf0) == 0x80) {  /* fix map */
                return 0;
            } else {
                return 0;
            }
        }
    }
    return 1;
}

int _unpack(lua_State *L) {
    int popn = 1;
    struct mp_buf_t *mp = (struct mp_buf_t *)luaL_checkudata(L, 1, LMPACK_NAME);
    struct mp_buf *c = mp->mp;
    if (lua_gettop(L) > 1)
        popn = luaL_checkinteger(L, 2);
    for (;popn > 0; popn--) {
        mp_cur_need(c, 1);
        unsigned char *p = c->b + c->cur;
        switch(p[0]) {
            case 0xc0:  /* nil */
                lua_pushnil(L);
                mp_cur_consume(c,1);
                break;
            case 0xc3:  /* true */
                lua_pushboolean(L,1);
                mp_cur_consume(c,1);
                break;
            case 0xc2:  /* false */
                lua_pushboolean(L,0);
                mp_cur_consume(c,1);
                break;
            case 0xc4: {
                int64_t size = 0;
                if (mp_decode_int(c, &size, L) != 0) luaL_error(L, "Error format mp_buf");
                if (size < 0)   luaL_error(L, "Error format mp_buf len");
                mp_cur_need(c,size);//consuned in mp_decode_int
                struct mp_buf_t *nmp = (struct mp_buf_t*)lua_newuserdata(L, sizeof(*nmp));
                memset(nmp, 0, sizeof(*nmp));
                if (0 != luaL_newmetatable(L, LMPACK_NAME))
                    luaL_error(L, "Impossible error!");
                lua_setmetatable(L, -2);
                nmp->mp = mp_buf_new();
                p = c->b + c->cur;
                mp_buf_append(nmp->mp, p, size);
                mp_cur_consume(c, size);
                break;
            }
            case 0xca:  /* float */
                mp_cur_need(c,5);
                assert(sizeof(float) == 4);
                {
                    float f;
                    memcpy(&f,p+1,4);
                    memrevifle(&f,4);
                    lua_pushnumber(L,f);
                    mp_cur_consume(c,5);
                }
                break;
            case 0xcb:  /* double */
                mp_cur_need(c,9);
                assert(sizeof(double) == 8);
                {
                    double d;
                    memcpy(&d,p+1,8);
                    memrevifle(&d,8);
                    lua_pushnumber(L,d);
                    mp_cur_consume(c,9);
                }
                break;
            case 0xd9:  /* raw 8 */
                mp_cur_need(c,2);
                {
                    size_t l = p[1];
                    mp_cur_need(c,2+l);
                    lua_pushlstring(L,(char*)p+2,l);
                    mp_cur_consume(c,2+l);
                }
                break;
            case 0xda:  /* raw 16 */
                mp_cur_need(c,3);
                {
                    size_t l = (p[1] << 8) | p[2];
                    mp_cur_need(c,3+l);
                    lua_pushlstring(L,(char*)p+3,l);
                    mp_cur_consume(c,3+l);
                }
                break;
            case 0xdb:  /* raw 32 */
                mp_cur_need(c,5);
                {
                    size_t l = ((size_t)p[1] << 24) |
                               ((size_t)p[2] << 16) |
                               ((size_t)p[3] << 8) |
                                (size_t)p[4];
                    mp_cur_consume(c,5);
                    mp_cur_need(c,l);
                    lua_pushlstring(L,(char*)p,l);
                    mp_cur_consume(c,l);
                }
                break;
            case 0xdc:  /* array 16 */
            case 0xdd:  /* array 32 */
            case 0xde:  /* map 16 */
            case 0xdf:  /* map 32 */
                luaL_error(L, "Unsupport Value Type %x", p[0]);
                break;
            default:   {/* types that can't be idenitified by first byte value. */
                int64_t value = 0;
                if (0 == mp_decode_int(c, &value, L)) {
                    lua_pushnumber(L, value);
                } else if ((p[0] & 0xe0) == 0xa0) {  /* fix raw */
                    size_t l = p[0] & 0x1f;
                    mp_cur_need(c,1+l);
                    lua_pushlstring(L,(char*)p+1,l);
                    mp_cur_consume(c,1+l);
                } else if ((p[0] & 0xf0) == 0x90) {  /* fix map */
                    luaL_error(L, "Unsupport Value Type %x", p[0]);
                } else if ((p[0] & 0xf0) == 0x80) {  /* fix map */
                    luaL_error(L, "Unsupport Value Type %x", p[0]);
                } else {
                    luaL_error(L, "Unsupport Value Type %x", p[0]);
                }
            }
        }
    }
    return popn;
}

int _pack(lua_State *L) {
    int i;
    int nargs = lua_gettop(L);
    struct mp_buf_t *mp = (struct mp_buf_t *)luaL_checkudata(L, 1, LMPACK_NAME);
    for(i = 2; i <= nargs; i++)
        mp_encode_lua_type(L, mp->mp, i);
    return 0;
}

int _gc(lua_State *L) {
    struct mp_buf_t *mp = (struct mp_buf_t *)luaL_checkudata (L, 1, LMPACK_NAME);
    mp_buf_free(mp->mp);
    return 0;
}

int _create(struct lua_State *L) {
    struct mp_buf_t *mp = (struct mp_buf_t*)lua_newuserdata(L, sizeof(*mp));
    memset(mp, 0, sizeof(*mp));
    mp->mp = mp_buf_new();
    if (luaL_newmetatable(L, LMPACK_NAME)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, _gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, _pack);
        lua_setfield(L, -2, "pack");
        lua_pushcfunction(L, _unpack);
        lua_setfield(L, -2, "unpack");
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