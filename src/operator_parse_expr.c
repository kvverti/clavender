#include "operator.h"
#include "lavender.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static Token* pe_head;        //the current token
static Operator* pe_decl;     //the current function declaration
static char* pe_startOfName;  //the beginning of the simple name
static bool pe_expectOperand; //do we expect an operand or an operator
static int pe_nesting;        //how nested in brackets we are

static int compare(TextBufferObj* a, TextBufferObj* b);
static void parseLiteral(TextBufferObj* obj);
static void parseIdent(TextBufferObj* obj);
static void parseSymbol(TextBufferObj* obj);
static void parseQualName(TextBufferObj* obj);
static void parseNumber(TextBufferObj* obj);
static void parseString(TextBufferObj* obj);
static void parseFuncValue(TextBufferObj* obj);
static void parseTextObj(TextBufferObj* obj); //calls above functions
//runs one cycle of shunting yard
static void shuntingYard(TextBufferObj* obj);
    
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

static void parseLiteral(TextBufferObj* obj) {
    
    obj->type = OPT_LITERAL;
    obj->literal = pe_head->value[0];
    switch(obj->literal) {
        case '(':
        case '[':
            pe_nesting++;
            //open groupings are "operands"
            if(!pe_expectOperand) {
                LV_OP_ERROR = OPE_EXPECT_PRE;
            }
            break;
        case ')':
        case ']':
            pe_nesting--;
            //fallthrough
        case ',':
            //close groupings are "operators"
            if(pe_expectOperand) {
                LV_OP_ERROR = OPE_EXPECT_INF;
            }
            break;
        default:
            LV_OP_ERROR = OPE_UNEXPECT_TOKEN;
    }
    if(obj->literal == ']')
        pe_expectOperand = true;
}

static void parseSymbolImpl(TextBufferObj* obj, FuncNamespace ns, char* name) {
    
    //we find the function with the simple name
    //in the innermost scope possible by going
    //through outer scopes until we can't find
    //a function definition.
    size_t valueLen = strlen(name);
    char* nsbegin = pe_decl->name;
    Operator* func;
    Operator* test = NULL;
    do {
        func = test;
        if(pe_startOfName == nsbegin)
            break; //we did all the namespaces
        //get the function with the name in the scope
        char* fname = concat(nsbegin,   //beginning of scope
            pe_startOfName - nsbegin,   //length of scope name
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
    pe_expectOperand ^= (!pe_expectOperand || func->arity == 0);
}

static void parseSymbol(TextBufferObj* obj) {
    
    FuncNamespace ns = pe_expectOperand ? FNS_PREFIX : FNS_INFIX;
    parseSymbolImpl(obj, ns, pe_head->value);
}

static void parseQualNameImpl(TextBufferObj* obj, FuncNamespace ns, char* name) {
    
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
    pe_expectOperand ^= (!pe_expectOperand || func->arity == 0);
}

static void parseQualName(TextBufferObj* obj) {
    
    FuncNamespace ns = pe_expectOperand ? FNS_PREFIX : FNS_INFIX;
    parseQualNameImpl(obj, ns, pe_head->value);
}

static void parseFuncValue(TextBufferObj* obj) {
    
    if(!pe_expectOperand) {
        LV_OP_ERROR = OPE_EXPECT_PRE;
        return;
    }
    size_t len = strlen(pe_head->value);
    FuncNamespace ns;
    //values ending in '\' are infix functions
    //subtract 1 from length so we can skip the initial '\'
    if(pe_head->value[len - 1] == '\\') {
        ns = FNS_INFIX;
        //substringing
        pe_head->value[len - 1] = '\0';
    } else {
        ns = FNS_PREFIX;
    }
    if(pe_head->type == TTY_QUAL_FUNC_VAL)
        parseQualNameImpl(obj, ns, pe_head->value + 1);
    else
        parseSymbolImpl(obj, ns, pe_head->value + 1);
    //undo substringing
    if(ns == FNS_INFIX)
        pe_head->value[len - 1] = '\\';
    obj->type = OPT_FUNCTION_VAL;
    pe_expectOperand = false;
}

static void parseIdent(TextBufferObj* obj) {
    
    if(pe_expectOperand) {
        //is this a def?
        if(strcmp(pe_head->value, "def") == 0) {
            //todo
        }
        //try parameter names first
        for(int i = 0; i < pe_decl->arity; i++) {
            if(strcmp(pe_head->value, pe_decl->params[i].name) == 0) {
                //save param name
                obj->type = OPT_PARAM;
                obj->param = i;
                pe_expectOperand = false;
                return;
            }
        }
    }
    //not a parameter, try a function
    parseSymbol(obj);
}

static void parseNumber(TextBufferObj* obj) {
    
    if(!pe_expectOperand) {
        LV_OP_ERROR = OPE_EXPECT_PRE;
        return;
    }
    double num = strtod(pe_head->value, NULL);
    obj->type = OPT_NUMBER;
    obj->number = num;
    pe_expectOperand = false;
}

static void parseString(TextBufferObj* obj) {
    
    if(!pe_expectOperand) {
        LV_OP_ERROR = OPE_EXPECT_PRE;
        return;
    }
    char* c = pe_head->value + 1; //skip open quote
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
    pe_expectOperand = false;
}

static void parseTextObj(TextBufferObj* obj) {
    
    switch(pe_head->type) {
        case TTY_LITERAL:
            parseLiteral(obj);
            break;
        case TTY_IDENT:
            parseIdent(obj);
            break;
        case TTY_SYMBOL:
            parseSymbol(obj);
            break;
        case TTY_QUAL_IDENT:
        case TTY_QUAL_SYMBOL:
            parseQualName(obj);
            break;
        case TTY_NUMBER:
            parseNumber(obj);
            break;
        case TTY_STRING:
            parseString(obj);
            break;
        case TTY_FUNC_VAL:
        case TTY_QUAL_FUNC_VAL:
            parseFuncValue(obj);
            break;
        case TTY_FUNC_SYMBOL:
            LV_OP_ERROR = OPE_UNEXPECT_TOKEN;
            break;
    }
}

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

#define INIT_STACK_LEN 16

static TextStack ops;
static TextStack out;
static IntStack params;

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
void handleRightBracket() {
    
    assert(isLiteral(ops.top, ']'));
    ops.top--; //pop ']'
    //shunt over operators
    while(!isLiteral(ops.top, '[')) {
        REQUIRE_NONEMPTY(ops);
        if(isLiteral(ops.top, ']'))
            handleRightBracket();
        else
            pushStack(&out, ops.top--);
    }
    //todo add call
    ops.top--; //pop '['
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
void shuntingYard(TextBufferObj* obj) {
    
    if(obj->type == OPT_LITERAL) {
        switch(obj->literal) {
            case '(':
                //push left paren and push new param count
                pushStack(&ops, obj);
                pushParam(&params, 0);
                break;
            case '[':
                //push to op stack and add to out
                pushStack(&ops, obj);
                pushStack(&out, obj);
                pushParam(&params, 0);
                break;
            case ']':
                //pop out onto op until '['
                //then push ']' onto op
                while(!isLiteral(out.top, '[')) {
                    REQUIRE_NONEMPTY(out);
                    pushStack(&ops, out.top--);
                }
                REQUIRE_NONEMPTY(out);
                out.top--;
                pushParam(&params, 0);
                pushStack(&ops, obj);
                break;
            case ')':
                //shunt over all operators until we hit left paren
                //if we underflow, then unbalanced parens
                while(!isLiteral(ops.top, '(')) {
                    REQUIRE_NONEMPTY(ops);
                    if(isLiteral(ops.top, ']'))
                        handleRightBracket();
                    else
                        pushStack(&out, ops.top--);
                }
                REQUIRE_NONEMPTY(ops);
                ops.top--;
                break;
        }
    } else if(obj->type != OPT_FUNCTION || obj->func->arity == 0) {
        //it's a value, shunt it over
        pushStack(&out, obj);
        ++*params.top;
    } else if(obj->type == OPT_FUNCTION) {
        //shunt over the ops of greater precedence if right assoc.
        //and greater or equal precedence if left assoc.
        int sub = (obj->func->fixing == FIX_RIGHT_IN ? 0 : 1);
        while(ops.top != ops.stack && (compare(obj, ops.top) - sub) < 0) {
            if(isLiteral(ops.top, ']'))
                handleRightBracket();
            else
                pushStack(&out, ops.top--);
        }
        //push the actual operator on ops
        pushStack(&ops, obj);
    } else {
        assert(false);
    }
}

#undef REQUIRE_NONEMPTY

void lv_op_parseExpr(Token* head, Operator* decl, TextBufferObj** res, size_t* len) {
    
    if(LV_OP_ERROR)
        return;
    //set up global vars
    pe_head = head;
    pe_decl = decl;
    pe_startOfName = strrchr(decl->name, ':') + 1;
    pe_expectOperand = true;
    pe_nesting = 0;
    out.len = INIT_STACK_LEN;
    out.stack = lv_alloc(INIT_STACK_LEN * sizeof(TextBufferObj));
    out.top = out.stack;
    ops.len = INIT_STACK_LEN;
    ops.stack = lv_alloc(INIT_STACK_LEN * sizeof(TextBufferObj));
    ops.top = ops.stack;
    params.len = INIT_STACK_LEN;
    params.stack = lv_alloc(INIT_STACK_LEN * sizeof(int));
    params.top = params.stack;
    do {
        //get the next text object
        TextBufferObj obj;
        parseTextObj(&obj);
        if(!LV_OP_ERROR)
            shuntingYard(&obj);
        if(LV_OP_ERROR) {
            lv_op_free(out.stack, out.top - out.stack + 1);
            lv_op_free(ops.stack, ops.top - ops.stack + 1);
            lv_free(params.stack);
            return;
        }
        pe_head = pe_head->next;
    } while(pe_head && pe_nesting >= 0);
    //get leftover ops over
    while(ops.top != ops.stack) {
        if(isLiteral(ops.top, ']'))
            handleRightBracket();
        else
            pushStack(&out, ops.top--);
    }
    *res = out.stack;
    *len = out.top - out.stack + 1;
    lv_free(ops.stack);
    lv_free(params.stack);
}

void lv_op_free(TextBufferObj* obj, size_t len) {
    
    for(size_t i = 1; i < len; i++) {
        if(obj[i].type == OPT_STRING)
            lv_free(obj[i].str);
    }
    lv_free(obj);
}
