#include "operator.h"
#include "lavender.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#define INIT_STACK_LEN 16
typedef struct TextStack {
    size_t len;
    TextBufferObj* top;
    TextBufferObj* stack;
} TextStack;

typedef struct IntStack {
    size_t len;
    int* top;
    int* stack;
} IntStack;

/** Expression context passed to functions. */
typedef struct ExprContext {
    Token* head;            //the current token
    Operator* decl;         //the current function declaration
    char* startOfName;      //the beginning of the simple name
    bool expectOperand;     //do we expect an operand or an operator
    int nesting;            //how nested in brackets we are
    TextStack ops;          //the temporary operator stack
    TextStack out;          //the output stack
    IntStack params;        //the parameter stack
} ExprContext;

static int compare(TextBufferObj* a, TextBufferObj* b);
static void parseLiteral(TextBufferObj* obj, ExprContext* cxt);
static void parseIdent(TextBufferObj* obj, ExprContext* cxt);
static void parseSymbol(TextBufferObj* obj, ExprContext* cxt);
static void parseQualName(TextBufferObj* obj, ExprContext* cxt);
static void parseNumber(TextBufferObj* obj, ExprContext* cxt);
static void parseString(TextBufferObj* obj, ExprContext* cxt);
static void parseFuncValue(TextBufferObj* obj, ExprContext* cxt);
static void parseTextObj(TextBufferObj* obj, ExprContext* cxt); //calls above functions
//runs one cycle of shunting yard
static void shuntingYard(TextBufferObj* obj, ExprContext* cxt);
    
static int getLexicographicPrecedence(char c) {
    
    switch(c) {
        case '|': return 1;
        case '^': return 2;
        case '&': return 3;
        case '!':
        case '=': return 4;
        case '>':
        case '<': return 5;
        case '#': return 6; //':' was changed to '#' earlier
        case '-':
        case '+': return 7;
        case '%':
        case '/':
        case '*': return 8;
        case '~':
        case '?': return 9;
        default:  return 0;
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
        int ar = (a->type != OPT_LITERAL &&
            (a->type != OPT_FUNCTION || a->func->arity == 0));
        int br = (b->type != OPT_LITERAL &&
            (b->type != OPT_FUNCTION || b->func->arity == 0));
        if(ar || br) {
            assert(false);
            return ar - br;
        }
    }
    //close groupers ']' and ')' have next highest
    {
        int ac = (isLiteral(a, ')') || isLiteral(a, ']'));
        int bc = (isLiteral(b, ')') || isLiteral(b, ']'));
        if(ac || bc)
            return ac - bc;
    }
    //openers '(' and '[' have the lowest
    if(isLiteral(a, '(') || isLiteral(a, '['))
        return -1;
    if(isLiteral(b, '(') || isLiteral(b, '['))
        return 1;
    assert(a->type == OPT_FUNCTION);
    assert(b->type == OPT_FUNCTION);
    //check fixing
    //prefix > infix = postfix
    {
        int afix = (a->func->fixing == FIX_PRE);
        int bfix = (b->func->fixing == FIX_PRE);
        if(afix ^ bfix)
            return afix - bfix;
        if(afix) //prefix functions always have equal precedence
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
    obj->literal = cxt->head->value[0];
    switch(obj->literal) {
        case '(':
        case '[':
            cxt->nesting++;
            //open groupings are "operands"
            if(!cxt->expectOperand) {
                LV_OP_ERROR = OPE_EXPECT_PRE;
            }
            break;
        case ')':
        case ']':
            cxt->nesting--;
            //fallthrough
        case ',':
            //close groupings are "operators"
            if(cxt->expectOperand) {
                LV_OP_ERROR = OPE_EXPECT_INF;
            }
            break;
        default:
            LV_OP_ERROR = OPE_UNEXPECT_TOKEN;
    }
    if(obj->literal == ']')
        cxt->expectOperand = true;
}

static void parseSymbolImpl(TextBufferObj* obj, FuncNamespace ns, char* name, ExprContext* cxt) {
    
    //we find the function with the simple name
    //in the innermost scope possible by going
    //through outer scopes until we can't find
    //a function definition.
    size_t valueLen = strlen(name);
    char* nsbegin = cxt->decl->name;
    Operator* func;
    Operator* test = NULL;
    do {
        func = test;
        if(cxt->startOfName == nsbegin)
            break; //we did all the namespaces
        //get the function with the name in the scope
        char* fname = concat(nsbegin,   //beginning of scope
            cxt->startOfName - nsbegin,   //length of scope name
            name,                       //name to check
            valueLen);                  //length of name
        test = lv_op_getOperator(fname, ns);
        lv_free(fname);
        nsbegin = strchr(nsbegin, ':') + 1;
    } while(test);
    //test is null. func should contain the function
    if(!func) {
        //that name does not exist!
        LV_OP_ERROR = OPE_NAME_NOT_FOUND;
        return;
    }
    obj->type = OPT_FUNCTION;
    obj->func = func;
    //toggle if RHS is true
    cxt->expectOperand ^= (!cxt->expectOperand || func->arity == 0);
}

static void parseSymbol(TextBufferObj* obj, ExprContext* cxt) {
    
    FuncNamespace ns = cxt->expectOperand ? FNS_PREFIX : FNS_INFIX;
    parseSymbolImpl(obj, ns, cxt->head->value, cxt);
}

static void parseQualNameImpl(TextBufferObj* obj, FuncNamespace ns, char* name, ExprContext* cxt) {
    
    //since it's a qualified name, we don't need to
    //guess what function it could be!
    Operator* func = lv_op_getOperator(name, ns);
    if(!func) {
        //404 func not found
        LV_OP_ERROR = OPE_NAME_NOT_FOUND;
        return;
    }
    obj->type = OPT_FUNCTION;
    obj->func = func;
    //toggle if RHS is true
    cxt->expectOperand ^= (!cxt->expectOperand || func->arity == 0);
}

static void parseQualName(TextBufferObj* obj, ExprContext* cxt) {
    
    FuncNamespace ns = cxt->expectOperand ? FNS_PREFIX : FNS_INFIX;
    parseQualNameImpl(obj, ns, cxt->head->value, cxt);
}

static void parseFuncValue(TextBufferObj* obj, ExprContext* cxt) {
    
    if(!cxt->expectOperand) {
        LV_OP_ERROR = OPE_EXPECT_PRE;
        return;
    }
    size_t len = strlen(cxt->head->value);
    FuncNamespace ns;
    //values ending in '\' are infix functions
    //subtract 1 from length so we can skip the initial '\'
    if(cxt->head->value[len - 1] == '\\') {
        ns = FNS_INFIX;
        //substringing
        cxt->head->value[len - 1] = '\0';
    } else {
        ns = FNS_PREFIX;
    }
    if(cxt->head->type == TTY_QUAL_FUNC_VAL)
        parseQualNameImpl(obj, ns, cxt->head->value + 1, cxt);
    else
        parseSymbolImpl(obj, ns, cxt->head->value + 1, cxt);
    //undo substringing
    if(ns == FNS_INFIX)
        cxt->head->value[len - 1] = '\\';
    obj->type = OPT_FUNCTION_VAL;
    cxt->expectOperand = false;
}

static void parseIdent(TextBufferObj* obj, ExprContext* cxt) {
    
    if(cxt->expectOperand) {
        //is this a def?
        if(strcmp(cxt->head->value, "def") == 0) {
            //todo
        }
        //try parameter names first
        for(int i = 0; i < cxt->decl->arity; i++) {
            if(strcmp(cxt->head->value, cxt->decl->params[i].name) == 0) {
                //save param name
                obj->type = OPT_PARAM;
                obj->param = i;
                cxt->expectOperand = false;
                return;
            }
        }
    }
    //not a parameter, try a function
    parseSymbol(obj, cxt);
}

static void parseNumber(TextBufferObj* obj, ExprContext* cxt) {
    
    if(!cxt->expectOperand) {
        LV_OP_ERROR = OPE_EXPECT_PRE;
        return;
    }
    double num = strtod(cxt->head->value, NULL);
    obj->type = OPT_NUMBER;
    obj->number = num;
    cxt->expectOperand = false;
}

static void parseString(TextBufferObj* obj, ExprContext* cxt) {
    
    if(!cxt->expectOperand) {
        LV_OP_ERROR = OPE_EXPECT_PRE;
        return;
    }
    char* c = cxt->head->value + 1; //skip open quote
    LvString* newStr = lv_alloc(sizeof(LvString) + strlen(c) + 1);
    size_t len = 0;
    while(*c != '"') {
        if(*c == '\\') {
            //handle escape sequences
            switch(*++c) {
                case 'n': newStr->value[len] = '\n';
                    break;
                case 't': newStr->value[len] = '\t';
                    break;
                case '"': newStr->value[len] = '"';
                    break;
                case '\'': newStr->value[len] = '\'';
                    break;
                case '\\': newStr->value[len] = '\\';
                    break;
                default:
                    assert(false);
            }
        } else
            newStr->value[len] = *c;
        c++;
        len++;
    }
    newStr = lv_realloc(newStr, sizeof(LvString) + len + 1);
    newStr->value[len] = '\0';
    newStr->len = len;
    obj->type = OPT_STRING;
    obj->str = newStr;
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
        case TTY_STRING:
            parseString(obj, cxt);
            break;
        case TTY_FUNC_VAL:
        case TTY_QUAL_FUNC_VAL:
            parseFuncValue(obj, cxt);
            break;
        case TTY_FUNC_SYMBOL:
            LV_OP_ERROR = OPE_UNEXPECT_TOKEN;
            break;
    }
}

static void pushStack(TextStack* stack, TextBufferObj* obj) {
    
    if(stack->top + 1 == stack->stack + stack->len) {
        stack->len *= 2;
        stack->stack = lv_realloc(stack->stack,
            stack->len * sizeof(TextBufferObj));
    }
    *++stack->top = *obj;
}

static void pushParam(IntStack* stack, int num) {
    
    if(stack->top + 1 == stack->stack + stack->len) {
        stack->len *= 2;
        stack->stack = lv_realloc(stack->stack,
            stack->len * sizeof(TextBufferObj));
    }
    *++stack->top = num;
}

#define REQUIRE_NONEMPTY(s) \
    if(s.top == s.stack) { LV_OP_ERROR = OPE_UNBAL_GROUP; return; } else (void)0

/**
 * Handles Lavender square bracket notation.
 * Lavender requires that expressions in square brackets
 * be moved verbatim to the right of the next sub-expression.
 */
void handleRightBracket(ExprContext* cxt) {
    
    assert(isLiteral(cxt->ops.top, ']'));
    //shunt over operators
    do {
        cxt->ops.top--;
        REQUIRE_NONEMPTY(cxt->ops);
        if(isLiteral(cxt->ops.top, ']'))
            handleRightBracket(cxt);
        else
            pushStack(&cxt->out, cxt->ops.top);
    } while(!isLiteral(cxt->ops.top, '['));
    //todo add call
    cxt->ops.top--; //pop '['
}

/**
 * A modified version of Dijkstra's shunting yard algorithm for
 * converting infix to postfix. The differences from the original
 * algorithm are:
 *  1. Support for Lavender's square bracket notation.
 *      See handleRightBracket() for details.
 *  2. Validation that the number of parameters passed
 *      to functions match arity. (NYI)
 */
void shuntingYard(TextBufferObj* obj, ExprContext* cxt) {
    
    if(obj->type == OPT_LITERAL) {
        switch(obj->literal) {
            case '(':
                //push left paren and push new param count
                pushStack(&cxt->ops, obj);
                pushParam(&cxt->params, 0);
                break;
            case '[':
                //push to op stack and add to out
                pushStack(&cxt->ops, obj);
                pushStack(&cxt->out, obj);
                pushParam(&cxt->params, 0);
                break;
            case ']':
                //pop out onto op until '['
                //then push ']' onto op
                while(!isLiteral(cxt->out.top, '[')) {
                    REQUIRE_NONEMPTY(cxt->out);
                    pushStack(&cxt->ops, cxt->out.top--);
                }
                REQUIRE_NONEMPTY(cxt->out);
                cxt->out.top--;
                pushParam(&cxt->params, 0);
                pushStack(&cxt->ops, obj);
                break;
            case ')':
                //shunt over all operators until we hit left paren
                //if we underflow, then unbalanced parens
                while(!isLiteral(cxt->ops.top, '(')) {
                    REQUIRE_NONEMPTY(cxt->ops);
                    if(isLiteral(cxt->ops.top, ']'))
                        handleRightBracket(cxt);
                    else
                        pushStack(&cxt->out, cxt->ops.top--);
                }
                REQUIRE_NONEMPTY(cxt->ops);
                cxt->ops.top--;
                break;
        }
    } else if(obj->type != OPT_FUNCTION || obj->func->arity == 0) {
        //it's a value, shunt it over
        pushStack(&cxt->out, obj);
        ++*cxt->params.top;
    } else if(obj->type == OPT_FUNCTION) {
        //shunt over the ops of greater precedence if right assoc.
        //and greater or equal precedence if left assoc.
        int sub = (obj->func->fixing == FIX_RIGHT_IN ? 0 : 1);
        while(cxt->ops.top != cxt->ops.stack && (compare(obj, cxt->ops.top) - sub) < 0) {
            if(isLiteral(cxt->ops.top, ']'))
                handleRightBracket(cxt);
            else
                pushStack(&cxt->out, cxt->ops.top--);
        }
        //push the actual operator on ops
        pushStack(&cxt->ops, obj);
    } else {
        assert(false);
    }
}

void lv_op_parseExpr(Token* head, Operator* decl, TextBufferObj** res, size_t* len) {
    
    if(LV_OP_ERROR)
        return;
    //set up required environment
    //this is function local because
    //recursive calls are possible
    ExprContext cxt;
    cxt.head = head;
    cxt.decl = decl;
    cxt.startOfName = strrchr(decl->name, ':') + 1;
    cxt.expectOperand = true;
    cxt.nesting = 0;
    cxt.out.len = INIT_STACK_LEN;
    cxt.out.stack = lv_alloc(INIT_STACK_LEN * sizeof(TextBufferObj));
    cxt.out.top = cxt.out.stack;
    cxt.ops.len = INIT_STACK_LEN;
    cxt.ops.stack = lv_alloc(INIT_STACK_LEN * sizeof(TextBufferObj));
    cxt.ops.top = cxt.ops.stack;
    cxt.params.len = INIT_STACK_LEN;
    cxt.params.stack = lv_alloc(INIT_STACK_LEN * sizeof(int));
    cxt.params.top = cxt.params.stack;
    //loop over each token until we reach the end of the expression
    //(end-of-stream, closing grouper ')', or expression split ';') (expr-split NYI)
    do {
        //get the next text object
        TextBufferObj obj;
        parseTextObj(&obj, &cxt);
        if(!LV_OP_ERROR)
            shuntingYard(&obj, &cxt);
        if(LV_OP_ERROR) {
            lv_op_free(cxt.out.stack, cxt.out.top - cxt.out.stack + 1);
            lv_op_free(cxt.ops.stack, cxt.ops.top - cxt.ops.stack + 1);
            lv_free(cxt.params.stack);
            return;
        }
        cxt.head = cxt.head->next;
    } while(cxt.head && cxt.nesting >= 0);  //todo implement end on ';'
    //get leftover ops over
    while(cxt.ops.top != cxt.ops.stack) {
        if(isLiteral(cxt.ops.top, ']'))
            handleRightBracket(&cxt);
        else
            pushStack(&cxt.out, cxt.ops.top--);
    }
    *res = cxt.out.stack;
    *len = cxt.out.top - cxt.out.stack + 1;
    //calling plain lv_free is ok because ops is empty
    lv_free(cxt.ops.stack);
    lv_free(cxt.params.stack);
}

void lv_op_free(TextBufferObj* obj, size_t len) {
    
    for(size_t i = 1; i < len; i++) {
        if(obj[i].type == OPT_STRING)
            lv_free(obj[i].str);
    }
    lv_free(obj);
}
