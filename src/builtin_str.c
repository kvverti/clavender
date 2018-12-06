#include "builtin_funcs.h"
#include "textbuffer.h"
#include "lavender.h"
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>

#define INTRINSIC(name) \
    TextBufferObj lv_str_##name(TextBufferObj* args)

INTRINSIC(str) {
    assert(args[0].type == OPT_STRING);
    return args[0];
}

INTRINSIC(num) {
    assert(args[0].type == OPT_STRING);
    TextBufferObj res;
    char* rest;
    LvString* str = args[0].str;
    double d = strtod(str->value, &rest);
    if(rest != str->value + str->len) {
        //not all chars interpreted, error
        res.type = OPT_UNDEFINED;
    } else {
        res.type = OPT_NUMBER;
        res.number = d;
    }
    return res;
}

INTRINSIC(int) {
    assert(args[0].type == OPT_STRING);
    TextBufferObj res;
    char* rest;
    LvString* str = args[0].str;
    //unsigned negation is the same as two's complement negation
    uint64_t i64 = (uint64_t) strtoumax(str->value, &rest, 10);
    if(rest != str->value + str->len) {
        //not all chars interpreted
        res.type = OPT_UNDEFINED;
    } else {
        res.type = OPT_INTEGER;
        res.integer = i64;
    }
    return res;
}

INTRINSIC(cat) {
    assert(args[0].type == OPT_STRING);
    TextBufferObj res;
    if(args[1].type == OPT_STRING) {
        //string concatenation
        size_t alen = args[0].str->len;
        size_t blen = args[1].str->len;
        LvString* str = lv_alloc(sizeof(LvString) + alen + blen + 1);
        str->refCount = 0;
        str->len = alen + blen;
        memcpy(str->value, args[0].str->value, alen);
        memcpy(str->value + alen, args[1].str->value, blen);
        str->value[alen + blen] = '\0';
        res.type = OPT_STRING;
        res.str = str;
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

INTRINSIC(at) {
    assert(args[1].type == OPT_STRING);
    TextBufferObj res = { .type = OPT_UNDEFINED };
    if(args[0].type == OPT_INTEGER
        && !NEGATIVE_INT(args[0].integer)
        && args[0].integer < args[1].str->len) {
        res.type = OPT_STRING;
        res.str = lv_alloc(sizeof(LvString) + 2);
        res.str->refCount = 0;
        res.str->len = 1;
        res.str->value[0] = args[1].str->value[(size_t)args[0].integer];
        res.str->value[1] = '\0';
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

INTRINSIC(eq) {
    assert(args[0].type == OPT_STRING);
    TextBufferObj res;
    res.type = OPT_INTEGER;
    if(args[1].type == OPT_STRING) {
        res.integer = (args[0].str->len == args[1].str->len)
            && (strcmp(args[0].str->value, args[1].str->value) == 0);
    } else {
        res.integer = 0;
    }
    return res;
}

INTRINSIC(lt) {
    assert(args[0].type == OPT_STRING);
    TextBufferObj res;
    res.type = OPT_INTEGER;
    if(args[1].type == OPT_STRING) {
        res.integer = (strcmp(args[0].str->value, args[1].str->value) < 0);
    } else {
        res.integer = (args[0].type < args[1].type);
    }
    return res;
}

INTRINSIC(len) {
    assert(args[0].type == OPT_STRING);
    return (TextBufferObj) {
        .type = OPT_INTEGER,
        .integer = args[0].str->len
    };
}
