/**
 * IFJ26 Compiler - Header file for lexical analyzer
 * Author: Jakub Holub (xholubj00)
 * Date: 8.10.2026
 **/

#ifndef LEXER_H
#define LEXER_H

#include "dstring.h"
#include <stdio.h>

typedef enum {
  // Expressions:
  TOKEN_LEFT_PAREN,  // (
  TOKEN_RIGHT_PAREN, // )
  TOKEN_BRACE_LEFT,  // {
  TOKEN_BRACE_RIGHT, // }
  TOKEN_LESS,        // <
  TOKEN_GREATER,     // >
  TOKEN_LESS_EQ,     // <=
  TOKEN_GREATER_EQ,  // >=
  TOKEN_EQ,          // ==
  TOKEN_NOT_EQ,      // !=
  TOKEN_DOT,         // .
  TOKEN_DIV,         // /
  TOKEN_ASTERISK,    // *
  TOKEN_PLUS,        // +
  TOKEN_MINUS,       // -
  TOKEN_ASSIGN,      // =
  TOKEN_COMMA,       // ,
  TOKEN_NEWLINE,     // \n

  // Keywords:
  TOKEN_KW_IF,
  TOKEN_KW_ELSE,
  TOKEN_KW_WHILE,
  TOKEN_KW_RETURN,
  TOKEN_KW_DEF,
  TOKEN_KW_DO,
  TOKEN_KW_DOUBLE,
  TOKEN_KW_IMPORT,
  TOKEN_KW_INT,
  TOKEN_KW_STRING,
  TOKEN_KW_THEN,
  TOKEN_KW_UNIT,
  TOKEN_KW_VAL,
  TOKEN_KW_VAR,

  // Other keywords for extensions..
  TOKEN_KW_BOOLEAN,
  TOKEN_KW_CASE,
  TOKEN_KW_CLASS,
  TOKEN_KW_EXTENSION,
  TOKEN_KW_FOR,
  TOKEN_KW_LIST,
  TOKEN_KW_MATCH,
  TOKEN_KW_TYPE,

  // Identifier is made of Alphabet letters, numbers, _ (has to be more than 1)
  TOKEN_IDENTIFIER,

  TOKEN_INT_LITERAL,    // Integer
  TOKEN_DOUBLE_LITERAL, // Double

  // Bounded by " (ASCII 34), cannot contain end of line character
  // Allowed escape sequences: \\, \", \n, \r, \t
  TOKEN_STR_LITERAL,

  TOKEN_UNIT_LITERAL, // ()

  TOKEN_EOF, // EOF
} token_type;

typedef struct {
  token_type type;

  union {
    int int_val;       // For TOKEN_INT_LITERAL
    double double_val; // For TOKEN_DOUBLE_LITERAL
    char *str_val;     // FOr TOKEN_STR_LITERAL or TOKEN_IDENTIFIER
  } attribute;
} Token;

#endif
