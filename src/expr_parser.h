/**
 * IFJ Compiler - Expression parser header file
 * Author: David Pokorák (xpokord00)
 * Date: 15.10.2026
 **/

#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

#include "ast.h"
#include "token_stream.h"

expr *parse_expression(TokenStream *ts);

#endif