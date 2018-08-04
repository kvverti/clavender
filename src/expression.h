#ifndef EXPRESSION_H
#define EXPRESSION_H
#include "textbuffer.h"

typedef enum ExprError {
    XPE_NOT_FUNCT = 1,  //expr does not define a function
    XPE_UNTERM_EXPR,    //unterminated expression
    XPE_EXPT_ARGS,      //expected an argument list
    XPE_BAD_ARGS,       //bad argument list
    XPE_MISSING_BODY,   //missing function body
    XPE_DUP_DECL,       //duplicate function definitions
    XPE_NAME_NOT_FOUND, //simple name not found
    XPE_EXPECT_INF,     //operator expected
    XPE_EXPECT_PRE,     //operand expected
    XPE_UNEXPECT_TOKEN, //unexpected token
    XPE_UNBAL_GROUP,    //unbalanced parens or brackets
    XPE_BAD_ARITY,      //wrong number of params to function
    XPE_BAD_FIXING,     //arity and fixing not compatible
    XPE_BAD_LOCALS,     //bad function local list
} ExprError;

ExprError LV_EXPR_ERROR;

char* lv_expr_getError(ExprError error);

/**
 * Declares a function.
 * The tokens list must begin with a top-level
 * function definition. Sets bodyTok to point to the
 * first token in the body of the function and returns
 * the declaration object created.
 * If an error occurs, sets LV_EXPR_ERROR
 * and returns NULL.
 */
Operator* lv_expr_declareFunction(Token* tokens, Operator* nspace, Token** bodyTok);

/**
 * Parses the expression defined by the token sequence.
 * in the context of the given function declaration.
 * Stores a dynamically allocated list of components in
 * the parameter res and returns the first token after the
 * expression, or NULL if all tokens were consumed.
 * If an error occurs, sets LV_EXPR_ERROR and returns NULL.
 */
Token* lv_expr_parseExpr(Token* tokens, Operator* decl, TextBufferObj** res, size_t* len);

/**
 * Calls lv_expr_cleanup and additionally frees obj.
 */
void lv_expr_free(TextBufferObj* obj, size_t len);

/**
 * Frees data associated with the objects given.
 */
void lv_expr_cleanup(TextBufferObj* obj, size_t len);

#endif
