/**
 * IFJ26 Compiler - Header file for parser
 * Author: Vít Kutěj (xkutejv00)
 * Date: 11.10.2026
 **/

#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lexer.h"
#include "ast.h"
#include "token_stream.h"
#include "error.h"

program *parse(TokenStream *ts);

expr *parse_expression(TokenStream *ts);

#endif