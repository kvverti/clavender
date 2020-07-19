#ifndef TEXTBUFFER_H
#define TEXTBUFFER_H
#include "textbuffer_fwd.h"
#include "operator_fwd.h"
#include "hashtable.h"
#include "bigint.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Lavender's built in string object.
 */
struct LvString {
    size_t refCount;
    size_t len;
    char value[];
};

/**
 * A struct that stores a Lavender value. This may
 * be a number, string, function, etc. These values
 * are stored in the global text buffer.
 */
struct TextBufferObj {
    OpType type;
    OpType fromType;
    union {
        double number;
        uint64_t integer;
        BigInt* bigint;
        LvString* str;
        LvVect* vect;
        LvMap* map;
        int param;
        size_t symbIdx;
        Operator* func;
        struct {
            CaptureObj* capture;
            Operator* capfunc;
        };
        int callArity;
        int branchAddr;
        size_t addr;
        char literal;
        size_t* refCount; //aliases (dynamic obj)->refCount
    };
};

#define UNDEFINED_V ((TextBufferObj) { .type = OPT_UNDEFINED })

/**
 * Dynamically allocated capture arguments.
 * Captures keep a refCount of all the times they
 * are referred to. e.g. the stack and another capture
 * object.
 */
struct CaptureObj {
    size_t refCount;
    TextBufferObj value[];
};

/**
 * Vector object.
 */
struct LvVect {
    size_t refCount;
    size_t len;
    TextBufferObj data[];
};

typedef struct LvMapNode {
    size_t hash;
    TextBufferObj key;
    TextBufferObj value;
} LvMapNode;

/**
 * Map object. Maps are implemented using a lookup table sorted
 * according to the hash value of the key.
 */
struct LvMap {
    size_t refCount;
    size_t len;
    LvMapNode data[];
};

#endif
