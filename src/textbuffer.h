#ifndef TEXTBUFFER_H
#define TEXTBUFFER_H
#include "textbuffer_fwd.h"
#include "operator_fwd.h"
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
    union {
        double number;
        uint64_t integer;
        LvString* str;
        LvVect* vect;
        int param;
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

#endif
