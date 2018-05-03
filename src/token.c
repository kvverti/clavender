#include "token.h"
#include "lavender.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

static Token* tryGetFuncSymb();
static Token* tryGetQualName();
static Token* getSymbol();
static Token* getString();
static Token* getFuncVal();
static Token* getNumber();
static Token* getLiteral();

char* lv_tkn_getError(TokenError err) {
    
    #define LEN 6
    static char* msg[LEN] = {
        "Namespace without name",
        "Number ends in '.'",
        "Number has missing exponent",
        "Missing function value",
        "Unterminated string",
        "Unknown string escape sequence"
    };
    assert(err > 0 && err <= LEN);
    return msg[err - 1];
    #undef LEN
}

void lv_tkn_free(Token* head) {
    
    while(head) {
        Token* tail = head->next;
        free(head->value);
        free(head);
        head = tail;
    }
}

static bool inputEnd = false;
static size_t BUFFER_LEN;
static char* buffer;
static int bgn; //start pos of the current token
static int idx; //current index in the buffer
static FILE* input;

static bool reallocBuffer();

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

//for line comments
static int isnotnl(int c) {
    
    return c != '\n';
}

//loops over the input while the passed
//predicate is satisfied and there is input.
static void getInputWhile(int (*pred)(int)) {
    
    do {
        idx++;
        if(!buffer[idx] && !reallocBuffer()) {
            //no more input
            break;
        }
    } while(pred(buffer[idx]));
}

static char* extractValue() {
    
    char* res = lv_alloc(idx - bgn + 1);
    memcpy(res, buffer + bgn, idx - bgn);
    res[idx - bgn] = '\0';
    return res;
}

//reallocates the buffer with the start of the buffer
//at 'bgn'. If bgn == 0, also increases the buffer size.
//returns whether the buffer was reallocated.
static bool reallocBuffer() {
    
    assert(bgn >= 0 && bgn < BUFFER_LEN);
    if(inputEnd)
        return false;
    if(bgn) {
        //copy elements down
        size_t len = strlen(buffer);
        for(int i = bgn; i < len; i++)
            buffer[i - bgn] = buffer[i];
        fgets(buffer + (len - bgn), BUFFER_LEN - (len - bgn), input);
    } else {
        //reallocate the whole buffer
        char* tmp = realloc(buffer, BUFFER_LEN * 2);
        if(!tmp) {
            free(buffer);
            lv_shutdown();
        }
        buffer = tmp;
        memset(buffer + BUFFER_LEN, 0, BUFFER_LEN); //initialize new memory
        //subtract 1 for the NUL terminator
        fgets(buffer + BUFFER_LEN - 1, BUFFER_LEN + 1, input);
        if(lv_debug)
            printf("TOKEN: buffer resize, old=%lu new=%lu\n",
                BUFFER_LEN,
                BUFFER_LEN * 2);
        BUFFER_LEN *= 2;
    }
    inputEnd = (feof(input) || (buffer[0] && (buffer[strlen(buffer) - 1] == '\n')));
    idx -= bgn;
    bgn = 0;
    return true;
}

Token* lv_tkn_split(FILE* in) {
    
    if(LV_TKN_ERROR)
        return NULL;
    //reset static vars
    input = in;
    inputEnd = false;
    BUFFER_LEN = 6;
    buffer = lv_alloc(BUFFER_LEN);
    memset(buffer, 0, BUFFER_LEN); //initialize buffer
    bgn = 0;
    
    Token* head = NULL;
    Token* tail = head;
    fgets(buffer, BUFFER_LEN, input);
    inputEnd = (feof(input) || (buffer[0] && (buffer[strlen(buffer) - 1] == '\n')));
    while(buffer[bgn]) {
        char c = buffer[bgn];
        Token* tok = NULL;
        //check for comment
        if(c == '\'') {
            //increment until next newline
            idx = bgn;
            getInputWhile(isnotnl);
            bgn = idx;
        } else if(isspace(c)) {
            bgn++; //eat spaces
        } else if(isidbgn(c)) {
            tok = tryGetFuncSymb();
        } else if(issymb(c)) {
            tok = getSymbol();
        } else if(isdigit(c) || c == '.') {
            tok = getNumber();
        } else if(c == '\\') {
            tok = getFuncVal();
        } else if(c == '"') {
            tok = getString();
        } else {
            //literal token
            tok = getLiteral();
        }
        //check error
        if(LV_TKN_ERROR) {
            assert(!tok);
            free(head);
            free(buffer);
            return NULL;
        }
        if(tok) {
            //append to list
            if(tail)
                tail->next = tok;
            else //set the head
                head = tok;
            tail = tok;
        }
        if(!buffer[bgn]) {
            reallocBuffer();
        }
    }
    free(buffer);
    return head;
}

static Token* getLiteral() {
    
    Token* tok = lv_alloc(sizeof(Token));
    tok->type = TTY_LITERAL;
    tok->next = NULL;
    tok->value = lv_alloc(2);
    tok->value[0] = buffer[bgn];
    tok->value[1] = '\0'; //NUL terminator
    bgn++;
    return tok;
}

//returns the index of the next unprocessed char
static Token* tryGetFuncSymb() {
    //  TTY_FUNC_SYMBOL
    //fallback to
    //  TTY_IDENT
    //  TTY_QUAL_IDENT
    //  TTY_QUAL_SYMBOL
    idx = bgn;
    assert(buffer[idx]);
    char c = buffer[idx];
    //possibly a TTY_FUNC_SYMBOL
    if((c == 'u' || c == 'i' || c == 'r')) {
        idx++;
        if(!buffer[idx] && !reallocBuffer()){
            //can't be TTY_FUNC_SYMBOL
            return tryGetQualName();
        }
        if(buffer[idx] == '_') {
            idx++;
            if(!buffer[idx] && !reallocBuffer()){
                //can't be TTY_FUNC_SYMBOL
                return tryGetQualName();
            }
            if(issymb(buffer[idx])) {
                //definitely a TTY_FUNC_SYMBOL
                getInputWhile(issymb);
                Token* tok = lv_alloc(sizeof(Token));
                tok->type = TTY_FUNC_SYMBOL;
                tok->value = extractValue();
                tok->next = NULL;
                bgn = idx;
                return tok;
            }
        }
    }
    //not a TTY_FUNC_SYMBOL
    return tryGetQualName();
}

//sets LV_TKN_ERROR if an error occurs
static TokenType qualNameType() {
    //  TTY_IDENT
    //fallback to
    //  TTY_QUAL_SYMBOL
    //  TTY_QUAL_IDENT
    //assume TTY_IDENT for now
    TokenType type = TTY_IDENT;
    //get all identifier chars
    getInputWhile(isident);
    //check for qualified name
    if(buffer[idx] == ':') {
        //qualified name
        idx++;
        if(!buffer[idx] && !reallocBuffer()) {
            //invalid. set error and return
            LV_TKN_ERROR = TE_BAD_QUAL;
            return 0;
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
            return 0;
        }
    }
    return type;
}

static Token* tryGetQualName() {
    //  TTY_IDENT
    //fallback to
    //  TTY_QUAL_SYMBOL
    //  TTY_QUAL_IDENT
    idx = bgn;
    assert(buffer[bgn]);
    assert(isidbgn(buffer[idx]));
    TokenType type = qualNameType();
    if(LV_TKN_ERROR) {
        //error occurred getting type
        return NULL;
    }
    Token* tok = lv_alloc(sizeof(Token));
    tok->type = type;
    tok->value = extractValue();
    tok->next = NULL;
    bgn = idx;
    return tok;
}

static Token* getSymbol() {
    
    idx = bgn;
    assert(issymb(buffer[idx]));
    getInputWhile(issymb);
    Token* tok = lv_alloc(sizeof(Token));
    tok->type = TTY_SYMBOL;
    tok->value = extractValue();
    tok->next = NULL;
    bgn = idx;
    return tok;
}

static Token* getNumber() {
    
    idx = bgn;
    if(isdigit(buffer[idx])) {
        //start with whole number
        getInputWhile(isdigit);
        //optional decimal
        if(buffer[idx] == '.') {
            idx++;
            if((!buffer[idx] && !reallocBuffer())
                || !isdigit(buffer[idx])) {
                //the next char is not a digit
                //can't end a number in a decimal point
                LV_TKN_ERROR = TE_BAD_NUM;
                return NULL;
            }
            getInputWhile(isdigit);
        }
    } else {
        //required decimal
        assert(buffer[idx] == '.');
        idx++;
        if((!buffer[idx] && !reallocBuffer())
            || !isdigit(buffer[idx])) {
            //the next char is not a digit
            //can't end a number in a decimal point
            LV_TKN_ERROR = TE_BAD_NUM;
            return NULL;
        }
        getInputWhile(isdigit);
    }
    //optional exponent
    if(buffer[idx] == 'e' || buffer[idx] == 'E') {
        idx++;
        if(!buffer[idx] && !reallocBuffer()) {
            //can't end in 'e'
            LV_TKN_ERROR = TE_BAD_EXP;
            return NULL;
        }
        if(buffer[idx] == '+' || buffer[idx] == '-') {
            //signs are ok
            idx++;
            if(!buffer[idx] && !reallocBuffer()) {
                //but not at the end of the number
                LV_TKN_ERROR = TE_BAD_EXP;
                return NULL;
            }
        }
        if(!isdigit(buffer[idx])) {
            //we need digits in the exponent, people!
            LV_TKN_ERROR = TE_BAD_EXP;
            return NULL;
        }
        getInputWhile(isdigit);
    }
    Token* tok = lv_alloc(sizeof(Token));
    tok->type = TTY_NUMBER;
    tok->value = extractValue();
    tok->next = NULL;
    bgn = idx;
    return tok;
}

static Token* getFuncVal() {
    
    idx = bgn;
    assert(buffer[idx] == '\\');
    idx++;
    if(!buffer[idx] && !reallocBuffer()) {
        //who is inputing these lone backslashes?
        LV_TKN_ERROR = TE_BAD_FUNC_VAL;
        return NULL;
    }
    if(issymb(buffer[idx])) {
        //plain symbolic
        getInputWhile(issymb);
    } else if(isidbgn(buffer[idx])) {
        //non-symbolic
        //we don't need the return value, just the side effect
        qualNameType();
        if(LV_TKN_ERROR) {
            return NULL;
        }
    } else {
        //again with the lone backslashes!!
        LV_TKN_ERROR = TE_BAD_FUNC_VAL;
        return NULL;
    }
    //optional trailing backslash
    if(buffer[idx] == '\\')
        idx++;
    //don't need to reallocate since it's the end :)
    Token* tok = lv_alloc(sizeof(Token));
    tok->type = TTY_FUNC_VAL;
    tok->value = extractValue();
    tok->next = NULL;
    bgn = idx;
    return tok;
}

static Token* getString() {
    
    idx = bgn;
    assert(buffer[idx] == '"');
    do {
        if(buffer[idx] == '\\') {
            //check escape sequences
            idx++;
            if(!buffer[idx])
                reallocBuffer(); //success check covered by default case
            //note: strchr allows the NUL character; this does not
            switch(buffer[idx]) {
                case 'n':
                case 't':
                case '\'':
                case '"':
                case '\\':
                    idx++;
                    if(!buffer[idx] && !reallocBuffer()) {
                        LV_TKN_ERROR = TE_UNTERM_STR;
                        return NULL;
                    }
                    break;
                default:
                    LV_TKN_ERROR = TE_BAD_STR_ESC;
                    return NULL;
            }
        } else {
            idx++;
            if(!buffer[idx] && !reallocBuffer()) {
                //unterminated string
                LV_TKN_ERROR = TE_UNTERM_STR;
                return NULL;
            }
        }
    } while(buffer[idx] != '"');
    //consume the closing quote; don't need to realloc the buffer
    idx++;
    Token* tok = lv_alloc(sizeof(Token));
    tok->type = TTY_STRING;
    tok->value = extractValue();
    tok->next = NULL;
    bgn = idx;
    return tok;
}
