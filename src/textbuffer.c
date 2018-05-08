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

static void pushText(TextBufferObj* text, size_t len) {
    
    if(textBufferLen - textBufferTop < len) {
        //we must reallocate the buffer
        TEXT_BUFFER =
            lv_realloc(TEXT_BUFFER, textBufferLen * 2 * sizeof(TextBufferObj));
        memset(TEXT_BUFFER + textBufferLen, 0, textBufferLen * sizeof(TextBufferObj));
        textBufferLen *= 2;
    }
    memcpy(TEXT_BUFFER + textBufferTop, text, len);
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
        case OPT_STRING: { //do we really need to copy string?
            size_t sz = sizeof(LvString) + obj->str->len + 1;
            res = lv_alloc(sz);
            memcpy(res, obj->str, sz);
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
            res->len = len;
            sprintf(res->value, "%d", obj->callArity);
            strcat(res->value, str);
            return res;
        }
        default: {
            static char str[] = "<internal operator>";
            res = lv_alloc(sizeof(LvString) + sizeof(str));
            res->len = sizeof(str) - 1;
            strcpy(res->value, str);
            return res;
        }
    }
}

Token* lv_tb_defineFunction(Token* head, Operator* scope, Operator** res) {
    //get the function declaration
    Operator* decl = lv_expr_declareFunction(head, scope, &head);
    if(LV_EXPR_ERROR) {
        return NULL;
    }
    //get the function body (piecewise will come later)
    TextBufferObj* text = NULL;
    size_t len = 0;
    head = lv_expr_parseExpr(head, decl, &text, &len);
    if(LV_EXPR_ERROR) {
        FuncNamespace ns = decl->fixing == FIX_PRE ? FNS_PREFIX : FNS_INFIX;
        lv_op_removeOperator(decl->name, ns);
        return NULL;
    }
    if(lv_debug) {
        //print function info
        printf("Function name=%s, arity=%d, fixing=%c\n",
            decl->name, decl->arity, decl->fixing);
        for(int i = 0; i < decl->arity; i++) {
            printf("Parameter %d is %s. By-name? %d\n",
                i, decl->params[i].name, decl->params[i].byName);
        }
        for(size_t i = 1; i < len; i++) {
            LvString* str = lv_tb_getString(&text[i]);
            printf("Object type=%d, value=%s\n",
                text[i].type,
                str->value);
            lv_free(str);
        }
    }
    pushText(text + 1, len - 1);
    lv_free(text);
    if(res)
        *res = decl;
    return head;
}

void lv_tb_onStartup() {
 
    TEXT_BUFFER = lv_alloc(INIT_TEXT_BUFFER_LEN * sizeof(TextBufferObj));
    memset(TEXT_BUFFER, 0, INIT_TEXT_BUFFER_LEN * sizeof(TextBufferObj));
    textBufferLen = INIT_TEXT_BUFFER_LEN;
    textBufferTop = 0;
}

void lv_tb_onShutdown() {
    
    lv_expr_free(TEXT_BUFFER, textBufferTop);
}
