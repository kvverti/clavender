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
        //todo builtin type
        char literal;
    };
} TextBufferObj;

/**
 * Returns a Lavender string representation of the
 * given object.
 */
LvString* lv_tb_getString(TextBufferObj* obj);

#endif
