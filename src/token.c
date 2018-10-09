#include "token.h"
#include "lavender.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

// guess for typical line length in chars
#define INIT_LINE_LENGTH 128

static TokenType tryGetFuncSymb(void);
static TokenType tryGetQualName(void);
static TokenType tryGetEllipsis(void);
static TokenType tryGetEmptyArgs(void);
static TokenType getSymbol(void);
static TokenType getString(void);
static TokenType getFuncVal(void);
static TokenType getNumber(void);
static TokenType getLiteral(void);

char* lv_tkn_getError(TokenError err) {

    #define LEN 8
    static char* msg[LEN] = {
        "Namespace without name",
        "Number ends in '.'",
        "Number has missing exponent",
        "Missing function value",
        "Unterminated string",
        "Unknown string escape sequence",
        "Invalid character in string",
        "Unbalanced parentheses"
    };
    assert(err > 0 && err <= LEN);
    return msg[err - 1];
    #undef LEN
}

void lv_tkn_free(Token* head) {

    while(head) {
        Token* tail = head->next;
        lv_free(head);
        head = tail;
    }
}

typedef struct SourceFileLine {
    FILE* file;
    int line;
    struct SourceFileLine* next;
    char data[];
} SourceFileLine;

static bool inputEnd = false;
static size_t BUFFER_LEN;
static char* buffer; //alias for currentFileLine->data
static int bgn; //start pos of the current token
static int idx; //current index in the buffer
static FILE* input;
static int bracketNesting; //bracket nesting
static int parenNesting; //paren nesting
static int braceNesting; //curly brace nesting
static int currentLine = 1;
static SourceFileLine* currentFileLine = NULL;

void lv_tkn_releaseFile(FILE* file) {

    SourceFileLine** line = &currentFileLine;
    while(*line) {
        SourceFileLine** next = &(*line)->next;
        if((*line)->file == file) {
            lv_free(*line);
            *line = *next;
        }
        line = next;
    }
}

void lv_tkn_onStartup(void) { }

void lv_tkn_onShutdown(void) {

    SourceFileLine* line = currentFileLine;
    while(line) {
        SourceFileLine* next = line->next;
        lv_free(line);
        line = next;
    }
}

void lv_tkn_resetLine(void) {
    currentLine = 1;
}

static void setInputEnd(void) {
    //set inputEnd for the global buffer
    bool endOfLine = (parenNesting == 0
        && bracketNesting == 0
        && braceNesting == 0
        && buffer[0]);
    inputEnd = (feof(input) || endOfLine);
}

static int issymb(int c) {

    static char* symbols = "~!%^&*-+=|<>/?:";
    return strchr(symbols, c) != NULL;
}

static int isidbgn(int c) {

    static char* chars =
        "QWERTYUIOPASDFGHJKLZXCVBNM"
        "qwertyuiopasdfghjklzxcvbnm"
        "_";
    return strchr(chars, c) != NULL;
}

static int isident(int c) {

    return isidbgn(c) || isdigit(c);
}

static int isdot(int c) {

    return c == '.';
}

//loops over the input while the passed
//predicate is satisfied and there is input.
static void getInputWhile(int (*pred)(int)) {

    do {
        idx++;
        if(!buffer[idx]) {
            //no more input
            break;
        }
    } while(pred(buffer[idx]));
}

// TODO free the previous line if it contained no tokens
static void nextLine(void) {

    if(inputEnd) {
        return;
    }
    size_t lineLength = 0;
    SourceFileLine* line = NULL;
    size_t len = 0;
    char* buf; //beginning of substring
    char* nul; //end of substring
    do {
        //reallocate buffer to get the rest of the line
        lineLength += INIT_LINE_LENGTH;
        line = lv_realloc(line, sizeof(SourceFileLine) + lineLength);
        buf = line->data + len;
        assert(len < lineLength);
        memset(buf, 0, lineLength - len);
        fgets(buf, lineLength - len, input);
        //if there was a NUL character in the text stream that got added into
        //the buffer, the lexing code will get confused about the amount of
        //text read. To remedy this, we find NUL characters that are not at
        //the end of the buffer and replace them with spaces.
        //A valid NUL character in the buffer is occurs when
        //  a) the input reaches end of file
        //  b) the NUL occurs at the end of the buffer
        //  c) the NUL occurs after a newline character
        //Any other occurrences of NUL are invalid and must be purged.
        nul = strchr(buf, '\0');
        while(!feof(input) && nul != (buf + lineLength - len - 1) && (nul == buf || nul[-1] != '\n')) {
            if(lv_debug)
                printf("TOKEN: Stray NUL at position %lu of %lu (%s)\n", nul - buf, lineLength - len, buf);
            *nul = ' ';
            nul = strchr(nul, '\0');
        }
        len = nul - line->data;
    } while(nul != buf && nul[-1] != '\n');
    line->file = input;
    line->line = currentLine++;
    line->next = currentFileLine;
    currentFileLine = line;
    buffer = line->data;
    bgn = idx = 0;
    setInputEnd();
}

Token* lv_tkn_split(FILE* in) {

    if(LV_TKN_ERROR)
        return NULL;
    //reset static vars
    input = in;
    inputEnd = false;
    BUFFER_LEN = 64;
    bgn = idx = parenNesting = bracketNesting = braceNesting = 0;

    Token* head = NULL;
    Token* tail = head;
    nextLine();
    while(buffer[bgn]) {
        char c = buffer[bgn];
        TokenType type = -1;
        //check for comment
        if(c == '\'') {
            //skip the rest of the line
            idx = strlen(buffer + bgn) + bgn;
        } else if(c == '\n') {
            idx++;
            nextLine();
        } else if(isspace(c)) {
            idx++; //eat spaces
        } else if(isidbgn(c)) {
            type = tryGetFuncSymb();
        } else if(issymb(c)) {
            type = getSymbol();
        } else if(isdigit(c)) {
            type = getNumber();
        } else if(c == '.') {
            type = tryGetEllipsis();
        } else if(c == '\\') {
            type = getFuncVal();
        } else if(c == '"') {
            type = getString();
        } else if(c == '(') {
            type = tryGetEmptyArgs();
        } else {
            //literal token
            type = getLiteral();
        }
        //check error
        if(LV_TKN_ERROR) {
            //get the last few chars before the error in the cxt buffer
            int len;
            {
                int a = idx + 1, b = TKN_ERRCXT_LEN - 1;
                len = a < b ? a : b;
            }
            memcpy(lv_tkn_errcxt, buffer + idx + 1 - len, len);
            lv_tkn_errcxt[len] = '\0';
            lv_tkn_free(head);
            return NULL;
        }
        if(type != -1) {
            //create token
            Token* tok = lv_alloc(sizeof(Token) + (idx - bgn + 1));
            tok->type = type;
            tok->lineNumber = currentFileLine->line;
            tok->line = currentFileLine->data;
            tok->start = buffer + bgn;
            tok->len = idx - bgn;
            tok->next = NULL;
            memcpy(tok->value, buffer + bgn, idx - bgn);
            tok->value[idx - bgn] = '\0';
            //append to list
            if(tail)
                tail->next = tok;
            else //set the head
                head = tok;
            tail = tok;
        }
        //move to next token
        bgn = idx;
        if(!buffer[bgn]) {
            nextLine();
        }
    }
    return head;
}

static TokenType getLiteral(void) {

    switch(buffer[idx]) {
        case '(': parenNesting++;
            break;
        case ')': parenNesting--;
            break;
        case '[': bracketNesting++;
            break;
        case ']': bracketNesting--;
            break;
        case '{': braceNesting++;
            break;
        case '}': braceNesting--;
            break;
    }
    if(parenNesting < 0 || bracketNesting < 0 || braceNesting < 0) {
        LV_TKN_ERROR = TE_UNBAL_PAREN;
        return -1;
    }
    setInputEnd();
    idx++;
    return TTY_LITERAL;
}

//returns the index of the next unprocessed char
static TokenType tryGetFuncSymb(void) {
    //  TTY_FUNC_SYMBOL
    //fallback to
    //  TTY_IDENT
    //  TTY_QUAL_IDENT
    //  TTY_QUAL_SYMBOL
    assert(buffer[idx]);
    char c = buffer[idx];
    //possibly a TTY_FUNC_SYMBOL
    if((c == 'u' || c == 'i' || c == 'r')) {
        idx++;
        if(!buffer[idx]){
            //can't be TTY_FUNC_SYMBOL
            idx = bgn;
            return tryGetQualName();
        }
        if(buffer[idx] == '_') {
            idx++;
            if(!buffer[idx]){
                //can't be TTY_FUNC_SYMBOL
                idx = bgn;
                return tryGetQualName();
            }
            if(issymb(buffer[idx])) {
                //definitely a TTY_FUNC_SYMBOL
                getInputWhile(issymb);
                return TTY_FUNC_SYMBOL;
            }
        }
    }
    //not a TTY_FUNC_SYMBOL
    idx = bgn;
    return tryGetQualName();
}

static TokenType tryGetQualName(void) {
    //  TTY_IDENT
    //fallback to
    //  TTY_QUAL_SYMBOL
    //  TTY_QUAL_IDENT
    assert(buffer[bgn]);
    assert(isidbgn(buffer[idx]));
    //assume TTY_IDENT for now
    TokenType type = TTY_IDENT;
    //get all identifier chars
    getInputWhile(isident);
    //check for qualified name
    if(buffer[idx] == ':') {
        //qualified name
        idx++;
        if(!buffer[idx]) {
            //invalid. set error and return
            LV_TKN_ERROR = TE_BAD_QUAL;
            return -1;
        }
        if(isidbgn(buffer[idx])) {
            //alphanumeric name
            getInputWhile(isident);
            type = TTY_QUAL_IDENT;
        } else if(issymb(buffer[idx])) {
            //symbolic name
            getInputWhile(issymb);
            type = TTY_QUAL_SYMBOL;
        } else {
            //error
            LV_TKN_ERROR = TE_BAD_QUAL;
            return -1;
        }
    }
    return type;
}

static TokenType tryGetEllipsis(void) {

    assert(buffer[idx] == '.');
    getInputWhile(isdot);
    if((idx - bgn) == 3)
        return TTY_ELLIPSIS;
    //isn't ellipsis, must be number
    idx = bgn;
    return getNumber();
}

static TokenType tryGetEmptyArgs(void) {

    assert(buffer[idx] == '(');
    idx++;
    if(buffer[idx] && buffer[idx] == ')') {
        idx++;
        return TTY_EMPTY_ARGS;
    } else {
        idx = bgn;
        return getLiteral();
    }
}

static TokenType getSymbol(void) {

    assert(issymb(buffer[idx]));
    getInputWhile(issymb);
    return TTY_SYMBOL;
}

static TokenType getNumber(void) {

    if(isdigit(buffer[idx])) {
        //start with whole number
        getInputWhile(isdigit);
        //optional decimal
        if(buffer[idx] == '.') {
            idx++;
            if((!buffer[idx])
                || !isdigit(buffer[idx])) {
                //the next char is not a digit
                //can't end a number in a decimal point
                LV_TKN_ERROR = TE_BAD_NUM;
                return -1;
            }
            getInputWhile(isdigit);
        } else {
            //no decimal -> integral value
            return TTY_INTEGER;
        }
    } else {
        //required decimal
        assert(buffer[idx] == '.');
        idx++;
        if((!buffer[idx])
            || !isdigit(buffer[idx])) {
            //the next char is not a digit
            //can't end a number in a decimal point
            LV_TKN_ERROR = TE_BAD_NUM;
            return 0;
        }
        getInputWhile(isdigit);
    }
    //optional exponent
    if(buffer[idx] == 'e' || buffer[idx] == 'E') {
        idx++;
        if(!buffer[idx]) {
            //can't end in 'e'
            LV_TKN_ERROR = TE_BAD_EXP;
            return -1;
        }
        if(buffer[idx] == '+' || buffer[idx] == '-') {
            //signs are ok
            idx++;
            if(!buffer[idx]) {
                //but not at the end of the number
                LV_TKN_ERROR = TE_BAD_EXP;
                return -1;
            }
        }
        if(!isdigit(buffer[idx])) {
            //we need digits in the exponent, people!
            LV_TKN_ERROR = TE_BAD_EXP;
            return -1;
        }
        getInputWhile(isdigit);
    }
    return TTY_NUMBER;
}

static TokenType getFuncVal(void) {

    assert(buffer[idx] == '\\');
    idx++;
    TokenType res;
    if(!buffer[idx]) {
        //who is inputing these lone backslashes?
        LV_TKN_ERROR = TE_BAD_FUNC_VAL;
        return -1;
    }
    if(issymb(buffer[idx])) {
        //plain symbolic
        getInputWhile(issymb);
        res = TTY_FUNC_VAL;
    } else if(isidbgn(buffer[idx])) {
        //non-symbolic
        TokenType tmp = tryGetQualName();
        if(LV_TKN_ERROR) {
            return -1;
        }
        if(tmp == TTY_QUAL_IDENT || tmp == TTY_QUAL_SYMBOL)
            res = TTY_QUAL_FUNC_VAL;
        else
            res = TTY_FUNC_VAL;
    } else {
        //again with the lone backslashes!!
        LV_TKN_ERROR = TE_BAD_FUNC_VAL;
        return -1;
    }
    //optional trailing backslash
    if(buffer[idx] == '\\')
        idx++;
    //don't need to reallocate since it's the end :)
    return res;
}

static TokenType getString(void) {

    //idx = bgn;
    assert(buffer[idx] == '"');
    do {
        if(buffer[idx] == '\\') {
            //check escape sequences
            idx++; //NUL check covered by default case
            //note: strchr allows the NUL character; this does not
            switch(buffer[idx]) {
                case 'n':
                case 't':
                case '\'':
                case '"':
                case '\\':
                    idx++;
                    if(!buffer[idx]) {
                        LV_TKN_ERROR = TE_UNTERM_STR;
                        return -1;
                    }
                    break;
                default:
                    LV_TKN_ERROR = TE_BAD_STR_ESC;
                    return -1;
            }
        } else {
            if(buffer[idx] == '\n') {
                //these characters not allowed in strings
                LV_TKN_ERROR = TE_BAD_STR_CHR;
                return -1;
            }
            idx++;
            if(!buffer[idx]) {
                //unterminated string
                LV_TKN_ERROR = TE_UNTERM_STR;
                return -1;
            }
        }
    } while(buffer[idx] != '"');
    //consume the closing quote; don't need to realloc the buffer
    idx++;
    return TTY_STRING;
}
