/**
 * IFJ26 Compiler - Header file for parser
 * Author: Vít Kutěj (xkutejv00)
 * Date: 11.10.2026
 **/

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "error.h"
#include "lexer.h"
#include "token_stream.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

program *parse(TokenStream *ts);

expr *parse_expression(TokenStream *ts);
expr *parse_value(TokenStream *stream);

#endif