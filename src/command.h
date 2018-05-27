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

/**
 * Sets 'scopes' to a dynamically allocated array whose members
 * are strings denoting imported scopes. Sets 'len' to the number
 * of imported scopes. Note that calling lv_cmd_run may invalidate
 * this array. The caller must NOT free the array.
 */
void lv_cmd_getUsingScopes(char*** scopes, size_t* len);

/**
 * Gets the qualified name for the given simple name,
 * or NULL if no such mapping exists.
 */
char* lv_cmd_getQualNameFor(char* simpleName);

void lv_cmd_onStartup();
void lv_cmd_onShutdown();

#endif
