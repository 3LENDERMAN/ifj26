/**
 * IFJ26 Compiler - Token stream implementation
 * Author: Vít Kutěj (xkutejv00)
 * Date: 11.10.2026
 **/

#include "token_stream.h"
#include <stdlib.h>
#include <string.h>
#include "error.h"

struct TokenStream
{
    FILE *file;
    Token *tokens;
    size_t tokens_count;
    size_t token_index; // index of the first unprocessed token in the array
};

/**
 * Initialize and allocate memory for TokenStream structure
 **/
static TokenStream *allocate_stream(void)
{
    TokenStream *stream = calloc(1, sizeof *stream);
    if (stream == NULL)
    {
        throw_error(INTERNAL_ERROR, "Memory allocation failed.");
    }
    return stream;
}

/**
 * Create token stream from file
 **/
TokenStream *ts_create(FILE *file)
{
    TokenStream *stream = allocate_stream();
    stream->file = file;
    return stream;
}

/**
 * Create token stream from array
 **/
TokenStream *ts_create_from_array(const Token *tokens, size_t count)
{
    TokenStream *stream = allocate_stream();
    size_t size = count ? count : 1;

    stream->tokens = malloc(size * sizeof(Token));
    if (stream->tokens == NULL)
    {
        free(stream);
        throw_error(INTERNAL_ERROR, "Memory allocation failed.");
    }

    if (count > 0 && tokens != NULL)
    {
        memcpy(stream->tokens, tokens, count * sizeof(Token));
        stream->tokens_count = count;
    }
    return stream;
}

/**
 * Fetch next token from file or array.
 **/
static const Token *fetch_next_token(TokenStream *stream)
{
    return NULL; // Placeholder
}

/**
 * Look ahead at token with offset
 **/
const Token *ts_peek(TokenStream *stream, int offset)
{
    return NULL; // Placeholder
}

/**
 * Move to next token in the stream
 **/
const Token *ts_next(TokenStream *stream)
{
    return NULL; // Placeholder
}

/**
 * Free memory of the stream structure and its token array
 **/
void ts_destroy(TokenStream *stream)
{
    if (stream == NULL)
    {
        return;
    }

    free(stream->tokens);
    free(stream);
}