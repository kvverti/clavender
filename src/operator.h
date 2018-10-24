#ifndef OPERATOR_H
#define OPERATOR_H
#include "operator_fwd.h"
#include "textbuffer_fwd.h"
#include "token.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// the maximum number of params a Lavender function may have
#define MAX_PARAMS 256

struct Param {
    char* name;
    Token* initializer;
    bool byName;
};

typedef TextBufferObj (*Builtin)(TextBufferObj*);

/**
 * A struct that stores a Lavender function and its name.
 * These values are stored in a global hashtable.
 */
struct Operator {
    char* name;
    FuncType type;
    int arity;
    Fixing fixing;
    int captureCount;
    int locals;
    union {
        int textOffset;
        Param* params;
        Builtin builtin;
    };
    Operator* enclosing;
    Operator* next; //used by anonFuncs
    uint8_t byName[MAX_PARAMS / 8]; //bitfield for by-name parameters
    bool varargs;
};

#define LV_SET_BYNAME(op, idx) \
    ((op)->byName[(idx) / 8] |= (1 << ((idx) % 8)))
#define LV_GET_BYNAME(op, idx) \
    (((op)->byName[(idx) / 8] >> ((idx) % 8)) & 1)

/**
 * Retrieves the operator with the given name.
 * Returns NULL if no such operator exists.
 */
Operator* lv_op_getOperator(char* name, FuncNamespace ns);

/**
 * Retrieves the operator with the given name
 * in the given scope. Returns NULL if no such operator exists.
 */
Operator* lv_op_getScopedOperator(char* scope, char* name, FuncNamespace ns);

/**
 * Adds the operator to the hashtable.
 * Returns whether the operator was successfully added.
 * If the operator has no name, it is not added to the
 * table, but this function returns true.
 */
bool lv_op_addOperator(Operator* op, FuncNamespace ns);

/**
 * Removes the operator from the hashtable
 * with the given name. Returns whether
 * the operator was present.
 */
bool lv_op_removeOperator(char* name, FuncNamespace ns);

/**
 * Retrieves all operators in the specified scope.
 */

void lv_op_onStartup(void);
//called on lv_shutdown
void lv_op_onShutdown(void);

#endif
