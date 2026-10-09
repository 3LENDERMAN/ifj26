/**
 * IFJ26 Compiler - Lexical analyser implementation
 * Author: Jakub Holub (xholubj00)
 * Date: 8.10.2026
 **/

#include "lexer.h"
#include "dstring.h"
#include "error.h"
#include <ctype.h>
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
  // Add token structure to garbage collector
  garbage_register(token);

  token->type = type;

  if (str != NULL) {
    token->attribute.str_val = str->str;
    garbage_register(str->str);

  } else {
    token->attribute.str_val = NULL;
  }

  return token;
}

/**
 * Resolves whether the given string is a keyword or a general identifier
 * Returns the corresponding token_type
 **/
token_type resolve_identifier(const dyn_string_t *str) {
  const char *clear_str = str->str;

  if (strcmp(clear_str, "_") == 0) // Later allow for EXTENSION
    throw_error(LEXER_ERROR, "Invalid identifier '_'");
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

/**
 *
 **/
Token *get_token(FILE *file) {
  // Define all states in FSM
  typedef enum {
    STATE_START,
    STATE_LESS,    // <
    STATE_GREATER, // >
    STATE_ASSIGN,  // =
    STATE_NOT,     // !

    // States for comments, division
    STATE_SLASH,               // /
    STATE_LINE_COMMENT,        //  inside //
    STATE_BLOCK_COMMENT,       // inside /*
    STATE_BLOCK_COMMENT_STAR,  // inside of /* and * loaded
    STATE_BLOCK_COMMENT_SLASH, // inside /* and / loaded
    STATE_EXPECT_EOL,

    // Identifiers
    STATE_IDENTIFIER,

    // Numbers states
    STATE_DOT_OR_DOUBLE, // . loaded (can be just dot or double)

    STATE_NUM_EXP_SIGN, // Sign of exponent part
    STATE_NUM_WHOLE,    // Whole number (or whole part)
    STATE_NUM_DECIMAL,  // Decimal part (behind '.')
    STATE_NUM_EXP,      // Digits of exponent
    STATE_NUM_DOT,      // Dot between whole and decimal part
    STATE_NUM_E,        // loaded e or E -> follows +, - or digit
    STATE_NUM_D,        // for optional suffix D or d

    // Checking underscores inside number literals
    STATE_NUM_WHOLE_USC, // inside whole part
    STATE_NUM_DEC_USC,   // inside decimal part
    STATE_NUM_EXP_USC,   // exponent
  } lexer_state;

  // Init state
  lexer_state state = STATE_START;

  bool block_has_newline = false;
  bool left_side_zero = false;
  // Number of nested block comments
  int comment_level = 0;

  // Dynamic string for storing and checking identifiers and literals
  dyn_string_t attribute_str;

  // Main FSM loop cycle
  while (1) {
    int c = fgetc(file);
    switch (state) {
    case STATE_START:
      if (c == ' ' || c == '\t' || c == '\r') {
        continue; // Stay in START, ignore whitespaces
      }

      // Check for one character tokens
      if (c == '\n')
        return create_token(TOKEN_NEWLINE, NULL);
      if (c == '(')
        return create_token(TOKEN_LEFT_PAREN, NULL);
      if (c == ')')
        return create_token(TOKEN_RIGHT_PAREN, NULL);
      if (c == '{')
        return create_token(TOKEN_BRACE_LEFT, NULL);
      if (c == '}')
        return create_token(TOKEN_BRACE_RIGHT, NULL);
      if (c == '*')
        return create_token(TOKEN_ASTERISK, NULL);
      if (c == '+')
        return create_token(TOKEN_PLUS, NULL);
      if (c == '-')
        return create_token(TOKEN_MINUS, NULL);
      if (c == ',')
        return create_token(TOKEN_COMMA, NULL);
      if (c == EOF)
        return create_token(TOKEN_EOF, NULL);

      // Multicharacter tokens
      if (c == '<') {
        state = STATE_LESS;
        continue;
      }
      if (c == '>') {
        state = STATE_GREATER;
        continue;
      }
      if (c == '=') {
        state = STATE_ASSIGN;
        continue;
      }
      if (c == '!') {
        state = STATE_NOT;
        continue;
      }

      // Comments
      if (c == '/') {
        state = STATE_SLASH;
        continue;
      }

      // Identifiers and keywords
      if (isalpha(c) || c == '_') {
        state = STATE_IDENTIFIER;
        if (!dstring_init(&attribute_str)) {
          throw_error(INTERNAL_ERROR,
                      "Failed to allocate memory for dynamic string");
        }
        // Start storing text into dynamic string
        dstring_append_char(&attribute_str, c);
        continue;
      }

      // Numbers
      if (c == '.') {
        state = STATE_DOT_OR_DOUBLE;
        continue;
      }
      if (isdigit(c)) {
        state = STATE_NUM_WHOLE;
        if (!dstring_init(&attribute_str)) {
          throw_error(INTERNAL_ERROR,
                      "Failed to allocate memory for dynamic string");
        }
        dstring_append_char(&attribute_str, c);
        if (c == '0') {
          left_side_zero = true;
        }
        continue;
      }

      throw_error(LEXER_ERROR, "Invalid character on input.");
      break;
    case STATE_LESS:
      if (c == '=') { // <=
        return create_token(TOKEN_LESS_EQ, NULL);
      } else { // <
        ungetc(c, file);
        return create_token(TOKEN_LESS, NULL);
      }
      break;
    case STATE_GREATER:
      if (c == '=') { // >=
        return create_token(TOKEN_GREATER_EQ, NULL);
      } else { // >
        ungetc(c, file);
        return create_token(TOKEN_GREATER, NULL);
      }
      break;
    case STATE_ASSIGN:
      if (c == '=') { // ==
        return create_token(TOKEN_EQ, NULL);
      } else { // =
        ungetc(c, file);
        return create_token(TOKEN_ASSIGN, NULL);
      }
      break;
    case STATE_NOT:
      if (c == '=') { // !=
        return create_token(TOKEN_NOT_EQ, NULL);
      } else { // ! (unary negation)
        throw_error(
            LEXER_ERROR,
            "Expected '=' after '!'"); // REVISIT LATER FOR BOOLTHEN EXTENSION
      }
      break;

    case STATE_SLASH:
      if (c == '/') { // Second '/' read -> one-line comment
        state = STATE_LINE_COMMENT;
        comment_level = 1;
      } else if (c == '*') { // '/*' read -> block comment starts
        state = STATE_BLOCK_COMMENT;
        comment_level = 1;
        block_has_newline = false;
      } else { // Bsic division operation (next character is not part of current
               // token)
        ungetc(c, file);
        return create_token(TOKEN_DIV, NULL);
      }
      break;

    case STATE_LINE_COMMENT:
      if (c == '\n') {
        return create_token(TOKEN_NEWLINE, NULL);
      } else if (c == EOF) {
        return create_token(TOKEN_EOF, NULL);
      }
      // Here ignore everything else inside one-line comment
      break;
    case STATE_BLOCK_COMMENT:
      if (c == '\n') {
        block_has_newline = true;
      } else if (c == '*') {
        state = STATE_BLOCK_COMMENT_STAR; // * inside /*.. *?
      } else if (c == '/') {
        state = STATE_BLOCK_COMMENT_SLASH; // / inside /*.. /?
      } else if (c == EOF) {
        throw_error(LEXER_ERROR, "Block comment missing end sequence (EOF)");
      }
      break;
    case STATE_BLOCK_COMMENT_STAR:
      if (c == '/') { // */ loaded - decrement level count
        comment_level--;
        if (comment_level == 0) {
          if (block_has_newline) {
            state = STATE_EXPECT_EOL;
          } else {
            state = STATE_START;
          }
        } else {
          state = STATE_BLOCK_COMMENT;
        }
      } else if (c == EOF) {
        throw_error(LEXER_ERROR, "Block comment missing end sequence (EOF)");
      } else if (c == '*') {
        state = STATE_BLOCK_COMMENT_STAR;
      } else {
        state = STATE_BLOCK_COMMENT;
      }
      break;
    case STATE_BLOCK_COMMENT_SLASH:
      if (c == '\n') {
        block_has_newline = true;
      } else if (c == '*') { // loaded /* -> increment level count
        comment_level++;
        state = STATE_BLOCK_COMMENT;
      } else if (c == '/') { // second / (//) inside /* ..//
        state = STATE_BLOCK_COMMENT_SLASH;
      } else if (c == EOF) {
        throw_error(LEXER_ERROR, "Block comment missing end sequence (EOF)");
      } else {
        state = STATE_BLOCK_COMMENT;
      }
      break;
    case STATE_EXPECT_EOL:
      if (c == ' ' || c == '\t' ||
          c == '\r') { // whitespace chars before \n are allowed
        continue;
      } else if (c == '\n') { // end of line
        return create_token(TOKEN_NEWLINE, NULL);
      } else if (c == EOF) {
        ungetc(c, file);
        return create_token(TOKEN_NEWLINE, NULL);
      } else { // Another token after block comment is not allowed
        throw_error(LEXER_ERROR, "Tokens after newline block comment.");
      }
      break;

    case STATE_IDENTIFIER:
      // Check if all character are valid (alpha-numerical + _)
      if (isalnum(c) || c == '_') {
        dstring_append_char(&attribute_str, c);
      } else {
        ungetc(c, file);

        // Here the identifier is complete - resolve it into token
        token_type token_resolved = resolve_identifier(&attribute_str);

        if (token_resolved != (TOKEN_IDENTIFIER)) { // is keyword
          dstring_free(&attribute_str);
          return create_token(token_resolved, NULL);
        } else { // is identifier - save the string
          return create_token(TOKEN_IDENTIFIER, &attribute_str);
        }
      }
      break;
    case STATE_DOT_OR_DOUBLE:
      if (isdigit(c)) {
        state = STATE_NUM_DECIMAL;    // decimal point without zero
        dstring_init(&attribute_str); // for better parsing, add 0 and . for
                                      // standard syntax
        dstring_append_char(&attribute_str, '0');
        dstring_append_char(&attribute_str, '.');
        dstring_append_char(&attribute_str, c);
      } else { // just a dot
        ungetc(c, file);
        return create_token(TOKEN_DOT, NULL);
      }
      break;
    case STATE_NUM_WHOLE: // whole part of decimal number, or whole number
      if (isdigit(c)) {
        if (left_side_zero) { // numbers such as "0512" or "000" are not allowed
          throw_error(LEXER_ERROR, "Whole number starting with zero");
        }
        dstring_append_char(&attribute_str, c);
      } else if (c == '.') {
        dstring_append_char(&attribute_str, c);
        state = STATE_NUM_DOT;
      } else if (c == 'e' || c == 'E') {
        dstring_append_char(&attribute_str, c);
        state = STATE_NUM_E;
      } else if (c == 'd' || c == 'D') {
        state = STATE_NUM_D;
      } else if (c == '_') {
        if (left_side_zero) {
          throw_error(
              LEXER_ERROR,
              "Number starting with zero cannot be followed by underscore");
        }
        state = STATE_NUM_WHOLE_USC;
      } else { // next token
        ungetc(c, file);
        return create_token(TOKEN_NUM_LITERAL, &attribute_str);
      }
      break;
    case STATE_NUM_DOT: // loaded . -> follows only digits
      if (isdigit(c)) {
        state = STATE_NUM_DECIMAL;
        dstring_append_char(&attribute_str, c);
      } else {
        throw_error(LEXER_ERROR, "Wrong character after decimal point");
      }
      break;
    case STATE_NUM_DECIMAL: // second char in decimal part after dot
      if (isdigit(c)) {
        dstring_append_char(&attribute_str, c);
      } else if (c == 'e' || c == 'E') {
        state = STATE_NUM_E;
        dstring_append_char(&attribute_str, c);
      } else if (c == 'd' || c == 'D') {
        state = STATE_NUM_D;
      } else if (c == '_') {
        state = STATE_NUM_DEC_USC;
      } else {
        ungetc(c, file);
        return create_token(TOKEN_NUM_LITERAL, &attribute_str);
      }
      break;
    case STATE_NUM_E:
      if (c == '+' || c == '-') {
        state = STATE_NUM_EXP_SIGN;
        dstring_append_char(&attribute_str, c);
      } else if (isdigit(c)) {
        state = STATE_NUM_EXP;
        dstring_append_char(&attribute_str, c);
      } else {
        throw_error(LEXER_ERROR,
                    "Expecting digit or sign character after exponent");
      }
      break;

    case STATE_NUM_EXP_SIGN:
      if (isdigit(c)) {
        state = STATE_NUM_EXP;
        dstring_append_char(&attribute_str, c);
      } else {
        throw_error(LEXER_ERROR, "Expecting digit after sign of exponent");
      }
      break;

    case STATE_NUM_EXP:
      if (isdigit(c)) {
        dstring_append_char(&attribute_str, c);
      } else if (c == 'd' || c == 'D') {
        state = STATE_NUM_D;
      } else if (c == '_') {
        state = STATE_NUM_EXP_USC;
      } else {
        ungetc(c, file);
        return create_token(TOKEN_NUM_LITERAL, &attribute_str);
      }
      break;

    case STATE_NUM_D:
      // Suffix - always end of number
      ungetc(c, file);
      return create_token(TOKEN_NUM_LITERAL, &attribute_str);
      break;

    case STATE_NUM_WHOLE_USC:
      if (isdigit(c)) {
        state = STATE_NUM_WHOLE; // continuing in whole part
        dstring_append_char(&attribute_str, c);
      } else {
        throw_error(LEXER_ERROR,
                    "Wrong syntax - underscores must be between digits");
      }
      break;
    case STATE_NUM_DEC_USC:
      if (isdigit(c)) {
        state = STATE_NUM_DECIMAL;
        dstring_append_char(&attribute_str, c);
      } else {
        throw_error(LEXER_ERROR,
                    "Wrong syntax - underscores must be between digits");
      }
      break;
    case STATE_NUM_EXP_USC:
      if (isdigit(c)) {
        state = STATE_NUM_EXP;
        dstring_append_char(&attribute_str, c);
      } else {
        throw_error(LEXER_ERROR,
                    "Wrong syntax - underscores must be between digits");
      }
      break;
    }
  }
}
