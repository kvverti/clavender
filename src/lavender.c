#include "lavender.h"
#include "token.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool lv_debug = false;
char* lv_filepath = ".";
char* lv_mainFile = NULL;

static void readInput(FILE* in, bool repl);

void lv_run() {
    
    if(lv_mainFile) {
        //run main NYI
    } else {
        if(lv_debug)
            puts("Running in debug mode");
        puts("Lavender runtime v. 1.0 by Chris Nero\n"
             "Open source at https://github.com/kvverti/clavender\n"
             "Enter function definitions or expressions");
        while(true) {
            lv_repl();
        }
    }
}

void* lv_alloc(size_t size) {
    
    void* res = malloc(size);
    if(!res)
        lv_shutdown();
    return res;
}

void lv_shutdown() {
    
    exit(0);
}

void lv_repl() {
    
    readInput(stdin, true);
}

static void readInput(FILE* in, bool repl) {
    
    if(repl)
        printf("> ");
    Token* toks = lv_tkn_split(in);
    if(LV_TKN_ERROR) {
        printf("Error parsing tokens: %s\n", lv_tkn_getError(LV_TKN_ERROR));
        LV_TKN_ERROR = 0;
        return;
    }
    Token* tmp = toks;
    while(tmp) {
        if(strcmp(tmp->value, "@") == 0) {
            lv_tkn_free(toks);
            lv_shutdown();
        }
        printf("Type %d, value %s\n", tmp->type, tmp->value);
        tmp = tmp->next;
    }
    lv_tkn_free(toks);
}
