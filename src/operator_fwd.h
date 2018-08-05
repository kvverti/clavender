#ifndef OPERATOR_FWD_H
#define OPERATOR_FWD_H
#include <stddef.h>
#include <stdbool.h>

typedef enum FuncType {
    FUN_FUNCTION,   //function definition
    FUN_BUILTIN,    //built in operator
    FUN_FWD_DECL    //function declaration (not present in final code)
} FuncType;

typedef enum Fixing {
    FIX_PRE = 'u',
    FIX_LEFT_IN = 'i',
    FIX_RIGHT_IN = 'r'
} Fixing;

typedef enum FuncNamespace {
    FNS_PREFIX = 0,
    FNS_INFIX,
    FNS_COUNT       //number of namespaces
} FuncNamespace;

typedef struct Param Param;
typedef struct Operator Operator;

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
