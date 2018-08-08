#ifndef TEXT_BUFFER_FWD_H
#define TEXT_BUFFER_FWD_H
#include "operator_fwd.h"
#include "token.h"
#include <stddef.h>

//must be a power of two and greater than number of OpTypes
//this prevents us having to add a field to TextBufferObj
#define LV_DYNAMIC 32
typedef enum OpType {
    OPT_UNDEFINED,      //undefined value
    OPT_NUMBER,         //Lavender number
    OPT_PARAM,          //function parameter
    OPT_PUT_PARAM,      //store top in param
    OPT_FUNCTION,       //function definition
    OPT_FUNCTION_VAL,   //function value
    OPT_FUNC_CAP,       //capture function with params
    OPT_FUNC_CALL,      //call value as function
    OPT_MAKE_VECT,      //make vector from args
    OPT_RETURN,         //return from function
    OPT_BEQZ,           //relative branch if zero
    OPT_ADDR,           //internal address (not present in text buffer)
    OPT_LITERAL,        //literal value (not present in final code)
    OPT_EMPTY_ARGS,     //empty args placeholder (not present in final code)
    OPT_STRING =        //dynamic objects start here
        LV_DYNAMIC,     //Lavender string
    OPT_VECT,           //Lavender vector
    OPT_CAPTURE,        //function value with captured params
} OpType;

typedef struct TextBufferObj TextBufferObj;
typedef struct CaptureObj CaptureObj;
typedef struct LvString LvString;
typedef struct LvVect LvVect;

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
