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
    OPT_BUILTIN,    //built in operator    
    OPT_LITERAL,    //literal value (not present in final code)
    OPT_FWD_DECL    //function declaration (not present in final code)
} OpType;

typedef enum Fixing {
    FIX_PRE = 'u',
    FIX_LEFT_IN = 'i',
    FIX_RIGHT_IN = 'r'
} Fixing;

/**
 * Lavender's built in string object.
 */
typedef struct LvString {
    size_t len;
    char value[];
} LvString;

typedef struct Param {
    //struct does not own memory
    //allocated to name
    char* name;
    bool byName;
} Param;

typedef struct LvFunc {
    int textOffset;
    int arity;
    Fixing fixing;
} LvFunc;

typedef struct LvFuncDecl {
    int arity;
    Fixing fixing;
    Param params[];
} LvFuncDecl;

/**
 * A struct that stores a Lavender function and its name.
 * These values are stored in a global hashtable.
 */
typedef struct Operator {
    char* name;
    OpType type;
    union {
        LvFunc func;
        LvFuncDecl* decl;
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
        LvFunc func;
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
    OPE_DUP_DECL        //inconsistent function declarations
} OpError;

OpError LV_OP_ERROR;

char* lv_op_getError(OpError error);

/**
 * Retrieves the operator with the given name.
 * Returns NULL if no such operator exists.
 */
Operator* lv_op_getOperator(char* name);

/**
 * Adds the operator to the hashtable.
 * Returns whether the operator was successfully added.
 * If the operator has no name, it is not added to the
 * table, but this function returns true.
 */
bool lv_op_addOperator(Operator* op);

/**
 * Removes the operator from the hashtable
 * with the given name. Returns whether
 * the operator was present.
 */
bool lv_op_removeOperator(char* name);

/**
 * Declares a function.
 * The tokens list must begin with a top-level
 * function definition. Returns a pointer to the
 * first token in the body of the function.
 * If an error occurs, sets LV_OP_ERROR
 * and returns NULL.
 */
Token* lv_op_declareFunction(Token* tokens);

/**
 * Parses the expression defined by the token sequence.
 * Returns a dynamically allocated array of
 * component operators to execute.
 */
TextBufferObj* lv_op_parseExpr(Token* tokens);

//called on lv_shutdown
void lv_op_onShutdown();

#endif
