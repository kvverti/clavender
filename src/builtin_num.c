#include "builtin_funcs.h"
#include "textbuffer.h"
#include <stdio.h>
#include <math.h>
#include <inttypes.h>
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
        char buffer[25]; // max length of %a string
        char sign;
        int normal;
        uint64_t mantissa;
        int exponent;
        sprintf(buffer, "%+a", d);
        sscanf(buffer, "%c 0x %d . %"SCNx64" p %d", &sign, &normal, &mantissa, &exponent);
        // to be continued...
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
