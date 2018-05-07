#ifndef TEXTBUFFER_H
#define TEXTBUFFER_H
#include "operator.h"
#include <stddef.h>

typedef enum OpType {
    OPT_NUMBER,         //Lavender number
    OPT_STRING,         //Lavender string
    OPT_PARAM,          //function parameter
    OPT_FUNCTION,       //function definition
    OPT_FUNCTION_VAL,   //function value
    OPT_FUNC_CALL,      //call value as function
    OPT_LITERAL,        //literal value (not present in final code)
} OpType;

/**
 * Lavender's built in string object.
 */
typedef struct LvString {
    size_t len;
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
        //todo builtin type
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
Token* lv_tb_defineFunction(Token* tokens, char* scope, Operator** res);

void lv_tb_onStartup();
void lv_tb_onShutdown();

#endif
