#ifndef TEXTBUFFER_H
#define TEXTBUFFER_H
#include "operator.h"
#include <stddef.h>

typedef enum OpType {
    OPT_UNDEFINED,      //undefined value
    OPT_NUMBER,         //Lavender number
    OPT_STRING,         //Lavender string
    OPT_PARAM,          //function parameter
    OPT_FUNCTION,       //function definition
    OPT_FUNCTION_VAL,   //function value
    OPT_FUNC_CALL,      //call value as function
    OPT_RETURN,         //return from function
    OPT_BEQZ,           //relative branch if zero
    OPT_ADDR,           //internal address (not present in text buffer)
    OPT_LITERAL,        //literal value (not present in final code)
} OpType;

/**
 * Lavender's built in string object.
 */
typedef struct LvString {
    size_t len;
    size_t refCount;
    char value[];
} LvString;

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
        int callArity;
        int branchAddr;
        size_t addr;
        char literal;
    };
} TextBufferObj;

/**
 * The global buffer where all Lavender function
 * code is stored.
 */
TextBufferObj* TEXT_BUFFER;

/**
 * Returns a Lavender string representation of the
 * given object.
 */
LvString* lv_tb_getString(TextBufferObj* obj);

/**
 * Defines the function described by the given token
 * sequence in the given scope. Returns a pointer to
 * the first unprocessed token in tokens, or NULL if
 * all tokens were processed. If res is not NULL,
 * stores the created function in res.
 */
Token* lv_tb_defineFunction(Token* tokens, Operator* scope, Operator** res);

/**
 * Given an existing function declaration and a pointer to the body,
 * define the function with the body. Returns a pointer to the first
 * unprocessed token, or NULL if all tokens were processed.
 */
Token* lv_tb_defineFunctionBody(Token* tokens, Operator* decl);

/**
 * Parses the given expression and adds it to the text buffer temporarily.
 * The start index of the expression is returned through out param startIdx.
 * If an error occurs, LV_EXPR_ERROR is set and this function returns NULL,
 * otherwise this function returns the next token in the sequence after the 
 * expression. The next call of lv_tb_clearExpr frees the data for this expression.
 */
Token* lv_tb_parseExpr(Token* tokens, Operator* scope, size_t* start, size_t* end);

/**
 * Clears the text buffer of any data associated with the previous parsed expression.
 */
void lv_tb_clearExpr(void);

void lv_tb_onStartup(void);
void lv_tb_onShutdown(void);

#endif
