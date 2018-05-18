#include "textbuffer.h"
#include "lavender.h"
#include "expression.h"
#include <string.h>
#include <stdio.h>

//redeclaration of the global text buffer
TextBufferObj* TEXT_BUFFER;
#define INIT_TEXT_BUFFER_LEN 8
static size_t textBufferLen;    //one past the end of the buffer
static size_t textBufferTop;    //one past the top of the buffer

/**
 * Adds the text to the buffer and appends a return object to the end.
 */
static void pushText(TextBufferObj* text, size_t len) {
    
    if(textBufferLen - textBufferTop < len) {
        //we must reallocate the buffer
        TEXT_BUFFER =
            lv_realloc(TEXT_BUFFER, textBufferLen * 2 * sizeof(TextBufferObj));
        memset(TEXT_BUFFER + textBufferLen, 0, textBufferLen * sizeof(TextBufferObj));
        textBufferLen *= 2;
    }
    memcpy(TEXT_BUFFER + textBufferTop, text, len * sizeof(TextBufferObj));
    textBufferTop += len;
}

//calculate the number of decimal digits
//in practice the index is usually < 10
static size_t length(int number) {
    
    size_t len = 1;
    while((number /= 10) != 0)
        len++;
    return len;
}

LvString* lv_tb_getString(TextBufferObj* obj) {
    
    LvString* res;
    switch(obj->type) {
        case OPT_STRING: {
            res = obj->str;
            return res;
        }
        case OPT_NUMBER: {
            //because rather nontrivial to find the length of a
            //floating-point value before putting it into a string,
            //we first make a reasonable (read: arbitrary) guess as
            //to the buffer length required. Then, we take the actual
            //number of characters printed by snprintf and reallocate
            //the buffer to that length (plus 1 for the terminator).
            //If our initial guess was to little, we call snprintf
            //again in order to get all the characters.
            #define GUESS_LEN 16
            res = lv_alloc(sizeof(LvString) + GUESS_LEN); //first guess
            res->refCount = 0;
            int len = snprintf(res->value, GUESS_LEN, "%g", obj->number);
            res = lv_realloc(res, sizeof(LvString) + len + 1);
            if(len >= GUESS_LEN) {
                snprintf(res->value, len + 1, "%g", obj->number);
            }
            res->len = len;
            return res;
            #undef GUESS_LEN
        }
        case OPT_FUNCTION:
        case OPT_FUNCTION_VAL: {
            size_t len = strlen(obj->func->name);
            res = lv_alloc(sizeof(LvString) + len + 1);
            res->refCount = 0;
            res->len = len;
            strcpy(res->value, obj->func->name);
            return res;
        }
        //not called outside of debug mode
        case OPT_PARAM: {
            static char str[] = "param ";
            size_t len = length(obj->param);
            len += sizeof(str) - 1;
            res = lv_alloc(sizeof(LvString) + len + 1);
            res->refCount = 0;
            res->len = len;
            strcpy(res->value, str);
            sprintf(res->value + sizeof(str) - 1, "%d", obj->param);
            return res;
        }
        case OPT_FUNC_CALL: {
            static char str[] = " CALL";
            size_t len = length(obj->callArity);
            len += sizeof(str) - 1;
            res = lv_alloc(sizeof(LvString) + len + sizeof(str));
            res->refCount = 0;
            res->len = len;
            sprintf(res->value, "%d", obj->callArity);
            strcat(res->value, str);
            return res;
        }
        case OPT_RETURN: {
            static char str[] = "return";
            res = lv_alloc(sizeof(LvString) + sizeof(str));
            res->refCount = 0;
            res->len = sizeof(str) - 1;
            strcpy(res->value, str);
            return res;
        }
        case OPT_BEQZ: {
            static char str[] = "beqz ";
            size_t len = length(obj->branchAddr) + sizeof(str) - 1;
            res = lv_alloc(sizeof(LvString) + len + sizeof(str));
            res->refCount = 0;
            res->len = len;
            strcpy(res->value, str);
            sprintf(res->value + sizeof(str) - 1, "%d", obj->branchAddr);
            return res;
        }
        default: {
            static char str[] = "<internal operator>";
            res = lv_alloc(sizeof(LvString) + sizeof(str));
            res->refCount = 0;
            res->len = sizeof(str) - 1;
            strcpy(res->value, str);
            return res;
        }
    }
}

static void rollback(Operator* decl, size_t top) {
    
    lv_op_removeOperator(decl->name,
        decl->fixing == FIX_PRE ? FNS_PREFIX : FNS_INFIX);
    //reset the text buffer
    for(size_t i = top; i < textBufferTop; i++) {
        if(TEXT_BUFFER[i].type == OPT_STRING)
            lv_free(TEXT_BUFFER[i].str);
    }
    textBufferTop = top;
}

static bool isExprEnd(Token* head) {
    
    //')' and ']' mark the end of the expression, ';' delimits conditionals
    return !head || head->value[0] == ')' || head->value[0] == ']' || head->value[0] == ';';
}

Token* lv_tb_defineFunction(Token* head, Operator* scope, Operator** res) {
    //get the function declaration
    Operator* decl = lv_expr_declareFunction(head, scope, &head);
    if(LV_EXPR_ERROR) {
        return NULL;
    }
    //save the top so we can roll back if necessary
    size_t top = textBufferTop;
    //function begin, often the same as top, but
    //different if there are nested functions.
    size_t fbgn;
    bool setbgn = false;
    bool conditional = false;
    //the index of the previous conditional branch
    //the value requires modification if there are
    //nested functions within this piecewise function
    size_t prevCondBranch = 0;
    if(isExprEnd(head)) {
        //no empty bodies allowed
        LV_EXPR_ERROR = XPE_MISSING_BODY;
        rollback(decl, top);
        return NULL;
    }
    while(!isExprEnd(head)) {
        TextBufferObj* text;
        size_t len;
        head = lv_expr_parseExpr(head, decl, &text, &len);
        if(LV_EXPR_ERROR) {
            rollback(decl, top);
            return NULL;
        }
        TextBufferObj end;
        if(head) {
            if(strcmp(head->value, "=>") == 0) {
                //can't have two bodies
                LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                rollback(decl, top);
                lv_expr_free(text, len);
                return NULL;
            } else if(head->value[0] == ';') {
                conditional = true;
                head = head->next;
                if(!head) { //a body is required
                    LV_EXPR_ERROR = XPE_MISSING_BODY;
                    rollback(decl, top);
                    lv_expr_free(text, len);
                    return NULL;
                }
                //it's a conditional
                TextBufferObj* cond;
                size_t clen;
                head = lv_expr_parseExpr(head, decl, &cond, &clen);
                if(LV_EXPR_ERROR) {
                    rollback(decl, top);
                    lv_expr_free(text, len);
                    return NULL;
                }
                if(prevCondBranch) {
                    //set the previous beanch statement's relative address.
                    //The beginning of the current condition will be placed
                    //at textBufferTop.
                    TEXT_BUFFER[prevCondBranch].branchAddr = textBufferTop - prevCondBranch;
                }
                //set the branch to the next condition, which usually
                //occurs at (len + 1) after the branch instruction,
                //but may be later becuase of nested function definitions.
                end.type = OPT_BEQZ;
                end.branchAddr = 0; //sentinel, will update later
                if(!setbgn) {
                    //only set fbgn on first run
                    fbgn = textBufferTop;
                    setbgn = true;
                }
                pushText(cond + 1, clen - 1);
                pushText(&end, 1);
                prevCondBranch = textBufferTop - 1;
                //another function body?
                if(head) {
                    if(strcmp(head->value, "=>") == 0) {
                        head = head->next;
                        if(isExprEnd(head)) {
                            LV_EXPR_ERROR = XPE_MISSING_BODY;
                            rollback(decl, top);
                            lv_expr_free(text, len);
                            lv_expr_free(cond, clen);
                            return NULL;
                        }
                    } else if(head->value[0] == ';') {
                        LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                        rollback(decl, top);
                        lv_expr_free(text, len);
                        lv_expr_free(cond, clen);
                        return NULL;
                    }
                }
                lv_free(cond);
            }
        } else if(conditional) {
            //function did not have a condition for one of its bodies
            LV_EXPR_ERROR = XPE_MISSING_BODY;
            rollback(decl, top);
            lv_expr_free(text, len);
            return NULL;
        }
        if(!setbgn) {
            fbgn = textBufferTop;
            setbgn = true;
        }
        pushText(text + 1, len - 1);
        end.type = OPT_RETURN;
        pushText(&end, 1);
        lv_free(text);
    }
    if(conditional) {
        if(prevCondBranch) {
            //set the last conditional branch
            TEXT_BUFFER[prevCondBranch].branchAddr = textBufferTop - prevCondBranch;
        }
        //push the default case (return undefined)
        TextBufferObj nan[2];
        nan[0].type = OPT_UNDEFINED;
        nan[1].type = OPT_RETURN;
        pushText(nan, 2);
    }
    //free param metadata
    for(int i = 0; i < decl->arity; i++)
        lv_free(decl->params[i].name);
    lv_free(decl->params);
    //set out param value
    decl->type = FUN_FUNCTION;
    decl->textOffset = fbgn;
    if(res)
        *res = decl;
    if(lv_debug) {
        //print function info
        printf("Function name=%s, arity=%d, fixing=%c, offset=%u\n",
            decl->name, decl->arity, decl->fixing, decl->textOffset);
    }
    return head;
}

static size_t startOfTmpExpr;

Token* lv_tb_parseExpr(Token* tokens, Operator* scope, size_t* start, size_t* end) {
    
    TextBufferObj* tmp;
    size_t tlen;
    Token* ret = lv_expr_parseExpr(tokens, scope, &tmp, &tlen);
    if(LV_EXPR_ERROR) {
        return NULL;
    }
    //add expr to buffer and set start of expr
    startOfTmpExpr = textBufferTop;
    pushText(tmp + 1, tlen - 1);
    lv_free(tmp);
    *start = startOfTmpExpr;
    *end = textBufferTop;
    if(lv_debug) {
        for(size_t i = 0; i < textBufferTop; i++) {
            LvString* str = lv_tb_getString(&TEXT_BUFFER[i]);
            printf("%lu: type=%d, value=%s\n",
                i,
                TEXT_BUFFER[i].type,
                str->value);
            if(str->refCount == 0)
                lv_free(str);
        }
    }
    return ret;
}

void lv_tb_clearExpr() {
    
    lv_expr_cleanup(TEXT_BUFFER + startOfTmpExpr, textBufferTop - startOfTmpExpr);
    textBufferTop = startOfTmpExpr;
    startOfTmpExpr = textBufferTop;
}

void lv_tb_onStartup() {
 
    TEXT_BUFFER = lv_alloc(INIT_TEXT_BUFFER_LEN * sizeof(TextBufferObj));
    memset(TEXT_BUFFER, 0, INIT_TEXT_BUFFER_LEN * sizeof(TextBufferObj));
    textBufferLen = INIT_TEXT_BUFFER_LEN;
    textBufferTop = 0;
    startOfTmpExpr = 0;
}

void lv_tb_onShutdown() {
    
    lv_expr_free(TEXT_BUFFER, textBufferTop);
}
