#include "textbuffer.h"
#include "lavender.h"
#include "expression.h"
#include "operator.h"
#include "builtin.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

//redeclaration of the global text buffer
TextBufferObj* TEXT_BUFFER;
#define INIT_TEXT_BUFFER_LEN 1024
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
        case OPT_UNDEFINED: {
            static char str[] = "<undefined>";
            res = lv_alloc(sizeof(LvString) + sizeof(str));
            res->refCount = 0;
            res->len = sizeof(str) - 1;
            strcpy(res->value, str);
            return res;
        }
        case OPT_STRING: {
            res = obj->str;
            return res;
        }
        case OPT_NUMBER: {
            //because rather nontrivial to find the length of a
            //floating-point value before putting it into a string,
            //we first call snprintf with an empty buffer. Then, we take
            //the number of characters printed by snprintf and allocate
            //the buffer to that length (plus 1 for the terminator).
            int len = snprintf(NULL, 0, "%g", obj->number);
            res = lv_alloc(sizeof(LvString) + len + 1);
            snprintf(res->value, len + 1, "%g", obj->number);
            res->refCount = 0;
            res->len = len;
            return res;
        }
        case OPT_INTEGER: {
            //get the sign bit
            bool negative = obj->integer >> 63;
            //get the magnitude of the value
            uint64_t value = negative ? (-obj->integer) : obj->integer;
            //get the length (+1 for minus sign)
            size_t len = snprintf(NULL, 0, "%"PRIu64, value);
            res = lv_alloc(sizeof(LvString) + negative + len + 1);
            res->refCount = 0;
            res->len = negative + len;
            if(negative) {
                res->value[0] = '-';
            }
            snprintf(res->value + negative, len + 1, "%"PRIu64, value);
            return res;
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
        case OPT_CAPTURE: {
            //func-name[cap1, cap2, ..., capn]
            size_t len = strlen(obj->capfunc->name) + 1;
            res = lv_alloc(sizeof(LvString) + len + 1);
            res->refCount = 0;
            strcpy(res->value, obj->capfunc->name);
            res->value[len - 1] = '[';
            res->value[len] = '\0';
            for(int i = 0; i < obj->capfunc->captureCount; i++) {
                LvString* tmp = lv_tb_getString(&obj->capture->value[i]);
                len += tmp->len + 1;
                res = lv_realloc(res, sizeof(LvString) + len + 1);
                strcat(res->value, tmp->value);
                res->value[len - 1] = ',';
                res->value[len] = '\0';
                if(tmp->refCount == 0)
                    lv_free(tmp);
            }
            res->value[len - 1] = ']';
            res->len = len;
            return res;
        }
        case OPT_VECT: {
            //handle Nil vect separately
            if(obj->vect->len == 0) {
                static char str[] = "{ }";
                res = lv_alloc(sizeof(LvString) + sizeof(str));
                res->refCount = 0;
                res->len = sizeof(str) - 1;
                memcpy(res->value, str, sizeof(str));
                return res;
            }
            //[ val1, val2, ..., valn ]
            size_t len = 2;
            res = lv_alloc(sizeof(LvString) + len + 1);
            res->refCount = 0;
            res->value[0] = '{';
            res->value[1] = ' ';
            res->value[2] = '\0';
            //concatenate values
            for(size_t i = 0; i < obj->vect->len; i++) {
                LvString* tmp = lv_tb_getString(&obj->vect->data[i]);
                len += tmp->len + 2;
                res = lv_realloc(res, sizeof(LvString) + len + 1);
                strcat(res->value, tmp->value);
                res->value[len - 2] = ',';
                res->value[len - 1] = ' ';
                res->value[len] = '\0';
                if(tmp->refCount == 0)
                    lv_free(tmp);
            }
            res->value[len - 2] = ' ';
            res->value[len - 1] = '}';
            res->len = len;
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
        case OPT_PUT_PARAM: {
            static char str[] = "put ";
            size_t len = length(obj->param);
            len += sizeof(str) - 1;
            res = lv_alloc(sizeof(LvString) + len + 1);
            res->refCount = 0;
            res->len = len;
            strcpy(res->value, str);
            sprintf(res->value + sizeof(str) - 1, "%d", obj->param);
            return res;
        }
        case OPT_MAKE_VECT:
        case OPT_FUNC_CALL2:
        case OPT_FUNC_CALL: {
            #define LEN sizeof(" CALL")
            size_t len = length(obj->callArity);
            len += LEN - 1;
            res = lv_alloc(sizeof(LvString) + len + LEN);
            res->refCount = 0;
            res->len = len;
            sprintf(res->value, "%d", obj->callArity);
            strcat(res->value, obj->type == OPT_MAKE_VECT ? " VECT"
                : obj->type == OPT_FUNC_CALL2 ? " CAL2" : " CALL");
            return res;
            #undef LEN
        }
        case OPT_FUNC_CAP: {
            static char str[] = "CAP";
            res = lv_alloc(sizeof(LvString) + sizeof(str));
            res->refCount = 0;
            res->len = sizeof(str) - 1;
            strcpy(res->value, str);
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

    //')' and ']' and '}' mark the end of the expression, ';' delimits conditionals
    return !head || head->value[0] == ')' || head->value[0] == ']' || head->value[0] == '}' || head->value[0] == ';';
}

Token* lv_tb_defineFunction(Token* head, Operator* scope, Operator** res) {
    //get the function declaration
    Operator* decl = lv_expr_declareFunction(head, scope, &head);
    if(LV_EXPR_ERROR) {
        return head;
    }
    head = lv_tb_defineFunctionBody(head, decl);
    if(LV_EXPR_ERROR)
        return head;
    if(res)
        *res = decl;
    return head;
}

static Token* parseFunctionLocals(Operator* decl);

Token* lv_tb_defineFunctionBody(Token* head, Operator* decl) {

    //save the top so we can roll back if necessary
    size_t top = textBufferTop;
    //function begin, often the same as top, but
    //different if there are nested functions.
    size_t fbgn = textBufferTop;
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
        return head;
    }
    //test for native impl (note native impls are not allowed locals)
    if(strcmp(head->value, "native") == 0) {
        head = head->next;
        if(!isExprEnd(head)) {
            LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
            rollback(decl, top);
            return head;
        }
        if(decl->locals > 0) {
            LV_EXPR_ERROR = XPE_NATIVE_LOCALS;
            rollback(decl, top);
            return head;
        }
        //only intrinsics supported for now
        Builtin func = lv_blt_getIntrinsic(decl->name);
        if(!func) {
            LV_EXPR_ERROR = XPE_NATIVE_NOT_FOUND;
            rollback(decl, top);
            return head;
        }
        for(int i = 0; i < (decl->arity + decl->locals); i++)
            lv_free(decl->params[i].name);
        lv_free(decl->params);
        decl->type = FUN_BUILTIN;
        decl->builtin = func;
        return head;
    }
    //parse function local initializers (if any)
    Token* err = parseFunctionLocals(decl);
    if(LV_EXPR_ERROR) {
        rollback(decl, top);
        return err;
    }
    if(decl->locals > 0) {
        //set the branch addr to the top for local jump
        setbgn = true;
        prevCondBranch = textBufferTop - 1;
    }
    while(!isExprEnd(head)) {
        TextBufferObj* text;
        size_t len;
        head = lv_expr_parseExpr(head, decl, &text, &len);
        if(LV_EXPR_ERROR) {
            rollback(decl, top);
            return head;
        }
        TextBufferObj end;
        if(head) {
            if(strcmp(head->value, "=>") == 0) {
                //can't have two bodies
                LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                rollback(decl, top);
                lv_expr_free(text, len);
                return head;
            } else if(head->value[0] == ';') {
                conditional = true;
                if(!head->next) { //a body is required
                    LV_EXPR_ERROR = XPE_MISSING_BODY;
                    rollback(decl, top);
                    lv_expr_free(text, len);
                    return head;
                }
                head = head->next;
                //it's a conditional
                TextBufferObj* cond;
                size_t clen;
                head = lv_expr_parseExpr(head, decl, &cond, &clen);
                if(LV_EXPR_ERROR) {
                    rollback(decl, top);
                    lv_expr_free(text, len);
                    return head;
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
                            return head;
                        }
                    } else if(head->value[0] == ';') {
                        LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                        rollback(decl, top);
                        lv_expr_free(text, len);
                        lv_expr_free(cond, clen);
                        return head;
                    }
                }
                lv_free(cond);
            } else if(prevCondBranch) {
                //set locals jump to the first instruction of the body
                TEXT_BUFFER[prevCondBranch].branchAddr = textBufferTop - prevCondBranch;
            }
        } else if(conditional) {
            //function did not have a condition for one of its bodies
            LV_EXPR_ERROR = XPE_MISSING_BODY;
            rollback(decl, top);
            lv_expr_free(text, len);
            return NULL;
        } else if(prevCondBranch) {
            //set locals jump to the first instruction of the body
            TEXT_BUFFER[prevCondBranch].branchAddr = textBufferTop - prevCondBranch;
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
    for(int i = 0; i < (decl->arity + decl->locals); i++)
        lv_free(decl->params[i].name);
    lv_free(decl->params);
    //set out param value
    decl->type = FUN_FUNCTION;
    decl->textOffset = fbgn;
    if(lv_debug) {
        //print function info
        printf("Function name=%s, arity=%d, capture=%d, locals=%d, fixing=%c, varargs=%s, offset=%u\n",
            decl->name,
            decl->arity,
            decl->captureCount,
            decl->locals,
            decl->fixing,
            decl->varargs ? "true" : "false",
            decl->textOffset);
        for(size_t i = decl->textOffset; i < textBufferTop; i++) {
            LvString* str = lv_tb_getString(&TEXT_BUFFER[i]);
            printf("%lu: type=%d, value=%s\n",
                i,
                TEXT_BUFFER[i].type,
                str->value);
            if(str->refCount == 0)
                lv_free(str);
        }
    }
    return head;
}

/**
 * Parses each function local initializer and places the code in sequence
 * before the function proper. The value of the initializer expression
 * is placed into the function local positions using the PUT operation.
 * Returns NULL if locals were successfully parsed, the erroring token
 * otherwise.
 */
static Token* parseFunctionLocals(Operator* decl) {

    assert(decl->type == FUN_FWD_DECL);
    if(decl->locals == 0) {
        return NULL;
    }
    //array to store the local initializers
    struct Initializer {
        TextBufferObj* code;
        size_t len;
    } initializers[decl->locals];
    int len = decl->arity + decl->locals;
    for(int i = decl->arity; i < len; i++) {
        //parse the initializer
        Token* startOfInit = decl->params[i].initializer;
        assert(startOfInit);
        struct Initializer* init = &initializers[i - decl->arity];
        startOfInit = lv_expr_parseExpr(startOfInit, decl, &init->code, &init->len);
        if(LV_EXPR_ERROR) {
            //there was an error parsing the initializer
            return startOfInit;
        }
        //startOfInit should now point to the closing paren
        //check that no params >= i are being accessed
        //this precludes functions being defined in function locals (#10)
        for(size_t j = 0; j < init->len; j++) {
            if(init->code[j].type == OPT_PARAM && init->code[j].param >= i) {
                LV_EXPR_ERROR = XPE_NAME_NOT_FOUND;
                //free all the initializers
                for(size_t k = 0; k < decl->locals; k++) {
                    lv_expr_free(initializers[k].code, initializers[k].len);
                }
                return startOfInit;
            }
        }
        assert(startOfInit->value[0] == ')');
    }
    //push initializer and put operation
    for(size_t i = 0; i < decl->locals; i++) {
        pushText(initializers[i].code + 1, initializers[i].len - 1);
        lv_free(initializers[i].code);
        TextBufferObj put = { .type = OPT_PUT_PARAM, .param = i + decl->arity };
        pushText(&put, 1);
    }
    TextBufferObj jump[2] = {
        { .type = OPT_NUMBER, .number = 0 },
        { .type = OPT_BEQZ, .branchAddr = 0 }
    };
    pushText(jump, 2);
    return NULL;
}

static size_t startOfTmpExpr;

Token* lv_tb_parseExpr(Token* tokens, Operator* scope, size_t* start, size_t* end) {

    TextBufferObj* tmp;
    size_t tlen;
    Token* ret = lv_expr_parseExpr(tokens, scope, &tmp, &tlen);
    if(LV_EXPR_ERROR) {
        return ret;
    }
    //add expr to buffer and set start of expr
    startOfTmpExpr = textBufferTop;
    pushText(tmp + 1, tlen - 1);
    lv_free(tmp);
    *start = startOfTmpExpr;
    *end = textBufferTop;
    return ret;
}

void lv_tb_clearExpr(void) {

    lv_expr_cleanup(TEXT_BUFFER + startOfTmpExpr, textBufferTop - startOfTmpExpr);
    textBufferTop = startOfTmpExpr;
    startOfTmpExpr = textBufferTop;
}

void lv_tb_onStartup(void) {

    TEXT_BUFFER = lv_alloc(INIT_TEXT_BUFFER_LEN * sizeof(TextBufferObj));
    memset(TEXT_BUFFER, 0, INIT_TEXT_BUFFER_LEN * sizeof(TextBufferObj));
    textBufferLen = INIT_TEXT_BUFFER_LEN;
    textBufferTop = 0;
    startOfTmpExpr = 0;
}

void lv_tb_onShutdown(void) {

    lv_expr_free(TEXT_BUFFER, textBufferTop);
}
