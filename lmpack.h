#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "lua.h"
#include "lauxlib.h"
/* Allows a preprocessor directive to override MAX_NESTING */
#ifndef LUACMSGPACK_MAX_NESTING
    #define LUACMSGPACK_MAX_NESTING  16 /* Max tables nesting. */
#endif

typedef struct mp_buf {
    unsigned char *b;
    size_t len, free;
} mp_buf;

struct mp_buf *mp_buf_new(lua_State *L) {
    mp_buf *buf = (mp_buf*)malloc(sizeof(*buf));
    buf->b = NULL;
    buf->len = buf->free = 0;
    return buf;
}

void mp_buf_free(mp_buf *buf) {
    if (buf->b) 
        free(buf->b);
    free(buf);
}

void mp_buf_append(mp_buf *buf, const unsigned char *s, size_t len) {
    if (buf->free < len) {
        size_t newlen = buf->len+len;
        buf->b = (unsigned char*)realloc(buf->b, newlen*2);
        buf->free = newlen;
    }
    memcpy(buf->b+buf->len,s,len);
    buf->len += len;
    buf->free -= len;
}

/* ------------------------- Low level MP encoding -------------------------- */
void mp_encode_bytes(mp_buf *buf, const unsigned char *s, size_t len) {
    unsigned char hdr[5];
    int hdrlen;

    if (len < 32) {
        hdr[0] = 0xa0 | (len&0xff); /* fix raw */
        hdrlen = 1;
    } else if (len <= 0xff) {
        hdr[0] = 0xd9;
        hdr[1] = len;
        hdrlen = 2;
    } else if (len <= 0xffff) {
        hdr[0] = 0xda;
        hdr[1] = (len&0xff00)>>8;
        hdr[2] = len&0xff;
        hdrlen = 3;
    } else {
        hdr[0] = 0xdb;
        hdr[1] = (len&0xff000000)>>24;
        hdr[2] = (len&0xff0000)>>16;
        hdr[3] = (len&0xff00)>>8;
        hdr[4] = len&0xff;
        hdrlen = 5;
    }
    mp_buf_append(buf,hdr,hdrlen);
    mp_buf_append(buf,s,len);
}

void memrevifle(void *ptr, size_t len) {
    unsigned char   *p = (unsigned char *)ptr,
                    *e = (unsigned char *)p+len-1,
                    aux;
    int test = 1;
    unsigned char *testp = (unsigned char*) &test;

    if (testp[0] == 0) return; /* Big endian, nothing to do. */
    len /= 2;
    while(len--) {
        aux = *p;
        *p = *e;
        *e = aux;
        p++;
        e--;
    }
}
/* we assume IEEE 754 internal format for single and double precision floats. */
void mp_encode_double(mp_buf *buf, double d) {
    unsigned char b[9];
    float f = d;

    assert(sizeof(f) == 4 && sizeof(d) == 8);
    if (d == (double)f) {
        b[0] = 0xca;    /* float IEEE 754 */
        memcpy(b+1,&f,4);
        memrevifle(b+1,4);
        mp_buf_append(buf,b,5);
    } else if (sizeof(d) == 8) {
        b[0] = 0xcb;    /* double IEEE 754 */
        memcpy(b+1,&d,8);
        memrevifle(b+1,8);
        mp_buf_append(buf,b,9);
    }
}

void mp_encode_int(mp_buf *buf, int64_t n) {
    unsigned char b[9];
    int enclen;

    if (n >= 0) {
        if (n <= 127) {
            b[0] = n & 0x7f;    /* positive fixnum */
            enclen = 1;
        } else if (n <= 0xff) {
            b[0] = 0xcc;        /* uint 8 */
            b[1] = n & 0xff;
            enclen = 2;
        } else if (n <= 0xffff) {
            b[0] = 0xcd;        /* uint 16 */
            b[1] = (n & 0xff00) >> 8;
            b[2] = n & 0xff;
            enclen = 3;
        } else if (n <= 0xffffffffLL) {
            b[0] = 0xce;        /* uint 32 */
            b[1] = (n & 0xff000000) >> 24;
            b[2] = (n & 0xff0000) >> 16;
            b[3] = (n & 0xff00) >> 8;
            b[4] = n & 0xff;
            enclen = 5;
        } else {
            b[0] = 0xcf;        /* uint 64 */
            b[1] = (n & 0xff00000000000000LL) >> 56;
            b[2] = (n & 0xff000000000000LL) >> 48;
            b[3] = (n & 0xff0000000000LL) >> 40;
            b[4] = (n & 0xff00000000LL) >> 32;
            b[5] = (n & 0xff000000) >> 24;
            b[6] = (n & 0xff0000) >> 16;
            b[7] = (n & 0xff00) >> 8;
            b[8] = n & 0xff;
            enclen = 9;
        }
    } else {
        if (n >= -32) {
            b[0] = ((signed char)n);   /* negative fixnum */
            enclen = 1;
        } else if (n >= -128) {
            b[0] = 0xd0;        /* int 8 */
            b[1] = n & 0xff;
            enclen = 2;
        } else if (n >= -32768) {
            b[0] = 0xd1;        /* int 16 */
            b[1] = (n & 0xff00) >> 8;
            b[2] = n & 0xff;
            enclen = 3;
        } else if (n >= -2147483648LL) {
            b[0] = 0xd2;        /* int 32 */
            b[1] = (n & 0xff000000) >> 24;
            b[2] = (n & 0xff0000) >> 16;
            b[3] = (n & 0xff00) >> 8;
            b[4] = n & 0xff;
            enclen = 5;
        } else {
            b[0] = 0xd3;        /* int 64 */
            b[1] = (n & 0xff00000000000000LL) >> 56;
            b[2] = (n & 0xff000000000000LL) >> 48;
            b[3] = (n & 0xff0000000000LL) >> 40;
            b[4] = (n & 0xff00000000LL) >> 32;
            b[5] = (n & 0xff000000) >> 24;
            b[6] = (n & 0xff0000) >> 16;
            b[7] = (n & 0xff00) >> 8;
            b[8] = n & 0xff;
            enclen = 9;
        }
    }
    mp_buf_append(buf,b,enclen);
}

void mp_encode_array(mp_buf *buf, int64_t n) {
    unsigned char b[5];
    int enclen;

    if (n <= 15) {
        b[0] = 0x90 | (n & 0xf);    /* fix array */
        enclen = 1;
    } else if (n <= 65535) {
        b[0] = 0xdc;                /* array 16 */
        b[1] = (n & 0xff00) >> 8;
        b[2] = n & 0xff;
        enclen = 3;
    } else {
        b[0] = 0xdd;                /* array 32 */
        b[1] = (n & 0xff000000) >> 24;
        b[2] = (n & 0xff0000) >> 16;
        b[3] = (n & 0xff00) >> 8;
        b[4] = n & 0xff;
        enclen = 5;
    }
    mp_buf_append(buf,b,enclen);
}

void mp_encode_map(mp_buf *buf, int64_t n) {
    unsigned char b[5];
    int enclen;

    if (n <= 15) {
        b[0] = 0x80 | (n & 0xf);    /* fix map */
        enclen = 1;
    } else if (n <= 65535) {
        b[0] = 0xde;                /* map 16 */
        b[1] = (n & 0xff00) >> 8;
        b[2] = n & 0xff;
        enclen = 3;
    } else {
        b[0] = 0xdf;                /* map 32 */
        b[1] = (n & 0xff000000) >> 24;
        b[2] = (n & 0xff0000) >> 16;
        b[3] = (n & 0xff00) >> 8;
        b[4] = n & 0xff;
        enclen = 5;
    }
    mp_buf_append(buf,b,enclen);
}

void mp_encode_mp(mp_buf *buf, mp_buf *buf2) {

}
