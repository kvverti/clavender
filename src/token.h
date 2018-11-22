#ifndef TOKEN_H
#define TOKEN_H
#include <stdio.h>

typedef enum TokenType {
    TTY_IDENT,          //simple alphanumeric
    TTY_QUAL_IDENT,     //qualified alphanumeric
    TTY_NUMBER,         //numeric (floating-point)
    TTY_INTEGER,        //numeric (integral)
    TTY_SYMBOL,         //simple symbol
    TTY_QUAL_SYMBOL,    //qualified symbol
    TTY_FUNC_SYMBOL,    //symbolic function with prefix
    TTY_FUNC_VAL,       //function value
    TTY_QUAL_FUNC_VAL,  //qualified function value
    TTY_STRING,         //string
    TTY_DOT_SYMB,       //.symbol value
    TTY_ELLIPSIS,       //vararg (vect capture) modifier
    TTY_EMPTY_ARGS,     //empty arg placeholder ()
    TTY_LITERAL         //literal character
} TokenType;

typedef struct Token {
    TokenType type;
    char* line;
    char* start;    //beginning of token text in source
    size_t len;     //length of token text in source
    int lineNumber;
    struct Token* next;
} Token;

typedef enum TokenError {
    TE_BAD_QUAL = 1,    //bad qualified identifier
    TE_BAD_NUM,         //bad decimal format
    TE_BAD_EXP,         //bad exponent format
    TE_BAD_FUNC_VAL,    //bad function value
    TE_UNTERM_STR,      //unterminated string
    TE_BAD_STR_ESC,     //bad string escape sequence
    TE_BAD_STR_CHR,     //bad string character
    TE_BAD_INT_PREFIX,  //bad numeric prefix
    TE_UNBAL_PAREN      //unbalanced parens
} TokenError;

TokenError LV_TKN_ERROR;
struct {
    char* line;
    int lineNumber;
    size_t startIdx;
    size_t endIdx;
} lv_tkn_errContext;

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

/**
 * Wrapper for strcmp with token names.
 */
int lv_tkn_cmp(Token* tok, char* val);

/**
 * Resets the internal line number count to 1.
 */
void lv_tkn_resetLine(void);

/**
 * Releases the memory allocated for the given source file.
 * Invalidates all tokens generated from the file.
 */
void lv_tkn_releaseFile(FILE* file);

void lv_tkn_onStartup(void);
void lv_tkn_onShutdown(void);

#endif
