/**
 * IFJ26 Compiler - Lexical analyser implementation
 * Author: Jakub Holub (xholubj00)
 * Date: 8.10.2026
 **/

#include "lexer.h"
#include "dstring.h"
#include "error.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Create and initialize token
 **/
Token *create_token(token_type type, dyn_string_t *str) {
  Token *token = malloc(sizeof(Token));

  if (token == NULL) {
    throw_error(INTERNAL_ERROR, "Memory allocation failed.");
  }

  token->type = type;

  if (str != NULL) {
    token->attribute.str_val = str->str;
  } else {
    token->attribute.str_val = NULL;
  }

  return token;
}

/**
 * Resolves whether the given string is a keyword or a general identifier
 * Returns the corresponding token_type
 **/
token_type resolve_identifier(dyn_string_t *str) {
  char *clear_str = str->str;

  if (strcmp(clear_str, "if") == 0)
    return TOKEN_KW_IF;
  if (strcmp(clear_str, "else") == 0)
    return TOKEN_KW_ELSE;
  if (strcmp(clear_str, "do") == 0)
    return TOKEN_KW_DO;
  if (strcmp(clear_str, "def") == 0)
    return TOKEN_KW_DEF;
  if (strcmp(clear_str, "Double") == 0)
    return TOKEN_KW_DOUBLE;
  if (strcmp(clear_str, "import") == 0)
    return TOKEN_KW_IMPORT;
  if (strcmp(clear_str, "Int") == 0)
    return TOKEN_KW_INT;
  if (strcmp(clear_str, "return") == 0)
    return TOKEN_KW_RETURN;
  if (strcmp(clear_str, "String") == 0)
    return TOKEN_KW_STRING;
  if (strcmp(clear_str, "then") == 0)
    return TOKEN_KW_THEN;
  if (strcmp(clear_str, "Unit") == 0)
    return TOKEN_KW_UNIT;
  if (strcmp(clear_str, "val") == 0)
    return TOKEN_KW_VAL;
  if (strcmp(clear_str, "var") == 0)
    return TOKEN_KW_VAR;
  if (strcmp(clear_str, "while") == 0)
    return TOKEN_KW_WHILE;

  // Optional key words for extensions
  if (strcmp(clear_str, "Boolean") == 0)
    return TOKEN_KW_BOOLEAN;
  if (strcmp(clear_str, "case") == 0)
    return TOKEN_KW_CASE;
  if (strcmp(clear_str, "class") == 0)
    return TOKEN_KW_CLASS;
  if (strcmp(clear_str, "extension") == 0)
    return TOKEN_KW_EXTENSION;
  if (strcmp(clear_str, "for") == 0)
    return TOKEN_KW_FOR;
  if (strcmp(clear_str, "List") == 0)
    return TOKEN_KW_LIST;
  if (strcmp(clear_str, "match") == 0)
    return TOKEN_KW_MATCH;
  if (strcmp(clear_str, "type") == 0)
    return TOKEN_KW_TYPE;

  // No match - clasify as name, identifier
  return TOKEN_IDENTIFIER;
}
