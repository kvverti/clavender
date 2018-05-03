#ifndef TOKEN_H
#define TOKEN_H
#include <stdio.h>

typedef enum TokenType {
    TTY_IDENT,          //simple alphanumeric
    TTY_QUAL_IDENT,     //qualified alphanumeric
    TTY_NUMBER,         //numeric (floating-point)
    TTY_SYMBOL,         //simple symbol
    TTY_QUAL_SYMBOL,    //qualified symbol
    TTY_FUNC_SYMBOL,    //symbolic function with prefix
    TTY_FUNC_VAL,       //function value
    TTY_QUAL_FUNC_VAL,  //qualified function value
    TTY_STRING,         //string
    TTY_LITERAL         //literal character
} TokenType;

typedef struct Token {
    TokenType type;
    char* value;
    struct Token* next;
} Token;

typedef enum TokenError {
    TE_BAD_QUAL = 1,    //bad qualified identifier
    TE_BAD_NUM,         //bad decimal format
    TE_BAD_EXP,         //bad exponent format
    TE_BAD_FUNC_VAL,    //bad function value
    TE_UNTERM_STR,      //unterminated string
    TE_BAD_STR_ESC      //bad string escape sequence
} TokenError;

TokenError LV_TKN_ERROR;

/**
 * Retrieves an error message for the specified error.
 */
char* lv_tkn_getError(TokenError err);

/**
 * Splits the input string into tokens.
 * Returns a linked list of tokens.
 * Sets LV_TOK_ERROR and returns NULL if an error occurred.
 */
Token* lv_tkn_split(FILE* input);

/**
 * Frees the memory used by the given Token list.
 */
void lv_tkn_free(Token* head);

#endif
