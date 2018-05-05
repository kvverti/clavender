#ifndef OPERATOR_H
#define OPERATOR_H
#include "token.h"
#include <stddef.h>
#include <stdbool.h>

typedef enum OpType {
    OPT_NUMBER,     //Lavender number
    OPT_STRING,     //Lavender string
    OPT_PARAM,      //function parameter
    OPT_FUNCTION,   //function definition
    OPT_LITERAL,    //literal value (not present in final code)
} OpType;

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

/**
 * Lavender's built in string object.
 */
typedef struct LvString {
    size_t len;
    char value[];
} LvString;

typedef struct Param {
    char* name;
    bool byName;
} Param;

/**
 * A struct that stores a Lavender function and its name.
 * These values are stored in a global hashtable.
 */
typedef struct Operator {
    char* name;
    FuncType type;
    int arity;
    Fixing fixing;
    int captureCount;
    union {
        int textOffset;
        Param* params;
        //todo builtin type
    };
    struct Operator* next;
} Operator;

/**
 * A struct that stores a Lavender value. This may
 * be a number, string, function, etc. These values
 * are stored in the global text buffer.
 */
typedef struct TextBufferObj {
    OpType type;
    union {
        double number;
        LvString* str;
        int param;
        Operator* func;
        //todo builtin type
        char literal;
    };
} TextBufferObj;

typedef enum OpError {
    OPE_NOT_FUNCT = 1,  //expr does not define a function
    OPE_UNTERM_EXPR,    //unterminated expression
    OPE_EXPT_ARGS,      //expected an argument list
    OPE_BAD_ARGS,       //bad argument list
    OPE_MISSING_BODY,   //missing function body
    OPE_DUP_DECL,       //inconsistent function declarations
    OPE_NAME_NOT_FOUND, //simple name not found
    OPE_EXPECT_INF,     //operator expected
    OPE_EXPECT_PRE,     //operand expected
    OPE_UNEXPECT_TOKEN  //unexpected token
} OpError;

OpError LV_OP_ERROR;

char* lv_op_getError(OpError error);

/**
 * Retrieves the operator with the given name.
 * Returns NULL if no such operator exists.
 */
Operator* lv_op_getOperator(char* name, FuncNamespace ns);

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
 * Declares a function.
 * The tokens list must begin with a top-level
 * function definition. Sets bodyTok to point to the
 * first token in the body of the function and returns
 * the declaration object created.
 * If an error occurs, sets LV_OP_ERROR
 * and returns NULL.
 */
Operator* lv_op_declareFunction(Token* tokens, char* nspace, Token** bodyTok);

/**
 * Parses the expression defined by the token sequence.
 * in the context of the given function declaration.
 * Returns a dynamically allocated array of
 * component operators to execute.
 */
void lv_op_parseExpr(Token* tokens, Operator* decl, TextBufferObj** expr, size_t* len);

void lv_op_onStartup();
//called on lv_shutdown
void lv_op_onShutdown();

#endif
