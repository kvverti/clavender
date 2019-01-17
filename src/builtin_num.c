#include "builtin_funcs.h"
#include "textbuffer.h"
#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <float.h>
#include <assert.h>

#define INTRINSIC(name) \
    TextBufferObj lv_num_##name(TextBufferObj* args)

INTRINSIC(str) {
    assert(args[0].type == OPT_NUMBER);
    TextBufferObj res;
    int len = snprintf(NULL, 0, "%g", args[0].number);
    LvString* str = lv_alloc(sizeof(LvString) + len + 1);
    str->refCount = 0;
    str->len = len;
    snprintf(str->value, len + 1, "%g", args[0].number);
    res.type = OPT_STRING;
    res.str = str;
    return res;
}

INTRINSIC(num) {
    assert(args[0].type == OPT_NUMBER);
    return args[0];
}

INTRINSIC(int) {
    assert(args[0].type == OPT_NUMBER);
    TextBufferObj res;
    double d = args[0].number;
    if(!isfinite(d)) {
        res.type = OPT_UNDEFINED;
    } else {
        int exp;
        int sign = signbit(d);
        double small = frexp(d, &exp);
        small = ldexp(small, DBL_MANT_DIG);
        small = copysign(small, 1.0);
        exp -= DBL_MANT_DIG;
        uint64_t sig = (uint64_t) small;
        if(exp < -63) {
            res.type = OPT_INTEGER;
            res.integer = 0;
        } else if(exp <= 0) {
            res.type = OPT_INTEGER;
            res.integer = sig >> -exp;
            if(sign) {
                res.integer = -res.integer;
            }
        } else {
            union Fbi {
                BigInt bi;
                char c[sizeof(BigInt) + sizeof(WordType)];
            } fbi;
            fbi.bi.refCount = 0;
            fbi.bi.len = 1;
            fbi.bi.data[0] = sign ? -sig : sig;
            BigInt* bi = yabi_lshift(&fbi.bi, exp);
            if(bi->len == 1) {
                res.type = OPT_INTEGER;
                res.integer = bi->data[0];
                lv_free(bi);
            } else {
                res.type = OPT_BIGINT;
                res.bigint = bi;
            }
        }
    }
    return res;
}

INTRINSIC(hash) {
    assert(args[0].type == OPT_NUMBER);
    TextBufferObj res;
    union U {
        double d;
        char c[sizeof(double)];
    } bytes = { args[0].number };
    uint64_t hash = 0;
    for(size_t i = 0; i < sizeof(double); i++) {
        hash = hash * 31 + bytes.c[i];
    }
    res.type = OPT_INTEGER;
    res.integer = hash;
    return res;
}

INTRINSIC(eq) {
    assert(args[0].type == OPT_NUMBER);
    assert(args[1].type == OPT_NUMBER);
    return (TextBufferObj) {
        .type = OPT_INTEGER,
        .integer = (args[0].number == args[1].number)
    };
}

INTRINSIC(lt) {
    assert(args[0].type == OPT_NUMBER);
    assert(args[1].type == OPT_NUMBER);
    return (TextBufferObj) {
        .type = OPT_INTEGER,
        .integer = (args[0].number < args[1].number)
    };
}

INTRINSIC(ge) {
    assert(args[0].type == OPT_NUMBER);
    assert(args[1].type == OPT_NUMBER);
    return (TextBufferObj) {
        .type = OPT_INTEGER,
        .integer = (args[0].number >= args[1].number)
    };
}

INTRINSIC(pos) {
    assert(args[0].type == OPT_NUMBER);
    return args[0];
}

INTRINSIC(neg) {
    assert(args[0].type == OPT_NUMBER);
    return (TextBufferObj) {
        .type = OPT_NUMBER,
        .number = -args[0].number
    };
}

INTRINSIC(add) {
    assert(args[0].type == OPT_NUMBER);
    TextBufferObj res;
    double b = args[1].type == OPT_NUMBER ? args[1].number : lv_int_num(&args[1]).number;
    res.type = OPT_NUMBER;
    res.number = args[0].number + b;
    return res;
}

INTRINSIC(sub) {
    assert(args[0].type == OPT_NUMBER);
    TextBufferObj res;
    double b = args[1].type == OPT_NUMBER ? args[1].number : lv_int_num(&args[1]).number;
    res.type = OPT_NUMBER;
    res.number = args[0].number - b;
    return res;
}

INTRINSIC(mul) {
    assert(args[0].type == OPT_NUMBER);
    TextBufferObj res;
    double b = args[1].type == OPT_NUMBER ? args[1].number : lv_int_num(&args[1]).number;
    res.type = OPT_NUMBER;
    res.number = args[0].number * b;
    return res;
}

INTRINSIC(div) {
    assert(args[0].type == OPT_NUMBER);
    TextBufferObj res;
    double a = args[0].number;
    double b = args[1].type == OPT_NUMBER ? args[1].number : lv_int_num(&args[1]).number;
    if(isfinite(a) && isfinite(b) && b != 0.0) {
        TextBufferObj tmp = {
            .type = OPT_NUMBER,
            .number = (a - fmod(a, b)) / b
        };
        res = lv_num_int(&tmp);
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

INTRINSIC(rdiv) {
    assert(args[0].type == OPT_NUMBER);
    TextBufferObj res;
    double a = args[0].number;
    double b = args[1].type == OPT_NUMBER ? args[1].number : lv_int_num(&args[1]).number;
    double q;
    if(b == 0.0) {
        // 0 / 0 = nan, a / 0 = +-inf
        int sign = (bool)signbit(a) ^ (bool)signbit(b);
        q = copysign(a == 0.0 ? NAN : INFINITY, -sign);
    } else {
        q = a / b;
    }
    res.type = OPT_NUMBER;
    res.number = q;
    return res;
}
