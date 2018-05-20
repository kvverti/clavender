#ifndef COMMAND_H
#define COMMAND_H
#include "token.h"
#include <stdbool.h>

/**
 * Runs the command given by the token sequence.
 * The argument is the first token of the command,
 * not including the initial '@' character.
 * Returns whether the command succeeded.
 */
bool lv_cmd_run(Token* first);

/**
 * Message string for the last run command.
 */
char* lv_cmd_message;

#endif
