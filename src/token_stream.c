/**
 * IFJ26 Compiler - Token stream implementation
 * Author: Vít Kutěj (xkutejv00)
 * Date: 11.10.2026
 **/

#include "token_stream.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

#define RING_BUFFER_SIZE 8

struct TokenStream {
  FILE *file;
  Token *tokens;
  size_t tokens_count;
  size_t token_index; // index of the first unprocessed token in the array

  const Token *ring_buffer[RING_BUFFER_SIZE];
  size_t head; // index of the first buffered token
  size_t buffered_count;

  const Token *eof_token;
};

/**
 * Initialize and allocate memory for TokenStream structure
 **/
static TokenStream *allocate_stream(void) {
  TokenStream *stream = calloc(1, sizeof *stream);
  if (stream == NULL) {
    throw_error(INTERNAL_ERROR, "Memory allocation failed.");
  }
  return stream;
}

/**
 * Create token stream from file
 **/
TokenStream *ts_create(FILE *file) {
  TokenStream *stream = allocate_stream();
  stream->file = file;
  return stream;
}

/**
 * Create token stream from array
 **/
TokenStream *ts_create_from_array(const Token *tokens, size_t count) {
  TokenStream *stream = allocate_stream();
  size_t size = count ? count : 1;

  stream->tokens = malloc(size * sizeof(Token));
  if (stream->tokens == NULL) {
    free(stream);
    throw_error(INTERNAL_ERROR, "Memory allocation failed.");
  }

  if (count > 0 && tokens != NULL) {
    memcpy(stream->tokens, tokens, count * sizeof(Token));
    stream->tokens_count = count;
  }
  return stream;
}

/**
 * Fetch next token from file or array.
 **/
static const Token *fetch_next_token(TokenStream *stream) {
  if (stream->file != NULL) {
    return get_token(stream->file);
  }
  if (stream->token_index < stream->tokens_count) {
    return &stream->tokens[stream->token_index++];
  }
  return NULL;
}

/**
 * Look ahead at token with offset
 **/
const Token *ts_peek(TokenStream *stream, int offset) {
  // Check for valid offset, maximum lookahead is RING_BUFFER_SIZE - 1
  if (offset < 0 || offset >= RING_BUFFER_SIZE) {
    throw_error(INTERNAL_ERROR,
                "Lookahead offset is larger than the ring buffer size.");
  }

  while (stream->buffered_count <= (size_t)offset) {
    const Token *token;
    if (stream->eof_token != NULL) {
      token = stream->eof_token;
    } else {
      token = fetch_next_token(stream);
      if (token == NULL || token->type == TOKEN_EOF) {
        if (token == NULL) {
          // Array ended without EOF token, use fallback EOF token
          static const Token fallback_eof = {TOKEN_EOF, {0}};
          token = &fallback_eof;
        }
        stream->eof_token = token;
      }
    }

    stream->ring_buffer[(stream->head + stream->buffered_count) %
                        RING_BUFFER_SIZE] = token;
    stream->buffered_count++;
  }

  return stream
      ->ring_buffer[(stream->head + (size_t)offset) % RING_BUFFER_SIZE];
}

/**
 * Move to next token in the stream
 **/
const Token *ts_next(TokenStream *stream) {
  const Token *token = ts_peek(stream, 0);
  stream->head = (stream->head + 1) % RING_BUFFER_SIZE;
  stream->buffered_count--;
  return token;
}

/**
 * Free memory of the stream structure and its token array
 **/
void ts_destroy(TokenStream *stream) {
  if (stream == NULL) {
    return;
  }

  free(stream->tokens);
  free(stream);
}