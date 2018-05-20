#include "command.h"
#include "lavender.h"
#include <string.h>

typedef struct CommandElement {
    char* name;
    bool (*func)(Token*);
} CommandElement;

static bool quit(Token* head);
static bool import(Token* head);
static bool using(Token* head);

static CommandElement COMMANDS[] = {
    { "quit", quit },
    { "import", import },
    { "using", using },
};
#define NUM_COMMANDS (sizeof(COMMANDS) / sizeof(CommandElement))

bool lv_cmd_run(Token* head) {
    
    if(!head || head->type != TTY_IDENT) {
        lv_cmd_message = "Not a command";
        return false;
    }
    for(size_t i = 0; i < NUM_COMMANDS; i++) {
        if(strcmp(head->value, COMMANDS[i].name) == 0) {
            return COMMANDS[i].func(head->next);
        }
    }
    lv_cmd_message = "No command found";
    return false;
}

static bool quit(Token* head) {
    
    lv_tkn_free(head);
    lv_shutdown();
    return true; //never reached
}

static bool import(Token* head) {
    
    lv_cmd_message = "Import not implemented";
    return false;
}

static bool using(Token* head) {
    
    lv_cmd_message = "Using not implemented";
    return false;
}
