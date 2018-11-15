#include "lavender.h"
#include "expression.h"
#include "operator.h"
#include "builtin.h"
#include "command.h"
#include "dynbuffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

bool lv_debug = false;
char* lv_filepath = ".";
char* lv_mainFile = NULL;
size_t lv_maxStackSize = 512 * 1024; //512KiB
struct LvMainArgs lv_mainArgs = { NULL, 0 };

static void readInput(FILE* in, bool repl);
static size_t jumpAndLink(Operator* func);
static void runCycle(void);

static DynBuffer stack; //of TextBufferObj
static size_t pc;   //program counter
static size_t fp;   //frame pointer: index of the first argument
static Operator atFunc; //built in sys:__at__
static Builtin sysLt; //built in sys:__lt__
static Operator scope = { .type = FUN_FWD_DECL };

static void push(TextBufferObj* obj) {

    if(obj->type & LV_DYNAMIC)
        ++*obj->refCount;
    if(lv_maxStackSize
        && (stack.len + 1) == stack.cap
        && stack.len >= lv_maxStackSize) {
        //we've exceeded the maximum stack size
        LvString* inst = lv_tb_getString(&TEXT_BUFFER[pc]);
        LvString* arg = lv_tb_getString(obj);
        printf("Stack overflow: pc=%lu, inst=%s, toPush=%s\n",
            pc, inst->value, arg->value);
        if(inst->refCount == 0)
            lv_free(inst);
        if(arg->refCount == 0)
            lv_free(arg);
        lv_shutdown();
    }
    lv_buf_push(&stack, obj);
}

static void popAll(size_t numToPop) {

    TextBufferObj* start = lv_buf_get(&stack, stack.len - numToPop);
    lv_expr_cleanup(start, numToPop);
    stack.len -= numToPop;
}

static TextBufferObj removeTop(void) {

    TextBufferObj res;
    lv_buf_pop(&stack, &res);
    if(res.type & LV_DYNAMIC)
        --*res.refCount;
    return res;
}

//simple linear storage should be enough
//for the relatively small number of namespaces
static DynBuffer importedFiles; //of char*

static bool addFile(char* file) {

    for(size_t i = 0; i < importedFiles.len; i++) {
        char* str = *(char**)lv_buf_get(&importedFiles, i);
        if(strcmp(str, file) == 0)
            return false; //already imported
    }
    size_t len = strlen(file) + 1;
    char* tmp = lv_alloc(len);
    memcpy(tmp, file, len);
    lv_buf_push(&importedFiles, &tmp);
    return true;
}

void lv_run(void) {

    lv_startup();
    bool load = lv_readFile("sys") && lv_readFile("global");
    if(load) {
        lv_globalEquals.type = OPT_FUNCTION;
        lv_globalEquals.func = lv_op_getOperator("global:=", FNS_INFIX);
        lv_globalHash.type = OPT_FUNCTION;
        lv_globalHash.func = lv_op_getOperator("global:hash", FNS_PREFIX);
        lv_globalLt.type = OPT_FUNCTION;
        lv_globalLt.func = lv_op_getOperator("global:<", FNS_INFIX);
    }
    if(!load) {
        puts("Fatal: stdlib does not exist");
    } else if(lv_mainFile) {
        bool read = lv_readFile(lv_mainFile);
        if(!read) {
            puts("Error reading main file");
        } else {
            //get the main function
            char mainName[] = ":main";
            char entryPointName[strlen(lv_mainFile) + sizeof(mainName)];
            strcpy(entryPointName, lv_mainFile);
            strcat(entryPointName, mainName);
            Operator* entryPoint = lv_op_getOperator(entryPointName, FNS_PREFIX);
            if(entryPoint && entryPoint->arity == 1) {
                //box params
                TextBufferObj args;
                args.type = OPT_VECT;
                args.vect = lv_alloc(sizeof(LvVect) + lv_mainArgs.count * sizeof(TextBufferObj));
                args.vect->refCount = 0;
                args.vect->len = lv_mainArgs.count;
                for(size_t i = 0; i < args.vect->len; i++) {
                    size_t argLen = strlen(lv_mainArgs.args[i]);
                    LvString* str =
                        lv_alloc(sizeof(LvString) + argLen + 1);
                    str->refCount = 1;
                    str->len = argLen;
                    strcpy(str->value, lv_mainArgs.args[i]);
                    args.vect->data[i].type = OPT_STRING;
                    args.vect->data[i].str = str;
                }
                //call main function
                push(&args);
                //there's no stack frame to keep track of
                //and there's no expression in the text buffer
                //so we have to go by stack size.
                jumpAndLink(entryPoint);
                do {
                    runCycle();
                } while(stack.len != 1);
                //print result
                TextBufferObj obj;
                lv_buf_pop(&stack, &obj);
                LvString* str = lv_tb_getString(&obj);
                puts(str->value);
                if(str->refCount == 0) {
                    lv_free(str);
                }
                lv_expr_cleanup(&obj, 1);
            } else {
                //cannot call function
                puts("Main function missing or incompatible");
            }
        }
    } else {
        if(lv_debug)
            puts("Running in debug mode");
        puts("Lavender runtime v. 1.0 by Chris Nero\n"
             "Open source at https://github.com/kvverti/clavender\n"
             "Enter function definitions or expressions");
        while(!feof(stdin)) {
            lv_repl();
        }
    }
    lv_shutdown();
}

void* lv_alloc(size_t size) {

    void* value = malloc(size);
    if(!value) {
        printf("Allocation failed: %lu bytes\n", size);
        lv_shutdown();
    }
    return value;
}

void* lv_realloc(void* ptr, size_t size) {

    void* tmp = realloc(ptr, size);
    if(!tmp) {
        free(ptr);
        printf("Allocation failed: %lu bytes\n", size);
        lv_shutdown();
    }
    return tmp;
}

void lv_free(void* ptr) {

    free(ptr);
}

void lv_startup(void) {

    pc = fp = 0;
    lv_buf_init(&stack, sizeof(TextBufferObj));
    lv_buf_init(&importedFiles, sizeof(char*));
    lv_tkn_onStartup();
    lv_op_onStartup();
    lv_tb_onStartup();
    lv_blt_onStartup();
    lv_cmd_onStartup();
    memset(&atFunc, 0, sizeof(atFunc));
    atFunc.type = FUN_BUILTIN;
    atFunc.name = "sys:__at__";
    atFunc.arity = 2;
    atFunc.builtin = lv_blt_getIntrinsic("sys:__at__");
    assert(atFunc.builtin);
    sysLt = lv_blt_getIntrinsic("sys:__lt__");
    assert(sysLt);
}

void lv_shutdown(void) {

    lv_cmd_onShutdown();
    lv_blt_onShutdown();
    lv_tb_onShutdown();
    lv_op_onShutdown();
    lv_tkn_onShutdown();
    lv_expr_cleanup(stack.data, stack.len);
    for(size_t i = 0; i < importedFiles.len; i++) {
        lv_free(*(char**)lv_buf_get(&importedFiles, i));
    }
    lv_free(importedFiles.data);
    lv_free(stack.data);
    exit(0);
}

void lv_repl(void) {

    readInput(stdin, true);
}

static bool isFuncDef(Token* head) {

    //functions may have one level of parens
    assert(head);
    if(head->type == TTY_LITERAL && head->start[0] == '(') {
        head = head->next;
    }
    return head && lv_tkn_cmp(head, "def") == 0;
}

static void printError(Token* err, char* groupMessage) {

    printf("On line %d: %s: %s\n",
        err ? err->lineNumber : 0,
        groupMessage,
        lv_expr_getError(LV_EXPR_ERROR));
    if(err) {
        printf("    %s", err->line);
        char arrows[err->len + 1];
        memset(arrows, '^', err->len);
        arrows[err->len] = '\0';
        printf("    %*s\n", (int)((err->start - err->line) + err->len), arrows);
    }
}

static void printTokenError(char* groupMessage) {

    printf("On line %d: %s: %s\n",
        lv_tkn_errContext.lineNumber,
        groupMessage,
        lv_tkn_getError(LV_TKN_ERROR));
    printf("    %s", lv_tkn_errContext.line);
    size_t len = lv_tkn_errContext.endIdx - lv_tkn_errContext.startIdx;
    char arrows[len + 1];
    memset(arrows, '^', len);
    arrows[len] = '\0';
    printf("    %*s\n", (int)lv_tkn_errContext.endIdx, arrows);
}

/** Helper struct to organize function declaration data. */
typedef struct HelperDeclObj {
    Operator* func;
    Token* exprStart;
    Token* body;
} HelperDeclObj;

static bool getFuncSig(FILE* file, Operator* scope, DynBuffer* decls);

bool lv_readFile(char* name) {

    if(!addFile(name))
        return true; //nothing to do..
    //open file
    //TODO clean up a bit (i.e. less strlen)
    static char ext[] = ".lv";  //lv_filepath/name.lv
    char* file = lv_alloc(strlen(lv_filepath) + 1 + strlen(name) + sizeof(ext));
    strcpy(file, lv_filepath);
    strcat(file, "/");  // '/' works on all major OS (Windows, Mac, Linux)
    strcat(file, name);
    strcat(file, ext);
    //try the current directory first, then the Lavender filepath
    FILE* importFile = fopen(file + strlen(lv_filepath) + 1, "r");
    if(!importFile) {
        importFile = fopen(file, "r");
        if(!importFile) {
            lv_free(file);
            return false;
        }
    }
    //end open file
    //parse file
    DynBuffer decls;    //of HelperDeclObj
    lv_buf_init(&decls, sizeof(HelperDeclObj));
    bool res = true;
    scope.name = name;
    lv_tkn_resetLine();
    //parse all function declarations (not the bodies)
    //and gather runtime commands
    while(!feof(importFile) && res) {
        res = getFuncSig(importFile, &scope, &decls);
    }
    //successful parse of all declarations
    if(res) {
        //parse function definitions
        for(size_t i = 0; i < decls.len; i++) {
            HelperDeclObj* obj = lv_buf_get(&decls, i);
            if(!obj->func) {
                //runtime command
                bool successful = lv_cmd_run(obj->body);
                if(!successful || lv_debug)
                    puts(lv_cmd_message);
                if(!successful) {
                    res = false;
                    break; //stop execution because of command error
                }
            } else {
                //function definition
                Token* err = lv_tb_defineFunctionBody(obj->body, obj->func);
                if(LV_EXPR_ERROR) {
                    res = false;
                    printError(err, "Error parsing function body");
                    LV_EXPR_ERROR = 0;
                    break;
                }
            }
        }
    }
    for(size_t i = 0; i < decls.len; i++) {
        HelperDeclObj* obj = lv_buf_get(&decls, i);
        lv_tkn_free(obj->exprStart);
    }
    lv_free(decls.data);
    lv_tkn_releaseFile(importFile);
    fclose(importFile);
    lv_free(file);
    return res;
}

/** Parse a function definition OR a runtime command. */
static bool getFuncSig(FILE* file, Operator* scope, DynBuffer* decls) {

    Token* head = lv_tkn_split(file);
    if(LV_TKN_ERROR) {
        printTokenError("Error parsing input");
        LV_TKN_ERROR = 0;
        return false;
    }
    if(!head) {
        //empty line
        return true;
    }
    if(head->start[0] == '@') {
        //runtime command
        //push next and free '@' token
        HelperDeclObj toPush = { NULL, head, head->next };
        lv_buf_push(decls, &toPush);
        return true;
    }
    if(!isFuncDef(head)) {
        //must be function definitions
        puts("Error parsing input: Not a function definition");
        lv_tkn_free(head);
        return false;
    }
    //declare function
    Token* body = NULL;
    Operator* op = lv_expr_declareFunction(head, scope, &body);
    if(LV_EXPR_ERROR) {
        assert(body);
        printError(body, "Error parsing function signatures");
        LV_EXPR_ERROR = 0;
        lv_tkn_free(head);
        return false;
    }
    //push declaration and body pointer
    HelperDeclObj toPush = { op, head, body };
    lv_buf_push(decls, &toPush);
    return true;
}

static void readInput(FILE* in, bool repl) {

    if(repl) {
        printf("> ");
        lv_tkn_resetLine();
    }
    Token* toks = lv_tkn_split(in);
    if(LV_TKN_ERROR) {
        printTokenError("Error parsing input");
        LV_TKN_ERROR = 0;
        return;
    }
    if(toks) {
        //it's not going into the list, auto alloc is fine
        if(toks->start[0] == '@') {
            Token* cmd = toks->next;
            lv_free(toks); //free '@' token because we may not return
            lv_cmd_run(cmd);
            puts(lv_cmd_message);
            toks = cmd; //so it's freed later
        } else if(isFuncDef(toks)) {
            //define function
            Operator* op;
            scope.name = "repl";    //namespace
            Token* end = lv_tb_defineFunction(toks, &scope, &op);
            if(LV_EXPR_ERROR) {
                printError(end, "Error parsing function");
                LV_EXPR_ERROR = 0;
            } else {
                puts(op->name);
            }
        } else {
            //parse expression
            scope.name = "repl:";      //expr requires an enclosing function
            size_t startIdx, endIdx;
            Token* end = lv_tb_parseExpr(toks, &scope, &startIdx, &endIdx);
            if(LV_EXPR_ERROR) {
                printError(end, "Error parsing expression");
                LV_EXPR_ERROR = 0;
            } else {
                pc = startIdx;
                while(pc != endIdx) {
                    runCycle();
                }
                assert(stack.len == 1);
                TextBufferObj obj;
                lv_buf_pop(&stack, &obj);
                LvString* str = lv_tb_getString(&obj);
                puts(str->value);
                if(str->refCount == 0) {
                    lv_free(str);
                }
                lv_expr_cleanup(&obj, 1);
                lv_tb_clearExpr();
            }
        }
    }
    lv_tkn_releaseFile(in);
    lv_tkn_free(toks);
}

static void makeVect(int length) {

    TextBufferObj vect;
    vect.type = OPT_VECT;
    vect.vect = lv_alloc(sizeof(LvVect) + length * sizeof(TextBufferObj));
    vect.vect->refCount = 0;
    vect.vect->len = length;
    for(size_t i = vect.vect->len; i > 0; i--) {
        //preserve refCounts because we are transferring to vect
        lv_buf_pop(&stack, &vect.vect->data[i - 1]);
    }
    push(&vect);
}

static int mapKeyCmp(const void* p1, const void* p2) {

    LvMapNode* a = (LvMapNode*)p1;
    LvMapNode* b = (LvMapNode*)p2;
    if(a->hash < b->hash) {
        return -1;
    } else if(a->hash > b->hash) {
        return 1;
    } else {
        //compare by sys:__lt__ for equal hashes
        //ensures the same ordering of the same elements
        if(lv_blt_lt(&a->key, &b->key)) {
            return -1;
        } else if(lv_blt_lt(&b->key, &a->key)) {
            return 1;
        }
        return 0;
    }
}

static void makeMap(int size) {

    TextBufferObj map;
    map.type = OPT_MAP;
    map.map = lv_alloc(sizeof(LvMap) + size * sizeof(LvMapNode));
    map.map->refCount = 0;
    for(int i = size; i > 0; i--) {
        LvMapNode* n = &map.map->data[i - 1];
        TextBufferObj key;
        lv_buf_pop(&stack, &n->value);
        lv_buf_pop(&stack, &n->key);
        //keys must be eagerly evaluated, unfortunately
        if(lv_evalByName(&n->key, &key)) {
            lv_expr_cleanup(&n->key, 1);
            if(key.type & LV_DYNAMIC) {
                ++*key.refCount;
            }
            n->key = key;
        }
        n->hash = lv_blt_hash(&n->key);
    }
    //now we sort keys, yay!
    qsort(map.map->data, size, sizeof(LvMapNode), mapKeyCmp);
    size_t len = size;
    //remove duplicates
    if(size > 1) {
        LvMapNode* last = &map.map->data[0];
        LvMapNode* check = &map.map->data[1];
        LvMapNode* end = map.map->data + size;
        while(check != end) {
            if(check->hash == last->hash && lv_blt_equal(&check->key, &last->key)) {
                lv_expr_cleanup(&last->key, 1);
                lv_expr_cleanup(&last->value, 1);
            } else {
                last++;
            }
            *last = *check++;
        }
        len = last - map.map->data + 1;
        if(len < size) {
            map.map = lv_realloc(map.map, sizeof(LvMap) + len * sizeof(LvMapNode));
        }
    }
    map.map->len = len;
    push(&map);
}

/**
 * Prepares the stack for calling the given function with the
 * given number of arguments. Returns whether the setup is successful
 * and sets op to the underlying operator. If the function is a string
 * or a vector, and one argument is passed, the underlying operator is
 * set to the built in __at__ function.
 */
static bool setUpFuncCall(TextBufferObj* _func, size_t numArgs, Operator** underlying) {

    assert(_func);
    assert(underlying);
    bool success = false;
    Operator* op = NULL;
    TextBufferObj realFunc = { .type = OPT_UNDEFINED };
    TextBufferObj* func = _func;
    if(numArgs != 0 && lv_evalByName(_func, &realFunc)) {
        if(realFunc.type & LV_DYNAMIC) {
            ++*realFunc.refCount;
        }
        func = &realFunc;
    }
    switch(func->type) {
        case OPT_FUNCTION_VAL: {
            op = func->func;
            //collect varargs into vect
            if(op->varargs) {
                int vectLen = numArgs - (op->arity - 1);
                if(vectLen >= 0) {
                    makeVect(vectLen);
                    success = true;
                }
            } else if(numArgs == op->arity) {
                success = true;
            }
            break;
        }
        case OPT_CAPTURE: {
            op = func->capfunc;
            int nonCapArity = op->arity - op->captureCount;
            //collect varargs into vect
            if(op->varargs) {
                int vectLen = numArgs - (nonCapArity - 1);
                if(vectLen >= 0) {
                    makeVect(vectLen);
                    nonCapArity = numArgs; //force execution
                }
            }
            if(numArgs == nonCapArity) {
                //push captured params onto stack
                for(int i = 0; i < op->captureCount; i++) {
                    push(&func->capture->value[i]);
                }
                success = true;
            }
            break;
        }
        case OPT_STRING:
        case OPT_VECT:
        case OPT_MAP:
            if(numArgs == 1) {
                op = &atFunc;
                push(func);
                success = true;
            }
            break;
        default:
            break;
    }
    //can't call, pop args and return false
    if(!success)
        popAll(numArgs);
    *underlying = op;
    if(realFunc.type != OPT_UNDEFINED) {
        //cleanup by-name expression result
        lv_expr_cleanup(&realFunc, 1);
    }
    return success;
}

/**
 * Evaluates the given object iff it is a zero-arity function and
 * returns the result through ret. Returns true if obj was a zero
 * arity function and false if it was not.
 */
bool lv_evalByName(TextBufferObj* obj, TextBufferObj* ret) {

    if((obj->type == OPT_CAPTURE && obj->capfunc->arity == obj->capfunc->captureCount)
    || (obj->type == OPT_FUNCTION_VAL && obj->func->arity == 0)) {
        lv_callFunction(obj, 0, NULL, ret);
        return true;
    }
    return false;
}

/**
 * Calls the given function by saving the current stack frame
 * and jumping to the first instruction of the given function.
 * Returns the value of the current (old) frame.
 */
static size_t jumpAndLink(Operator* func) {

    assert(func);
    size_t frame = fp;
    switch(func->type) {
        case FUN_FWD_DECL: {
            //this should never happen
            assert(false);
            break;
        }
        case FUN_BUILTIN: {
            //call built in function, then pop args and push result
            //we never actually pushed a new frame, so return the old
            //(current) value.
            size_t tmpFp = stack.len - func->arity;
            TextBufferObj res = func->builtin(lv_buf_get(&stack, tmpFp));
            //keep a reference to res while we pop
            if(res.type & LV_DYNAMIC)
                ++*res.refCount;
            popAll(func->arity);
            TextBufferObj tmp;
            if(lv_evalByName(&res, &tmp)) {
                lv_expr_cleanup(&res, 1);
                res = tmp;
                if(res.type & LV_DYNAMIC)
                    ++*res.refCount;
            }
            if(stack.len > 0) {
                TextBufferObj* top = lv_buf_get(&stack, stack.len - 1);
                if(top->type == OPT_FUNC_CALL2) {
                    *top = res;
                    break;
                }
            } //else
            if(res.type & LV_DYNAMIC)
                --*res.refCount;
            push(&res);
            break;
        }
        case FUN_FUNCTION: {
            //calling convention
            //  0. push <undefined> into local slots
            //  1. push fp
            //  2. set fp = stack.len - func.arity - func.locals - 1 (first argument)
            //  3. push pc (return value)
            //  4. set pc = first inst of function
            //The stack looks like this:
            //  ... arg0 arg1 .. argN-1 fp pc ...
            //       ^^
            //       fp
            TextBufferObj obj;
            obj.type = OPT_UNDEFINED;
            for(int i = 0; i < func->locals; i++) {
                push(&obj);
            }
            obj.type = OPT_ADDR;
            obj.addr = fp;
            push(&obj);
            fp = stack.len - func->arity - func->locals - 1;
            obj.addr = pc;
            push(&obj);
            pc = func->textOffset;
            break;
        }
    }
    return frame;
}

static void runCycle(void) {

    TextBufferObj* value = &TEXT_BUFFER[pc++];
    TextBufferObj func; //used in some operations
    switch(value->type) {
        case OPT_FUNC_CAP: {
            //capture outer arguments into function object
            //see expression.c:shuntingYard for capture stack layout
            func = removeTop();
            assert(func.func->type == FUN_FUNCTION); //only Lv functions can capture
            TextBufferObj obj;
            obj.type = OPT_CAPTURE;
            obj.capfunc = func.func;
            obj.capture = lv_alloc(sizeof(CaptureObj)
                + func.func->captureCount * sizeof(TextBufferObj));
            obj.capture->refCount = 0;
            for(int i = func.func->captureCount - 1; i >= 0; i--) {
                //preserve refCounts because we are transferring to capture
                lv_buf_pop(&stack, &obj.capture->value[i]);
            }
            push(&obj);
            break;
        }
        case OPT_MAKE_VECT: {
            makeVect(value->callArity);
            break;
        }
        case OPT_MAKE_MAP:
            makeMap(value->callArity);
            break;
        case OPT_FUNCTION_VAL:
        case OPT_UNDEFINED:
        case OPT_NUMBER:
        case OPT_INTEGER:
        case OPT_STRING:
        case OPT_CAPTURE:
        case OPT_VECT:
        case OPT_MAP:
        case OPT_SYMB:
            //push it on the stack
            push(value);
            break;
        case OPT_PARAM:
            //does not evaluate zero-arity functions
            push(lv_buf_get(&stack, fp + value->param));
            break;
        case OPT_PUT_PARAM: {
            //pop top and place in i'th param
            TextBufferObj* param = lv_buf_get(&stack, fp + value->param);
            lv_buf_pop(&stack, &func);
            *param = func;
            break;
        }
        case OPT_BEQZ: {
            TextBufferObj obj = removeTop();
            if(!lv_blt_toBool(&obj))
                pc += value->branchAddr - 1;
            break;
        }
        case OPT_FUNC_CALL2: {
            int arity = value->callArity;
            //in contrast to func call 1, the function is at the bottom
            {
                //remove the function logically from the stack
                TextBufferObj* pos = lv_buf_get(&stack, stack.len - arity);
                func = *pos;
                //signal for return instruction to remove bottom func
                pos->type = OPT_FUNC_CALL2;
            }
            Operator* op;
            bool setup = setUpFuncCall(&func, arity - 1, &op);
            lv_expr_cleanup(&func, 1);
            if(!setup) {
                assert(stack.len > 0);
                TextBufferObj* top = lv_buf_get(&stack, stack.len - 1);
                top->type = OPT_UNDEFINED;
            } else {
                jumpAndLink(op);
            }
            break;
        }
        case OPT_FUNCTION: {
            jumpAndLink(value->func);
            break;
        }
        case OPT_RETURN: {
            //bypass removeTop for the return value
            //so we keep its string refCount intact
            //this keeps popAll from freeing the return value
            TextBufferObj retVal;
            lv_buf_pop(&stack, &retVal);
            //reset pc and fp
            pc = removeTop().addr;
            size_t tmpFp = removeTop().addr;
            //pop args
            popAll(stack.len - fp);
            fp = tmpFp;
            TextBufferObj tmp;
            if(lv_evalByName(&retVal, &tmp)) {
                lv_expr_cleanup(&retVal, 1);
                //evalByName does not refCount the return value
                if(tmp.type & LV_DYNAMIC) {
                    ++*tmp.refCount;
                }
                retVal = tmp;
            }
            if(stack.len > 0) {
                TextBufferObj* top = lv_buf_get(&stack, stack.len - 1);
                if(top->type == OPT_FUNC_CALL2) {
                    *top = retVal;
                    break;
                }
            } //else
            lv_buf_push(&stack, &retVal);
            break;
        }
        case OPT_LITERAL:
        case OPT_ADDR:
        case OPT_EMPTY_ARGS:
            assert(false);
            return;
    }
}

/**
 * Calls the function given with the parameters given and returns
 * the result. This function handles captures, vects, strings, and functions.
 */
void lv_callFunction(TextBufferObj* func, size_t numArgs, TextBufferObj* args, TextBufferObj* ret) {

    //push args onto stack
    Operator* op;
    for(size_t i = 0; i < numArgs; i++) {
        push(&args[i]);
    }
    if(!setUpFuncCall(func, numArgs, &op)) {
        ret->type = OPT_UNDEFINED;
    } else {
        size_t frame = jumpAndLink(op);
        //we stop executing when the frame pushed by
        //jumpAndLink is popped.
        while(fp != frame) {
            runCycle();
        }
        *ret = removeTop();
    }
}
