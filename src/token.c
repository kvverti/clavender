#include "token.h"
#include "lavender.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

static TokenType tryGetFuncSymb();
static TokenType tryGetQualName();
static TokenType getSymbol();
static TokenType getString();
static TokenType getFuncVal();
static TokenType getNumber();
static TokenType getLiteral();

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
        lv_free(head);
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

static void fgetsWrapper(char* buf, int n, FILE* stream) {
    
    fgets(buf, n, stream);
    //if there was a NUL character in the text stream that got added into
    //the buffer, the lexing code will get confused about the amount of
    //text read. To remedy this, we find NUL characters that are not at
    //the end of the buffer and replace them with spaces.
    //A valid NUL character in the buffer is occurs when
    //  a) the input reaches end of file
    //  b) the NUL occurs at the end of the buffer
    //  c) the NUL occurs after a newline character
    //Any other occurrences of NUL are invalid and must be purged.
    size_t len = strlen(buf);
    while(!feof(stream) && len != (n - 1) && (len == 0 || buf[len - 1] != '\n')) {
        if(lv_debug)
            printf("TOKEN: Stray NUL at position %lu of %d (%s)\n", len, n, buf);
        buf[len] = ' ';
        len = strlen(buf);
    }
    //set inputEnd for the global buffer
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
        assert(idx >= bgn);
        for(int i = bgn; i < idx; i++)
            buffer[i - bgn] = buffer[i];
        fgetsWrapper(buffer + (idx - bgn), BUFFER_LEN - (idx - bgn), input);
    } else {
        //reallocate the whole buffer
        char* tmp = realloc(buffer, BUFFER_LEN * 2);
        if(!tmp) {
            lv_free(buffer);
            lv_shutdown();
        }
        buffer = tmp;
        size_t oldLen = BUFFER_LEN;
        BUFFER_LEN *= 2;
        memset(buffer + oldLen, 0, oldLen); //initialize new memory
        //subtract 1 for the NUL terminator
        fgetsWrapper(buffer + oldLen - 1, oldLen + 1, input);
        if(lv_debug)
            printf("TOKEN: buffer resize, old=%lu new=%lu\n",
                oldLen,
                BUFFER_LEN);
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
    bgn = idx = 0;
    
    Token* head = NULL;
    Token* tail = head;
    fgetsWrapper(buffer, BUFFER_LEN, input);
    inputEnd = (feof(input) || (buffer[0] && (buffer[strlen(buffer) - 1] == '\n')));
    while(buffer[bgn]) {
        char c = buffer[bgn];
        TokenType type = -1;
        //check for comment
        if(c == '\'') {
            //increment until next newline
            getInputWhile(isnotnl);
        } else if(isspace(c)) {
            idx++; //eat spaces
        } else if(isidbgn(c)) {
            type = tryGetFuncSymb();
        } else if(issymb(c)) {
            type = getSymbol();
        } else if(isdigit(c) || c == '.') {
            type = getNumber();
        } else if(c == '\\') {
            type = getFuncVal();
        } else if(c == '"') {
            type = getString();
        } else {
            //literal token
            type = getLiteral();
        }
        //check error
        if(LV_TKN_ERROR) {
            lv_tkn_free(head);
            lv_free(buffer);
            return NULL;
        }
        if(type != -1) {
            //create token
            Token* tok = lv_alloc(sizeof(Token) + (idx - bgn + 1));
            tok->type = type;
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
            reallocBuffer();
        }
    }
    lv_free(buffer);
    return head;
}

static TokenType getLiteral() {
    
    idx++;
    return TTY_LITERAL;
}

//returns the index of the next unprocessed char
static TokenType tryGetFuncSymb() {
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
        if(!buffer[idx] && !reallocBuffer()){
            //can't be TTY_FUNC_SYMBOL
            idx = bgn;
            return tryGetQualName();
        }
        if(buffer[idx] == '_') {
            idx++;
            if(!buffer[idx] && !reallocBuffer()){
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

static TokenType tryGetQualName() {
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
        if(!buffer[idx] && !reallocBuffer()) {
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

static TokenType getSymbol() {
    
    assert(issymb(buffer[idx]));
    getInputWhile(issymb);
    return TTY_SYMBOL;
}

static TokenType getNumber() {
    
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
                return -1;
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
            return 0;
        }
        getInputWhile(isdigit);
    }
    //optional exponent
    if(buffer[idx] == 'e' || buffer[idx] == 'E') {
        idx++;
        if(!buffer[idx] && !reallocBuffer()) {
            //can't end in 'e'
            LV_TKN_ERROR = TE_BAD_EXP;
            return -1;
        }
        if(buffer[idx] == '+' || buffer[idx] == '-') {
            //signs are ok
            idx++;
            if(!buffer[idx] && !reallocBuffer()) {
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

static TokenType getFuncVal() {
    
    assert(buffer[idx] == '\\');
    idx++;
    if(!buffer[idx] && !reallocBuffer()) {
        //who is inputing these lone backslashes?
        LV_TKN_ERROR = TE_BAD_FUNC_VAL;
        return -1;
    }
    if(issymb(buffer[idx])) {
        //plain symbolic
        getInputWhile(issymb);
    } else if(isidbgn(buffer[idx])) {
        //non-symbolic
        //we don't need the return value, just the side effect
        tryGetQualName();
        if(LV_TKN_ERROR) {
            return -1;
        }
    } else {
        //again with the lone backslashes!!
        LV_TKN_ERROR = TE_BAD_FUNC_VAL;
        return -1;
    }
    //optional trailing backslash
    if(buffer[idx] == '\\')
        idx++;
    //don't need to reallocate since it's the end :)
    return TTY_FUNC_VAL;
}

static TokenType getString() {
    
    //idx = bgn;
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
                        return -1;
                    }
                    break;
                default:
                    LV_TKN_ERROR = TE_BAD_STR_ESC;
                    return -1;
            }
        } else {
            idx++;
            if(!buffer[idx] && !reallocBuffer()) {
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
