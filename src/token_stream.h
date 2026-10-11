/**
 * IFJ26 Compiler - Header file for token stream
 * Author: Vít Kutěj (xkutejv00)
 * Date: 11.10.2026
 **/

#ifndef TOKEN_STREAM_H
#define TOKEN_STREAM_H

#include <stddef.h>
#include <stdio.h>
#include "lexer.h"

typedef struct TokenStream TokenStream;

TokenStream *ts_create(FILE *file);
TokenStream *ts_create_from_array(const Token *tokens, size_t count);
const Token *ts_peek(TokenStream *ts, int offset);
const Token *ts_next(TokenStream *ts);
void ts_destroy(TokenStream *ts);

#endif