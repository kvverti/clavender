#include "expression.h"
#include "lavender.h"
#include "operator.h"
#include "command.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>

#define INIT_STACK_LEN 16
typedef struct TextStack {
    size_t len;
    TextBufferObj* top;
    TextBufferObj* stack;
} TextStack;

static void pushStack(TextStack* stack, TextBufferObj* obj);

// kept in sync with cxt.ops. It's hacky, but requires minimal
// changes. This whole stack thing should be refactored anyway.
typedef struct TokenStack {
    size_t len;
    Token** top;
    Token** stack;
} TokenStack;

static void pushToken(TokenStack* stack, Token* tok);

typedef struct IntStack {
    size_t len;
    int* top;
    int* stack;
} IntStack;

static void pushParam(IntStack* stack, int n);

/** Expression context passed to functions. */
typedef struct ExprContext {
    Token* head;            //the current token
    Operator* decl;         //the current function declaration
    char* startOfName;      //the beginning of the simple name
    bool expectOperand;     //do we expect an operand or an operator
    int nesting;            //how nested in brackets we are
    TextStack ops;          //the temporary operator stack
    TextStack out;          //the output stack
    TokenStack tok;         //the stack for ops' tokens, used for error
    IntStack params;        //the parameter stack
    IntStack maps;          //the map stack - which levels of nesting are maps
} ExprContext;

static int compare(TextBufferObj* a, TextBufferObj* b);
static void parseLiteral(TextBufferObj* obj, ExprContext* cxt);
static void parseIdent(TextBufferObj* obj, ExprContext* cxt);
static void parseSymbol(TextBufferObj* obj, ExprContext* cxt);
static void parseQualName(TextBufferObj* obj, ExprContext* cxt);
static void parseNumber(TextBufferObj* obj, ExprContext* cxt);
static void parseInteger(TextBufferObj* obj, ExprContext* cxt);
static void parseString(TextBufferObj* obj, ExprContext* cxt);
static void parseDotSymb(TextBufferObj* obj, ExprContext* cxt);
static void parseFuncValue(TextBufferObj* obj, ExprContext* cxt);
static void parseTextObj(TextBufferObj* obj, ExprContext* cxt); //calls above functions
//runs one cycle of shunting yard
static void shuntingYard(TextBufferObj* obj, ExprContext* cxt);
static bool isLiteral(TextBufferObj* obj, char c);
static bool shuntOps(ExprContext* cxt);
static TextBufferObj makeByName(TextBufferObj* expr, size_t len, ExprContext* cxt);

#define IF_ERROR_CLEANUP \
    if(LV_EXPR_ERROR) { \
        lv_expr_cleanup(cxt.out.stack, cxt.out.top - cxt.out.stack + 1); \
        lv_expr_cleanup(cxt.ops.stack, cxt.ops.top - cxt.ops.stack + 1); \
        lv_free(cxt.out.stack); \
        lv_free(cxt.ops.stack); \
        lv_free(cxt.tok.stack); \
        lv_free(cxt.params.stack); \
        lv_free(cxt.maps.stack); \
        return cxt.head; \
    } else (void)0

Token* lv_expr_parseExpr(Token* head, Operator* decl, TextBufferObj** res, size_t* len) {

    if(LV_EXPR_ERROR)
        return head;
    //set up required environment
    //this is function local because
    //recursive calls are possible
    ExprContext cxt;
    cxt.head = head;
    cxt.decl = decl;
    cxt.startOfName = strrchr(decl->name, ':') + 1;
    cxt.expectOperand = true;
    cxt.nesting = 0;
    //initialize stacks. Set the zeroth element to a sentinel value.
    cxt.out.len = INIT_STACK_LEN;
    cxt.out.stack = lv_alloc(INIT_STACK_LEN * sizeof(TextBufferObj));
    cxt.out.top = cxt.out.stack;
    cxt.out.stack[0].type = OPT_UNDEFINED;
    cxt.ops.len = INIT_STACK_LEN;
    cxt.ops.stack = lv_alloc(INIT_STACK_LEN * sizeof(TextBufferObj));
    cxt.ops.top = cxt.ops.stack;
    cxt.ops.stack[0].type = OPT_UNDEFINED;
    cxt.tok.len = INIT_STACK_LEN;
    cxt.tok.stack = lv_alloc(INIT_STACK_LEN * sizeof(Token*));
    cxt.tok.top = cxt.tok.stack;
    cxt.tok.stack[0] = NULL;
    cxt.params.len = INIT_STACK_LEN;
    cxt.params.stack = lv_alloc(INIT_STACK_LEN * sizeof(int));
    cxt.params.top = cxt.params.stack;
    cxt.params.stack[0] = 0;
    cxt.maps.len = INIT_STACK_LEN;
    cxt.maps.stack = lv_alloc(INIT_STACK_LEN * sizeof(int));
    cxt.maps.top = cxt.maps.stack;
    cxt.maps.stack[0] = 0;
    //loop over each token until we reach the end of the expression
    //(end-of-stream, closing grouper ')', or expression split ';')
    do {
        //is this a def?
        TextBufferObj obj;
        if(lv_tkn_cmp(cxt.head, "def") == 0) {
            //must be expecting an operand
            if(!cxt.expectOperand) {
                LV_EXPR_ERROR = XPE_EXPECT_INF;
                IF_ERROR_CLEANUP;
            }
            //recursive call to lv_tb_defineFunction
            Operator* op;
            cxt.head = lv_tb_defineFunction(cxt.head, cxt.decl, &op);
            cxt.expectOperand = false;
            IF_ERROR_CLEANUP;
            obj.type = OPT_FUNCTION_VAL;
            obj.func = op;
            shuntingYard(&obj, &cxt);
            IF_ERROR_CLEANUP;
        } else if(lv_tkn_cmp(cxt.head, "=>") == 0) {
            if(cxt.nesting == 0) {
                //end of conditonal, should be
                break;
            } else if(!cxt.expectOperand && *cxt.maps.top == cxt.nesting) {
                //arrow in a map at operator position
                //shunt over the key expression
                if(cxt.ops.top == cxt.ops.stack) {
                    LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                    IF_ERROR_CLEANUP;
                }
                while(!isLiteral(cxt.ops.top, '{')) {
                    shuntOps(&cxt);
                    if(cxt.ops.top == cxt.ops.stack) {
                        LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                    }
                    IF_ERROR_CLEANUP;
                }
                *cxt.maps.top = -*cxt.maps.top;
                cxt.head = cxt.head->next;
                cxt.expectOperand = true;
            } else if(!cxt.expectOperand || cxt.ops.top->type != OPT_LITERAL) {
                LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                IF_ERROR_CLEANUP;
            } else {
                switch(cxt.ops.top->literal) {
                    case '(':
                    case '[':
                    case '{': {
                        //explicit by-name expression
                        TextBufferObj* byNameExprBody;
                        size_t byNameExprLen;
                        cxt.head = lv_expr_parseExpr(cxt.head->next, decl, &byNameExprBody, &byNameExprLen);
                        IF_ERROR_CLEANUP;
                        assert(byNameExprBody[0].type == OPT_UNDEFINED);
                        obj = makeByName(byNameExprBody + 1, byNameExprLen - 1, &cxt);
                        lv_free(byNameExprBody);
                        shuntingYard(&obj, &cxt);
                        IF_ERROR_CLEANUP;
                        break;
                    }
                    default:
                        LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                        IF_ERROR_CLEANUP;
                }
                cxt.expectOperand = false;
            }
        } else {
            //get the next text object
            parseTextObj(&obj, &cxt);
            IF_ERROR_CLEANUP;
            //detect end of expr before we parse
            if(cxt.nesting < 0            //closing grouper without opening grouper
            || cxt.head->start[0] == ';'  //conditional separator
            || (cxt.nesting == 0 && cxt.head->start[0] == ',') //comma at top level
            ) {
                break;
            }
            shuntingYard(&obj, &cxt);
            IF_ERROR_CLEANUP;
            cxt.head = cxt.head->next;
        }
    } while(cxt.head);
    //get leftover ops over
    while(cxt.ops.top != cxt.ops.stack) {
        shuntOps(&cxt);
        IF_ERROR_CLEANUP;
    }
    if(cxt.out.top == cxt.out.stack) {
        LV_EXPR_ERROR = XPE_MISSING_BODY;
        IF_ERROR_CLEANUP;
    }
    //check for tail call
    if(cxt.out.top->type == OPT_FUNCTION && cxt.out.top->func == cxt.decl) {
        cxt.out.top->type = OPT_TAIL;
    }
    *res = cxt.out.stack;
    *len = cxt.out.top - cxt.out.stack + 1;
    //calling plain lv_free is ok because ops is empty
    lv_free(cxt.ops.stack);
    lv_free(cxt.tok.stack);
    lv_free(cxt.params.stack);
    lv_free(cxt.maps.stack);
    return cxt.head;
}

#undef IF_ERROR_CLEANUP

//static helpers for lv_expr_parseExpr

static TextBufferObj makeByName(TextBufferObj* expr, size_t len, ExprContext* cxt) {

    TextBufferObj res;
    if(len == 1) {
        //trivial call-by-name expr, no need to wrap
        res = *expr;
        if(res.type == OPT_FUNCTION) {
            res.type = OPT_FUNCTION_VAL;
        }
    } else {
        Operator* op = lv_alloc(sizeof(Operator));
        op->type = FUN_FUNCTION;
        size_t nlen = strlen(cxt->decl->name) + 1;
        op->name = lv_alloc(nlen + 1);
        memcpy(op->name, cxt->decl->name, nlen);
        op->name[nlen - 1] = ':';
        op->name[nlen] = '\0';
        op->captureCount = cxt->decl->arity + cxt->decl->locals;
        op->arity = op->captureCount;
        op->locals = 0;
        op->fixing = FIX_PRE;
        op->enclosing = cxt->decl;
        op->next = NULL;
        op->varargs = false;
        op->textOffset = (int) lv_tb_addExpr(len, expr);
        memset(op->byName, 0, sizeof(op->byName));
        lv_op_addOperator(op, FNS_PREFIX);
        //add capture to expr body
        res.type = OPT_FUNCTION_VAL;
        res.func = op;
    }
    return res;
}

static int getLexicographicPrecedence(char c) {

    switch(c) {
        case '$': return 1;
        case '|': return 2;
        case '^': return 3;
        case '&': return 4;
        case '!':
        case '=': return 5;
        case '>':
        case '<': return 6;
        case '#': return 7; //':' was changed to '#' earlier
        case '-':
        case '+': return 8;
        case '%':
        case '/':
        case '*': return 9;
        case '~':
        case '?': return 10;
        default:  return 0;
    }
}

static int getFixingValue(TextBufferObj* obj) {

    if(obj->type == OPT_FUNC_CALL2) {
        return 1;
    }
    assert(obj->type == OPT_FUNCTION);
    if(obj->func->fixing == FIX_PRE || obj->func->arity == 1) {
        return 2;
    } else {
        return 0;
    }
}

/** Whether this object represents the literal character c. */
static bool isLiteral(TextBufferObj* obj, char c) {

    return obj->type == OPT_LITERAL && obj->literal == c;
}

/** Compares a and b by precedence. */
static int compare(TextBufferObj* a, TextBufferObj* b) {

    //values have highest precedence
    {
        int ar = (a->type != OPT_LITERAL && a->type != OPT_FUNC_CALL2 &&
            (a->type != OPT_FUNCTION || a->func->arity == 0));
        int br = (b->type != OPT_LITERAL && b->type != OPT_FUNC_CALL2 &&
            (b->type != OPT_FUNCTION || b->func->arity == 0));
        if(ar || br) {
            assert(false);
            return ar - br;
        }
    }
    //close groupers '}' and ')' have next highest
    {
        int ac = (isLiteral(a, ')') || isLiteral(a, '}'));
        int bc = (isLiteral(b, ')') || isLiteral(b, '}'));
        if(ac || bc)
            return ac - bc;
    }
    //openers '{' and '(' have the lowest
    if(isLiteral(a, '(') || isLiteral(a, '{'))
        return -1;
    if(isLiteral(b, '(') || isLiteral(b, '{'))
        return 1;
    //check fixing
    //prefix > call 2 > infix = postfix
    {
        int afix = getFixingValue(a);
        int bfix = getFixingValue(b);
        if(afix != bfix)
            return afix - bfix;
        if(afix != 0) //prefix functions and call2 always have equal precedence
            return 0;
    }
    //compare infix operators with modified Scala ordering
    //note that '**' has greater precedence than other combinations
    //get the beginning of the simple names
    {
        char* ac = strrchr(a->func->name, ':') + 1;
        char* bc = strrchr(b->func->name, ':') + 1;
        int ap = getLexicographicPrecedence(*ac);
        int bp = getLexicographicPrecedence(*bc);
        if(ap ^ bp)
            return ap - bp;
        //check special '**' combination
        ap = (strncmp(ac, "**", 2) == 0);
        bp = (strncmp(bc, "**", 2) == 0);
        return ap - bp;
    }
}

/** Returns a new string with concatenation of a and b */
static char* concat(char* a, int alen, char* b, int blen) {

    char* res = lv_alloc(alen + blen + 1);
    memcpy(res, a, alen);
    memcpy(res + alen, b, blen);
    res[alen + blen] = '\0';
    return res;
}

static void parseLiteral(TextBufferObj* obj, ExprContext* cxt) {

    obj->type = OPT_LITERAL;
    obj->literal = cxt->head->start[0];
    switch(obj->literal) {
        case '(':
            if(cxt->head->next->start[0] == ')') {
                //empty args
                obj->type = OPT_EMPTY_ARGS;
                if(!cxt->expectOperand) {
                    //unless this is a func call 2!
                    obj->fromType = obj->type;
                    obj->type = OPT_FUNC_CALL2;
                }
                cxt->expectOperand = false;
                cxt->head = cxt->head->next;
            } else {
                if(!cxt->expectOperand) {
                    //value call 2 operator
                    obj->fromType = obj->type;
                    obj->type = OPT_FUNC_CALL2;
                    cxt->expectOperand = true;
                }
                //else parenthesized expression
                //increment nesting in either case
                cxt->nesting++;
            }
            break;
        case '{':
            cxt->nesting++;
            //open groupings are "operands"
            if(!cxt->expectOperand) {
                //value call 2 operator
                obj->fromType = obj->type;
                obj->type = OPT_FUNC_CALL2;
                cxt->expectOperand = true;
            }
            break;
        case '}':
            if(cxt->expectOperand
            && cxt->params.top != cxt->params.stack
            && *cxt->params.top > 0) {
                //in an operand position, `}` may only directly
                //follow `{`
                LV_EXPR_ERROR = XPE_EXPECT_PRE;
            }
            cxt->nesting--;
            cxt->expectOperand = false;
            break;
        case ')':
            cxt->nesting--;
            //fallthrough
        case ',':
            //close groupings are "operators"
            if(cxt->nesting > 0 && cxt->expectOperand) {
                LV_EXPR_ERROR = XPE_EXPECT_INF;
            }
            break;
        case ';':
            //Separator for the conditional
            //portion of a function can only
            //occur when nesting == 0
            if(cxt->nesting != 0) {
                LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
            }
            break;
        default:
            LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
    }
    if(obj->literal == ',')
        cxt->expectOperand = true;
}

static void parseSymbolImpl(TextBufferObj* obj, FuncNamespace ns, char* _name, size_t nameLen, ExprContext* cxt) {

    //we find the function with the simple name
    //in the innermost scope possible by going
    //through outer scopes until we can't find
    //a function definition. Only if there is
    //no function name in the scope ladder do
    //we go for imported function names.
    char* nsbegin = cxt->decl->name;
    Operator* func = NULL;
    char name[nameLen + 1];
    memcpy(name, _name, nameLen);
    name[nameLen] = '\0';
    //change ':' to '#' in names
    {
        char* c = name;
        while((c = strchr(c, ':')))
            *c = '#';
    }
    do {
        //get the function with the name in the scope
        nsbegin = strchr(nsbegin, ':') + 1;
        char* fname = concat(cxt->decl->name,   //beginning of scope
            nsbegin - cxt->decl->name,          //length of scope name
            name,                               //name to check
            nameLen);                          //length of name
        Operator* test = lv_op_getOperator(fname, ns);
        lv_free(fname);
        if(test)
            func = test;
    } while(cxt->startOfName != nsbegin);
    //test is null. func should contain the function
    if(!func) {
        //try imported function names
        char* n = lv_cmd_getQualNameFor(name);
        if(n) {
            func = lv_op_getOperator(n, ns);
        } else {
            char** scopes;
            size_t len;
            lv_cmd_getUsingScopes(&scopes, &len);
            for(size_t i = 0; (i < len) && !func; i++) {
                func = lv_op_getScopedOperator(scopes[i], name, ns);
            }
        }
    }
    if(!func) {
        //that name does not exist!
        LV_EXPR_ERROR = XPE_NAME_NOT_FOUND;
        return;
    }
    obj->type = OPT_FUNCTION;
    obj->func = func;
    //toggle if RHS is true
    cxt->expectOperand ^=
        ((!cxt->expectOperand && func->arity != 1) || func->arity == 0);
}

static void parseSymbolHelp(TextBufferObj* obj, char* name, size_t nameLen, ExprContext* cxt) {

    FuncNamespace ns = cxt->expectOperand ? FNS_PREFIX : FNS_INFIX;
    parseSymbolImpl(obj, ns, name, nameLen, cxt);
    if(LV_EXPR_ERROR && !cxt->expectOperand) {
        //check prefix functions and make a func call 2
        LV_EXPR_ERROR = 0;
        cxt->expectOperand = true;
        parseSymbolImpl(obj, FNS_PREFIX, name, nameLen, cxt);
        if(!LV_EXPR_ERROR) {
            obj->fromType = obj->type;
            obj->type = OPT_FUNC_CALL2;
        }
    }
}

static void parseSymbol(TextBufferObj* obj, ExprContext* cxt) {

    parseSymbolHelp(obj, cxt->head->start, cxt->head->len, cxt);
}

static void parseQualNameImpl(TextBufferObj* obj, FuncNamespace ns, char* _name, size_t nameLen, ExprContext* cxt) {

    //change ':' to '#' in names, but only after the namespace separator
    char name[nameLen + 1];
    memcpy(name, _name, nameLen);
    name[nameLen] = '\0';
    {
        char* c = strchr(name, ':') + 1;
        while((c = strchr(c, ':')))
            *c = '#';
    }
    //since it's a qualified name, we don't need to
    //guess what function it could be!
    Operator* func = lv_op_getOperator(name, ns);
    if(!func) {
        //404 func not found
        LV_EXPR_ERROR = XPE_NAME_NOT_FOUND;
        return;
    }
    obj->type = OPT_FUNCTION;
    obj->func = func;
    //toggle if RHS is true
    cxt->expectOperand ^=
        ((!cxt->expectOperand && func->arity != 1) || func->arity == 0);
}

static void parseQualNameHelp(TextBufferObj* obj, char* name, size_t nameLen, ExprContext* cxt) {

    FuncNamespace ns = cxt->expectOperand ? FNS_PREFIX : FNS_INFIX;
    parseQualNameImpl(obj, ns, name, nameLen, cxt);
    if(LV_EXPR_ERROR && !cxt->expectOperand) {
        //check prefix functions and make a func call 2
        LV_EXPR_ERROR = 0;
        cxt->expectOperand = true;
        parseQualNameImpl(obj, FNS_PREFIX, name, nameLen, cxt);
        if(!LV_EXPR_ERROR) {
            obj->fromType = obj->type;
            obj->type = OPT_FUNC_CALL2;
        }
    }
}

static void parseQualName(TextBufferObj* obj, ExprContext* cxt) {

    parseQualNameHelp(obj, cxt->head->start, cxt->head->len, cxt);
}

static void parseFuncValue(TextBufferObj* obj, ExprContext* cxt) {

    bool expectedOperand = cxt->expectOperand;
    size_t len = cxt->head->len;
    FuncNamespace ns;
    //values ending in '\' are infix functions
    //subtract 1 from length so we can skip the initial '\'
    if(cxt->head->start[len - 1] == '\\') {
        ns = FNS_INFIX;
        cxt->expectOperand = false;
        //substringing
        cxt->head->start[len - 1] = '\0';
    } else {
        ns = FNS_PREFIX;
        cxt->expectOperand = true;
    }
    if(cxt->head->type == TTY_QUAL_FUNC_VAL) {
        parseQualNameHelp(obj, cxt->head->start + 1, cxt->head->len - 1, cxt);
    } else {
        parseSymbolHelp(obj, cxt->head->start + 1, cxt->head->len - 1, cxt);
    }
    //undo substringing
    if(ns == FNS_INFIX)
        cxt->head->start[len - 1] = '\\';
    //check that this isn't a zero arity function
    if(!LV_EXPR_ERROR && obj->func->arity == obj->func->captureCount) {
        LV_EXPR_ERROR = XPE_ZERO_ARITY_ALIAS;
        return;
    }
    obj->type = OPT_FUNCTION_VAL;
    if(!expectedOperand) {
        obj->fromType = obj->type;
        obj->type = OPT_FUNC_CALL2;
    }
    cxt->expectOperand = false;
}

static void parseIdent(TextBufferObj* obj, ExprContext* cxt) {

    //try parameter names first
    int numParams = cxt->decl->arity + cxt->decl->locals;
    for(int i = 0; i < numParams; i++) {
        if(lv_tkn_cmp(cxt->head, cxt->decl->params[i].name) == 0) {
            //save param name
            obj->type = OPT_PARAM;
            obj->param = i;
            if(!cxt->expectOperand) {
                obj->fromType = obj->type;
                obj->type = OPT_FUNC_CALL2;
            }
            cxt->expectOperand = false;
            return;
        }
    }
    //not a parameter, try a function
    parseSymbol(obj, cxt);
}

static void parseNumber(TextBufferObj* obj, ExprContext* cxt) {

    double num = strtod(cxt->head->start, NULL);
    obj->type = OPT_NUMBER;
    obj->number = num;
    if(!cxt->expectOperand) {
        obj->fromType = obj->type;
        obj->type = OPT_FUNC_CALL2;
    }
    cxt->expectOperand = false;
}

static void parseInteger(TextBufferObj* obj, ExprContext* cxt) {

    uint64_t num;
    if(cxt->head->start[0] == '0') {
        //parse in the given base
        switch(cxt->head->start[1]) {
            case 'x':
            case 'X':
                num = (uint64_t) strtoumax(cxt->head->start + 2, NULL, 16);
                break;
            case 'c':
            case 'C':
                num = (uint64_t) strtoumax(cxt->head->start + 2, NULL, 8);
                break;
            case 'b':
            case 'B':
                num = (uint64_t) strtoumax(cxt->head->start + 2, NULL, 2);
                break;
            default:
                num = (uint64_t) strtoumax(cxt->head->start, NULL, 10);
        }
    } else {
        num = (uint64_t) strtoumax(cxt->head->start, NULL, 10);
    }
    obj->type = OPT_INTEGER;
    obj->integer = num;
    if(!cxt->expectOperand) {
        obj->fromType = obj->type;
        obj->type = OPT_FUNC_CALL2;
    }
    cxt->expectOperand = false;
}

static size_t getStringValue(char* c, char* str) {

    size_t len = 0;
    while(*c != '"') {
        if(*c == '\\') {
            //handle escape sequences
            switch(*++c) {
                case 'n': str[len] = '\n';
                    break;
                case 't': str[len] = '\t';
                    break;
                case '"': str[len] = '"';
                    break;
                case '\'': str[len] = '\'';
                    break;
                case '\\': str[len] = '\\';
                    break;
                default:
                    assert(false);
            }
        } else {
            str[len] = *c;
        }
        c++;
        len++;
    }
    return len;
}

static void parseString(TextBufferObj* obj, ExprContext* cxt) {

    char* c = cxt->head->start + 1; //skip open quote
    LvString* newStr = lv_alloc(sizeof(LvString) + cxt->head->len);
    newStr->refCount = 1; //it will be added to the text buffer
    size_t len = getStringValue(c, newStr->value);
    newStr = lv_realloc(newStr, sizeof(LvString) + len + 1);
    newStr->value[len] = '\0';
    newStr->len = len;
    obj->type = OPT_STRING;
    obj->str = newStr;
    if(!cxt->expectOperand) {
        obj->fromType = obj->type;
        obj->type = OPT_FUNC_CALL2;
    }
    cxt->expectOperand = false;
}

static void parseDotSymb(TextBufferObj* obj, ExprContext* cxt) {

    char name[cxt->head->len];
    if(cxt->head->start[1] == '"') {
        size_t len = getStringValue(cxt->head->start + 2, name);
        assert(len < cxt->head->len);
        name[len] = '\0';
    } else {
        memcpy(name, cxt->head->start + 1, cxt->head->len - 1);
        name[cxt->head->len - 1] = '\0';
    }
    *obj = lv_tb_getSymb(name);
    if(!cxt->expectOperand) {
        obj->fromType = obj->type;
        obj->type = OPT_FUNC_CALL2;
    }
    cxt->expectOperand = false;
}

static void parseTextObj(TextBufferObj* obj, ExprContext* cxt) {

    switch(cxt->head->type) {
        case TTY_LITERAL:
            parseLiteral(obj, cxt);
            break;
        case TTY_IDENT:
            parseIdent(obj, cxt);
            break;
        case TTY_SYMBOL:
            parseSymbol(obj, cxt);
            break;
        case TTY_QUAL_IDENT:
        case TTY_QUAL_SYMBOL:
            parseQualName(obj, cxt);
            break;
        case TTY_NUMBER:
            parseNumber(obj, cxt);
            break;
        case TTY_INTEGER:
            parseInteger(obj, cxt);
            break;
        case TTY_STRING:
            parseString(obj, cxt);
            break;
        case TTY_DOT_SYMB:
            parseDotSymb(obj, cxt);
            break;
        case TTY_FUNC_VAL:
        case TTY_QUAL_FUNC_VAL:
            parseFuncValue(obj, cxt);
            break;
        case TTY_FUNC_SYMBOL:
        case TTY_ELLIPSIS:
            LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
            break;
    }
}

static void pushStack(TextStack* stack, TextBufferObj* obj) {
    //overwrite empty args because it's just a signal.
    //an abuse of the stack, but oh well
    //note that empty args will NEVER appear in the final text.
    if(stack->top->type == OPT_EMPTY_ARGS) {
        assert(stack->top != stack->stack);
        *stack->top = *obj;
        return;
    }
    if(stack->top + 1 == stack->stack + stack->len) {
        stack->len *= 2;
        size_t sz = stack->top - stack->stack;
        stack->stack = lv_realloc(stack->stack,
            stack->len * sizeof(TextBufferObj));
        stack->top = stack->stack + sz;
    }
    *++stack->top = *obj;
}

static void pushToken(TokenStack* stack, Token* tok) {

    if(stack->top + 1 == stack->stack + stack->len) {
        stack->len *= 2;
        size_t sz = stack->top - stack->stack;
        stack->stack = lv_realloc(stack->stack,
            stack->len * sizeof(Token*));
        stack->top = stack->stack + sz;
    }
    *++stack->top = tok;
}

static void pushParam(IntStack* stack, int num) {

    if(stack->top + 1 == stack->stack + stack->len) {
        stack->len *= 2;
        size_t sz = stack->top - stack->stack;
        stack->stack = lv_realloc(stack->stack,
            stack->len * sizeof(TextBufferObj));
        stack->top = stack->stack + sz;
    }
    *++stack->top = num;
}

#define REQUIRE_NONEMPTY(s) \
    if(s.top == s.stack) { LV_EXPR_ERROR = XPE_UNBAL_GROUP; return; } else (void)0

/**
 * Sets the negative placeholder arity to positive on
 * the first (prefix) or second (infix) function argument.
 */
static void fixArityFirstArg(ExprContext* cxt) {
    //check for first param to function and fix arity
    if(cxt->params.top != cxt->params.stack && *cxt->params.top < 0) {
        *cxt->params.top = -*cxt->params.top;
    }
}

static int arityFor(Operator* func, Operator* scope) {

    int ar;
    if(func == scope) {
        //capturing this function
        ar = scope->arity;
    } else {
        //detect whether this is an enclosing function or a nested
        //function. An enclosing function does not capture any locals
        //after its declaration.
        size_t localsToSkip = 0;
        Operator* outer = scope->enclosing;
        //subtract enclosing functions' locals while we have not reached func
        while(outer && outer != func) {
            localsToSkip += outer->locals;
            outer = outer->enclosing;
        }
        if(outer == NULL) {
            //not found in the enclosing functions, it must be inner!
            ar = scope->arity + scope->locals;
        } else {
            ar = scope->arity - localsToSkip - outer->locals;
        }
    }
    return ar;
}

/**
 * Given the last operation of an expression, returns a pointer to the beginning.
 */
static TextBufferObj* getExprBounds(TextBufferObj* end) {

    TextBufferObj* bgn = end;
    switch(end->type) {
        case OPT_UNDEFINED:
        case OPT_NUMBER:
        case OPT_INTEGER:
        case OPT_PARAM:
        case OPT_FUNCTION_VAL:
        case OPT_STRING:
        case OPT_SYMB:
            break;
        case OPT_FUNCTION:
            // get the beginning of the argument list
            for(int i = 0; i < (end->func->arity - end->func->captureCount); i++) {
                bgn = getExprBounds(bgn - 1);
            }
            break;
        case OPT_FUNC_CAP: {
            // get the beginning of the capture list
            // capture list is (1 + captureCount)
            TextBufferObj* func = end - 1;
            assert(func->type == OPT_FUNCTION_VAL);
            for(int i = 0; i <= func->func->captureCount; i++) {
                bgn = getExprBounds(bgn - 1);
            }
            break;
        }
        case OPT_MAKE_VECT:
        case OPT_FUNC_CALL2:
            // get the beginning of the parameter list
            for(int i = 0; i < end->callArity; i++) {
                bgn = getExprBounds(bgn - 1);
            }
            break;
        case OPT_MAKE_MAP:
            //maps have keys and values
            for(int i = 0; i < end->callArity; i++) {
                bgn = getExprBounds(bgn - 1);
                bgn = getExprBounds(bgn - 1);
            }
            break;
        default:
            assert(false);
    }
    return bgn;
}

static void collectByNameArgs(TextBufferObj* func, int ar, ExprContext* cxt) {

    //stack to hold by-name expressions for the current function
    //the stack holds the by-name expressions in reverse operation order
    static uint8_t arr[MAX_PARAMS / 8]; //zeroed
    if(memcmp(func->func->byName, arr, sizeof(arr)) == 0) {
        //no by-names, no need to process
        return;
    }
    TextStack tmpByNames;
    tmpByNames.len = INIT_STACK_LEN;
    tmpByNames.stack = lv_alloc(INIT_STACK_LEN * sizeof(TextBufferObj));
    tmpByNames.top = tmpByNames.stack;
    tmpByNames.stack[0].type = OPT_UNDEFINED;
    int lastParam = func->func->arity - func->func->captureCount - 1;
    assert(lastParam >= 0);
    for(int i = ar - 1; i >= 0; i--) {
        int p = i > lastParam ? lastParam : i;
        TextBufferObj* bgn = getExprBounds(cxt->out.top);
        if(LV_GET_BYNAME(func->func, p)) {
            //wrap in a by-name expression
            size_t len = cxt->out.top + 1 - bgn;
            TextBufferObj val = makeByName(bgn, len, cxt);
            if(val.type == OPT_FUNCTION_VAL
                && val.func->arity > 0
                && val.func->arity == val.func->captureCount) {
                //must capture
                TextBufferObj tmp = { .type = OPT_FUNC_CAP };
                pushStack(&tmpByNames, &tmp);
                pushStack(&tmpByNames, &val);
                tmp.type = OPT_PARAM;
                for(int i = cxt->decl->arity + cxt->decl->locals - 1; i >= 0; i--) {
                    tmp.param = i;
                    pushStack(&tmpByNames, &tmp);
                }
            } else {
                //no capture
                pushStack(&tmpByNames, &val);
            }
            cxt->out.top = bgn - 1;
        } else {
            //push to tmp stack unmodified
            while(cxt->out.top >= bgn) {
                pushStack(&tmpByNames, cxt->out.top--);
            }
        }
    }
    while(tmpByNames.top != tmpByNames.stack) {
        pushStack(&cxt->out, tmpByNames.top--);
    }
    lv_free(tmpByNames.stack);
}

//shunts over one op (or handles right bracket)
//optionally removing the top operator
//and checks function arity if applicable.
//Returns whether an error occurred.
static bool shuntOps(ExprContext* cxt) {

    TextBufferObj* tmp = cxt->ops.top;
    //if we need to push capture params onto the out stack,
    //we do so now, because we don't know the enclosing
    //function's arity at runtime.
    if(tmp->type == OPT_FUNCTION && tmp->func->arity > 0) {
        //ar holds the number of args passed in Lavender source
        int ar = *cxt->params.top--;
        collectByNameArgs(tmp, ar, cxt);
        //handle varargs
        if(tmp->func->varargs) {
            //make last arg + extra args on the end into a vector
            //zero vector args is allowed
            TextBufferObj obj;
            int lastParam = tmp->func->arity - tmp->func->captureCount - 1;
            assert(lastParam >= 0);
            obj.type = OPT_MAKE_VECT;
            obj.callArity = ar - lastParam;
            if(obj.callArity < 0) {
                LV_EXPR_ERROR = XPE_BAD_ARITY;
                cxt->head = *cxt->tok.top;
                return false;
            }
            //adjust arity to match fixed function arity
            ar = tmp->func->arity - tmp->func->captureCount;
            pushStack(&cxt->out, &obj);
        }
        //push any extra implicit capture args
        int end = arityFor(tmp->func, cxt->decl);
        for(int i = tmp->func->captureCount; i > 0; i--) {
            TextBufferObj obj;
            obj.type = OPT_PARAM;
            obj.param = end - i;
            assert(obj.param >= 0);
            pushStack(&cxt->out, &obj);
        }
        fixArityFirstArg(cxt);
        pushStack(&cxt->out, tmp);
        if((tmp->func->arity - tmp->func->captureCount) != ar) {
            LV_EXPR_ERROR = XPE_BAD_ARITY;
            cxt->head = *cxt->tok.top;
            return false;
        }
    } else if(tmp->type == OPT_FUNC_CALL2) {
        //get the proper param count
        int ar = *cxt->params.top--;
        if(ar < 0) {
            LV_EXPR_ERROR = XPE_BAD_ARITY;
            cxt->head = *cxt->tok.top;
            return false;
        } else {
            tmp->callArity = ar;
            pushStack(&cxt->out, tmp);
        }
    } else {
        pushStack(&cxt->out, tmp);
    }
    cxt->ops.top--;
    cxt->tok.top--;
    return true;
}

/**
 * A modified version of Dijkstra's shunting yard algorithm for
 * converting infix to postfix. The differences from the original
 * algorithm are:
 *  1. Support for Lavender's square bracket notation.
 *      See handleRightBracket() for details.
 *  2. Validation that the number of parameters passed
 *      to functions match arity.
 */
static void shuntingYard(TextBufferObj* obj, ExprContext* cxt) {

    if(obj->type == OPT_EMPTY_ARGS) {
        //set source args explicitly to 0 (or 1 for infix)
        //and make sure we are directly after a function
        if(cxt->ops.top->type != OPT_LITERAL && cxt->params.top != cxt->params.stack && *cxt->params.top < 0) {
            *cxt->params.top = -*cxt->params.top - 1;
            //to signal that a comma should NOT appear after
            pushStack(&cxt->out, obj);
        } else {
            //we aren't directly after a suitable function
            LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
            return;
        }
    } else if(obj->type == OPT_LITERAL) {
        switch(obj->literal) {
            case '(':
                //push left paren and push new param count
                pushStack(&cxt->ops, obj);
                pushToken(&cxt->tok, cxt->head);
                break;
            case '{':
                //push '{' to ops and push -1 to params
                fixArityFirstArg(cxt);
                pushStack(&cxt->ops, obj);
                pushToken(&cxt->tok, cxt->head);
                pushParam(&cxt->params, -1);
                pushParam(&cxt->maps, cxt->nesting);
                break;
            case '}': {
                //shunt ops onto out until '{'
                //then pop '{', get param count, and push VECT or MAP to out
                REQUIRE_NONEMPTY(cxt->ops);
                while(!isLiteral(cxt->ops.top, '{')) {
                    if(!shuntOps(cxt))
                        return;
                    REQUIRE_NONEMPTY(cxt->ops);
                }
                cxt->ops.top--;
                cxt->tok.top--;
                assert(cxt->params.top != cxt->params.stack);
                int arity = *cxt->params.top--;
                if(arity < 0) //{} construct
                    arity = 0;
                TextBufferObj vect = { .callArity = arity };
                if(-*cxt->maps.top - 1 == cxt->nesting) {
                    vect.type = OPT_MAKE_MAP;
                    cxt->maps.top--;
                    // cxt->tok.top--; //pop `mat`
                } else if(*cxt->maps.top - 1 == cxt->nesting) {
                    if(arity == 1 || arity == 0) {
                        //it's really a vect
                        vect.type = OPT_MAKE_VECT;
                        cxt->maps.top--;
                    } else {
                        //map did not specify a value!
                        LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                        return;
                    }
                } else {
                    vect.type = OPT_MAKE_VECT;
                }
                pushStack(&cxt->out, &vect);
                break;
            }
            case ')':
                //shunt over all operators until we hit left paren
                //if we underflow, then unbalanced parens
                REQUIRE_NONEMPTY(cxt->ops);
                while(!isLiteral(cxt->ops.top, '(')) {
                    if(!shuntOps(cxt))
                        return;
                    REQUIRE_NONEMPTY(cxt->ops);
                }
                cxt->ops.top--;
                cxt->tok.top--;
                break;
            case ',':
                //shunt ops until a left paren or left brace
                REQUIRE_NONEMPTY(cxt->ops);
                while(!isLiteral(cxt->ops.top, '(')
                    && !isLiteral(cxt->ops.top, '{')) {
                    if(!shuntOps(cxt))
                        return;
                    REQUIRE_NONEMPTY(cxt->ops);
                }
                //there cannot be more args after () in the current function
                //(this is why we shunt ops over first!)
                if(cxt->out.top->type == OPT_EMPTY_ARGS) {
                    LV_EXPR_ERROR = XPE_EXPECT_INF;
                    return;
                }
                //flip from value to key for maps
                if(-*cxt->maps.top == cxt->nesting) {
                    *cxt->maps.top = -*cxt->maps.top;
                } else if(*cxt->maps.top == cxt->nesting) {
                    if(*cxt->params.top != 1) {
                        //map did not specify a value!
                        LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                        return;
                    } else {
                        //not a map, but a vect
                        cxt->maps.top--;
                    }
                }
                REQUIRE_NONEMPTY(cxt->params);
                ++*cxt->params.top;
                break;
        }
    } else if(obj->type == OPT_FUNCTION_VAL && obj->func->captureCount > 0) {
        //push capture params onto stack, then push obj, then push CAP
        //out: ... cap1 cap2 .. capn obj CAP ...
        //self capture does not capture locals
        // int ar = (obj->func == cxt->decl) ? cxt->decl->arity
        //     : (cxt->decl->arity + cxt->decl->locals);

        int ar = arityFor(obj->func, cxt->decl);
        for(int i = obj->func->captureCount; i > 0; i--) {
            TextBufferObj tbo;
            tbo.type = OPT_PARAM;
            tbo.param = ar - i;
            assert(tbo.param >= 0);
            pushStack(&cxt->out, &tbo);
        }
        pushStack(&cxt->out, obj);
        TextBufferObj cap;
        cap.type = OPT_FUNC_CAP;
        fixArityFirstArg(cxt);
        pushStack(&cxt->out, &cap);
    } else if(obj->type == OPT_FUNC_CALL2) {
        //special case of the else branch
        //call 2 fixing=FIX_LEFT_IN
        while(cxt->ops.top != cxt->ops.stack && (compare(obj, cxt->ops.top) - 1) < 0) {
            if(!shuntOps(cxt))
                return;
        }
        // if(cxt->expectOperand) {
        if(obj->fromType != OPT_EMPTY_ARGS) {
            //nonzero arity version
            pushStack(&cxt->ops, obj);
            pushToken(&cxt->tok, cxt->head);
            pushToken(&cxt->tok, cxt->head);
            //func call 2 is an 'infix' operator
            pushParam(&cxt->params, -2);
            TextBufferObj obj2 = *obj;
            obj2.type = obj->fromType;
            shuntingYard(&obj2, cxt);
        } else {
            //zero arity version
            //push directly to out, since there are no args
            obj->callArity = 1;
            pushStack(&cxt->out, obj);
            //no need to push param because it's already on out
        }
    } else if(obj->type != OPT_FUNCTION || obj->func->arity == 0) {
        //it's a value, shunt it over
        fixArityFirstArg(cxt);
        pushStack(&cxt->out, obj);
    } else {
        assert(obj->type == OPT_FUNCTION);
        //shunt over the ops of greater precedence if right assoc.
        //and greater or equal precedence if left assoc.
        int sub = (obj->func->fixing != FIX_LEFT_IN ? 0 : 1);
        while(cxt->ops.top != cxt->ops.stack && (compare(obj, cxt->ops.top) - sub) < 0) {
            if(!shuntOps(cxt))
                return;
        }
        //push the actual operator on ops
        pushStack(&cxt->ops, obj);
        pushToken(&cxt->tok, cxt->head);
        //negative numbers are fix for when the first params aren't there.
        //We cannot detect the initial params with commas, so we must rely
        //on the params themselves to detect this negative value and set
        //it to positive when they are push onto the output stack.
        //In this way we can detect the error when there is no initial param.
        if(obj->func->fixing == FIX_PRE)
            pushParam(&cxt->params, -1);
        else if(obj->func->arity == 1)
            pushParam(&cxt->params, 1);
        else
            pushParam(&cxt->params, -2);
    }
}
