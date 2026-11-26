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

// initialize variables for indentation check
static int paren_level = 0;       // current level inside ()
static bool is_line_start = true; // is start of logival row
static int space_count = 0;
static int bracket_level = 0; // nesting inside []

// Stack for storing indentations
static int indent_stack[50] = {0}; // max 50 levels of indentation
static int indent_top = 0;
static int pending_indents = 0;

// queue for delayed tokens (INTERPOLATEDS, VALMEM)
static Token *pending_queue[8];
static int pending_head = 0;
static int pending_tail = 0;

static void enqueue_token(Token *tok) {
  if (pending_tail < 8) {
    pending_queue[pending_tail++] = tok;
  }
}

// context for interpolation strings
typedef struct {
  bool is_multiline;
  bool in_expr;
  int brace_depth;
} interp_ctx_t;

static interp_ctx_t interp_stack[16];
static int interp_top = -1;

static bool in_interp_expr(void) {
  return interp_top >= 0 && interp_stack[interp_top].in_expr;
}

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
token_type resolve_identifier(dyn_string_t *str) {
  const char *clear_str = str->str;

  if (strcmp(clear_str, "_") == 0)
    return TOKEN_UNDERSCORE;
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
  if (strcmp(clear_str, "true") == 0)
    return TOKEN_KW_TRUE;
  if (strcmp(clear_str, "false") == 0)
    return TOKEN_KW_FALSE;
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
 * Helper function for reading interpolated string (ext. INTERPOLATEDS)
 */
static Token *lex_interp_chunk(FILE *file) {
  bool multi = interp_stack[interp_top].is_multiline;
  dyn_string_t text;
  if (!dstring_init(&text)) {
    throw_error(INTERNAL_ERROR, "Failed to allocate memory for dynamic string");
  }

  while (1) {
    int c = fgetc(file);

    if (c == EOF) {
      dstring_free(&text);
      throw_error(LEXER_ERROR, "Unterminated interpolated string literal");
    }
    if (!multi && c == '\n') {
      dstring_free(&text);
      throw_error(LEXER_ERROR, "Newline in single-line interpolated string");
    }

    // end of string: " or """
    if (c == '"') {
      if (!multi) {
        interp_top--;
        if (text.length > 0) {
          enqueue_token(create_token(TOKEN_INTERP_END, NULL));
          return create_token(TOKEN_STR_LITERAL, &text);
        }
        dstring_free(&text);
        return create_token(TOKEN_INTERP_END, NULL);
      } else {
        int c2 = fgetc(file);
        if (c2 == '"') {
          int c3 = fgetc(file);
          if (c3 == '"') {
            interp_top--;
            if (text.length > 0) {
              enqueue_token(create_token(TOKEN_INTERP_END, NULL));
              return create_token(TOKEN_STR_LITERAL, &text);
            }
            dstring_free(&text);
            return create_token(TOKEN_INTERP_END, NULL);
          }
          dstring_append_char(&text, '"');
          dstring_append_char(&text, '"');
          if (c3 == EOF) {
            dstring_free(&text);
            throw_error(LEXER_ERROR,
                        "Unterminated multiline interpolated string");
          }
          ungetc(c3, file);
        } else {
          dstring_append_char(&text, '"');
          if (c2 == EOF) {
            dstring_free(&text);
            throw_error(LEXER_ERROR,
                        "Unterminated multiline interpolated string");
          }
          ungetc(c2, file);
        }
        continue;
      }
    }

    // escape sequence in oneline interpolated string
    if (!multi && c == '\\') {
      int esc = fgetc(file);
      if (esc == '"')
        dstring_append_char(&text, '"');
      else if (esc == 'n')
        dstring_append_char(&text, '\n');
      else if (esc == 'r')
        dstring_append_char(&text, '\r');
      else if (esc == 't')
        dstring_append_char(&text, '\t');
      else if (esc == '\\')
        dstring_append_char(&text, '\\');
      else {
        dstring_free(&text);
        throw_error(LEXER_ERROR,
                    "Invalid escape sequence in interpolated string");
      }
      continue;
    }

    // Interpolation: $$, ${expr}, $id
    if (c == '$') {
      int next = fgetc(file);
      if (next == '$') {
        dstring_append_char(&text, '$');
        continue;
      }
      if (next == '{') {
        interp_stack[interp_top].in_expr = true;
        interp_stack[interp_top].brace_depth = 1;
        if (text.length > 0) {
          enqueue_token(create_token(TOKEN_INTERP_EXPR_START, NULL));
          return create_token(TOKEN_STR_LITERAL, &text);
        }
        dstring_free(&text);
        return create_token(TOKEN_INTERP_EXPR_START, NULL);
      }
      if (isalpha(next) || next == '_') {
        dyn_string_t id_str;
        if (!dstring_init(&id_str)) {
          dstring_free(&text);
          throw_error(INTERNAL_ERROR,
                      "Failed to allocate memory for dynamic string");
        }
        dstring_append_char(&id_str, (char)next);
        int ch;
        while ((ch = fgetc(file)) != EOF && (isalnum(ch) || ch == '_')) {
          dstring_append_char(&id_str, (char)ch);
        }
        if (ch != EOF) {
          ungetc(ch, file);
        }
        if (strcmp(id_str.str, "_") == 0) {
          dstring_free(&id_str);
          dstring_free(&text);
          throw_error(LEXER_ERROR,
                      "Invalid identifier '_' in string interpolation");
        }

        Token *id_tok = create_token(TOKEN_IDENTIFIER, &id_str);
        if (text.length > 0) {
          enqueue_token(create_token(TOKEN_INTERP_EXPR_START, NULL));
          enqueue_token(id_tok);
          enqueue_token(create_token(TOKEN_INTERP_EXPR_END, NULL));
          return create_token(TOKEN_STR_LITERAL, &text);
        }
        dstring_free(&text);
        enqueue_token(id_tok);
        enqueue_token(create_token(TOKEN_INTERP_EXPR_END, NULL));
        return create_token(TOKEN_INTERP_EXPR_START, NULL);
      }

      dstring_free(&text);
      throw_error(LEXER_ERROR, "Invalid '$' sequence in interpolated string");
    }

    dstring_append_char(&text, (char)c);
  }
}

/**
 *
 **/
Token *get_token(FILE *file) {
  if (pending_head < pending_tail) {
    Token *t = pending_queue[pending_head++];
    if (pending_head == pending_tail) {
      pending_head = 0;
      pending_tail = 0;
    }
    return t;
  }
  // Check if all tokens for current indentation were sent
  if (pending_indents > 0) {
    pending_indents--;
    return create_token(TOKEN_INDENT_END, NULL);
  }

  if (interp_top >= 0 && !interp_stack[interp_top].in_expr) {
    return lex_interp_chunk(file);
  }

  // Define all states in FSM
  typedef enum {
    STATE_START,
    STATE_LESS,    // <
    STATE_GREATER, // >
    STATE_ASSIGN,  // =
    STATE_NOT,     // !
    STATE_AND,     // &
    STATE_OR,      // |
    STATE_PLUS,    // +

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

    // String literals
    STATE_STR_START,    // " Loaded
    STATE_STR_TWO_QTS,  // "" loaded
    STATE_STR,          // Normal oneline string
    STATE_STR_ESCAPE,   // Found / in standard string
    STATE_STR_MULTI,    // Multiline string
    STATE_STR_MULTI_Q1, // Found first " on exit
    STATE_STR_MULTI_Q2  // Found second "" on exit

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
      // Process start of line and indentation
      if (is_line_start) {
        if (c == ' ') {
          space_count++;
          continue;
        }
        if (c == '\t') {
          throw_error(LEXER_ERROR, "Tab key on the start of the line");
        } else if (c == '\r') {
          continue;
        } else if (c == '\n') {
          space_count = 0; // empty line, ignore
          continue;
        } else if (c == '/') {
          int next_c = fgetc(file);
          if (next_c == '/') { // one line comment -> ignore
            while (next_c != '\n' && next_c != EOF) {
              next_c = fgetc(file);
            }
            space_count = 0;
            continue;
          }
          ungetc(next_c, file);
        }

        // start of line found
        if (c != EOF) {
          is_line_start = false;

          // Compare with top of stack
          if (space_count > indent_stack[indent_top]) {
            indent_top++;
            indent_stack[indent_top] = space_count;
            ungetc(c, file);
            return create_token(TOKEN_INDENT_START, NULL);
          } else if (space_count < indent_stack[indent_top]) {
            while ((indent_top > 0) &&
                   (space_count < indent_stack[indent_top])) {
              indent_top--;
              pending_indents++; // Number of times to close the block
            }

            if (space_count != indent_stack[indent_top]) {
              throw_error(LEXER_ERROR, "Indentation error");
            }
            ungetc(c, file);
            pending_indents--;
            return create_token(TOKEN_INDENT_END, NULL);
          }
          // indentation hasnt changed
        }
      } else { // ignore whitespaces inside line
        if (c == ' ' || c == '\t' || c == '\r') {
          continue;
        }
      }
      // Check for one character tokens
      if (c == '\n') {
        if (paren_level > 0 || in_interp_expr() || bracket_level > 0) {
          continue;
        }
        is_line_start = true;
        space_count = 0; // prepare for next line
        return create_token(TOKEN_NEWLINE, NULL);
      }

      if (c == '(') {
        int next_c = fgetc(file);
        if (next_c == ')') {
          return create_token(TOKEN_UNIT_LITERAL, NULL); // ()
        } else {                                         // just ( -> go back
          ungetc(next_c, file);
          paren_level++;
          return create_token(TOKEN_LEFT_PAREN, NULL);
        }
      }
      if (c == ')') {
        if (paren_level > 0) {
          paren_level--;
        }
        return create_token(TOKEN_RIGHT_PAREN, NULL);
      }
      if (c == '@')
        return create_token(TOKEN_AT, NULL);
      if (c == ':')
        return create_token(TOKEN_COLON, NULL);
      if (c == ';')
        return create_token(TOKEN_SEMICOLON, NULL);
      if (c == '*')
        return create_token(TOKEN_ASTERISK, NULL);
      if (c == '-')
        return create_token(TOKEN_MINUS, NULL);
      if (c == ',')
        return create_token(TOKEN_COMMA, NULL);

      if (c == EOF) {
        // when eof, check if all blocks are closed, if not, close all
        if (indent_top > 0) {
          indent_top--;
          ungetc(c, file);
          return create_token(TOKEN_INDENT_END, NULL);
        }
        return create_token(TOKEN_EOF, NULL);
      }

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
      if (c == '&') {
        state = STATE_AND;
        continue;
      }
      if (c == '|') {
        state = STATE_OR;
        continue;
      }
      if (c == '+') {
        state = STATE_PLUS;
        continue;
      }
      if (c == '[') {
        bracket_level++;
        return create_token(TOKEN_LEFT_BRACKET, NULL);
      }
      if (c == ']') {
        if (bracket_level > 0) {
          bracket_level--;
        }
        return create_token(TOKEN_RIGHT_BRACKET, NULL);
      }
      // Extension INTERPOLATEDS
      if (c == '{') {
        if (in_interp_expr()) {
          interp_stack[interp_top].brace_depth++;
          continue;
        }
        throw_error(LEXER_ERROR, "Unexpected '{'");
      }
      if (c == '}') {
        if (in_interp_expr()) {
          interp_stack[interp_top].brace_depth--;
          if (interp_stack[interp_top].brace_depth == 0) {
            interp_stack[interp_top].in_expr = false;
            return create_token(TOKEN_INTERP_EXPR_END, NULL);
          }
          continue;
        }
        throw_error(LEXER_ERROR, "Unexpected '}'");
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

      // String literals
      if (c == '"') {
        state = STATE_STR_START;
        if (!dstring_init(&attribute_str)) {
          throw_error(INTERNAL_ERROR,
                      "Failed to allocate memory for dynamic string");
        }
        continue;
      }

      throw_error(LEXER_ERROR, "Invalid character on input.");
      break;
    case STATE_LESS:
      if (c == '=') { // <=
        return create_token(TOKEN_LESS_EQ, NULL);
      } else if (c == '-') {
        return create_token(TOKEN_LEFT_ARROW, NULL);
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
      } else if (c == '>') { // case and lambdas ext.
        return create_token(TOKEN_ARROW, NULL);
      } else { // =
        ungetc(c, file);
        return create_token(TOKEN_ASSIGN, NULL);
      }
      break;
    case STATE_NOT:
      if (c == '=') { // !=
        return create_token(TOKEN_NOT_EQ, NULL);
      } else { // ! (unary negation)
        ungetc(c, file);
        return create_token(TOKEN_NOT, NULL);
      }
      break;
    case STATE_AND:
      if (c == '&') { // &&
        return create_token(TOKEN_AND, NULL);
      } else {
        throw_error(LEXER_ERROR, "Expected '&' after '&'");
      }
      break;
    case STATE_OR:
      if (c == '|') { // ||
        return create_token(TOKEN_OR, NULL);
      } else { // | (union of types in ALGEBDT)
        ungetc(c, file);
        return create_token(TOKEN_PIPE, NULL);
      }
      break;

    case STATE_PLUS:
      if (c == '+') { // ++ (chaining in LISTS ext.)
        return create_token(TOKEN_PLUS_PLUS, NULL);
      } else { // +
        ungetc(c, file);
        return create_token(TOKEN_PLUS, NULL);
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
        if (paren_level > 0 || in_interp_expr() || bracket_level > 0) {
          state = STATE_START;
          continue;
        }
        is_line_start = true;
        space_count = 0;
        return create_token(TOKEN_NEWLINE, NULL);
      } else if (c == EOF) {
        ungetc(c, file);
        state = STATE_START;
        continue;
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
      } else if (c == '\n') {
        block_has_newline = true;
        state = STATE_BLOCK_COMMENT;
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
      // whitespace chars before \n are allowed
      if (c == ' ' || c == '\t' || c == '\r') {
        continue;
      } else if (c == '\n') { // end of line
        if (paren_level > 0 || in_interp_expr() || bracket_level > 0) {
          state = STATE_START;
          continue;
        }
        is_line_start = true;
        space_count = 0;
        return create_token(TOKEN_NEWLINE, NULL);
      } else if (c == EOF) {
        ungetc(c, file);
        state = STATE_START;
        continue;

      } else { // Another token after block comment is not allowed
        throw_error(LEXER_ERROR, "Tokens after newline block comment.");
      }
      break;

    case STATE_IDENTIFIER:
      // Check if all character are valid (alpha-numerical + _)
      if (isalnum(c) || c == '_') {
        dstring_append_char(&attribute_str, c);
      } else {
        // INTERPOLATEDS - catch "s prefix
        if (c == '"' && strcmp(attribute_str.str, "s") == 0) {
          dstring_free(&attribute_str);
          if (interp_top + 1 >= 16) {
            throw_error(INTERNAL_ERROR, "Interpolated string nesting too deep");
          }
          int c2 = fgetc(file);
          if (c2 == '"') {
            int c3 = fgetc(file);
            if (c3 == '"') {
              interp_stack[++interp_top] = (interp_ctx_t){true, false, 0};
              return create_token(TOKEN_INTERP_START, NULL);
            }
            if (c3 != EOF)
              ungetc(c3, file);
            // empty interpolated string s""
            dyn_string_t empty;
            dstring_init(&empty);
            return create_token(TOKEN_STR_LITERAL, &empty);
          }
          if (c2 != EOF)
            ungetc(c2, file);
          interp_stack[++interp_top] = (interp_ctx_t){false, false, 0};
          return create_token(TOKEN_INTERP_START, NULL);
        }
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
          dstring_free(&attribute_str);
          throw_error(LEXER_ERROR, "Whole number starting with zero");
        }
        dstring_append_char(&attribute_str, c);
      } else if (c == '.') {
        int next_c = fgetc(file);
        if (isdigit(next_c)) {
          dstring_append_char(&attribute_str, '.');
          dstring_append_char(&attribute_str, (char)next_c);
          state = STATE_NUM_DECIMAL;
        } else if (next_c == 'e' || next_c == 'E' || next_c == 'd' ||
                   next_c == 'D') {
          // 1.e5, 1.E+2, 1.d -> INVALID decimal num (no digit after dot)
          dstring_free(&attribute_str);
          throw_error(LEXER_ERROR,
                      "Malformed float literal after decimal point");
        } else if (isalpha(next_c) || next_c == '_') {
          // toString -> returns 5 and appends '.' to queue
          ungetc(next_c, file);
          enqueue_token(create_token(TOKEN_DOT, NULL));
          return create_token(TOKEN_NUM_LITERAL, &attribute_str);
        } else {
          dstring_free(&attribute_str);
          throw_error(LEXER_ERROR, "Wrong character after decimal point");
        }
      } else if (c == 'e' || c == 'E') {
        dstring_append_char(&attribute_str, c);
        state = STATE_NUM_E;
      } else if (c == 'd' || c == 'D') {
        dstring_append_char(&attribute_str, '.');
        dstring_append_char(&attribute_str, '0');
        state = STATE_NUM_D;
      } else if (c == '_') {
        if (left_side_zero) {
          dstring_free(&attribute_str);
          throw_error(
              LEXER_ERROR,
              "Number starting with zero cannot be followed by underscore");
        }
        state = STATE_NUM_WHOLE_USC;
      } else if (isalpha(c)) {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR, "Invalid suffix in number literal");
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
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR, "Wrong character after decimal point");
      }
      break;
    case STATE_NUM_DECIMAL: // decimal part after dot
      if (isdigit(c)) {
        dstring_append_char(&attribute_str, c);
      } else if (c == 'e' || c == 'E') {
        state = STATE_NUM_E;
        dstring_append_char(&attribute_str, c);
      } else if (c == 'd' || c == 'D') {
        state = STATE_NUM_D;
      } else if (c == '_') {
        state = STATE_NUM_DEC_USC;
      } else if (c == '.') {
        int next_c = fgetc(file);
        if (next_c == 'e' || next_c == 'E' || next_c == 'd' || next_c == 'D' ||
            isdigit(next_c)) {
          dstring_free(&attribute_str);
          throw_error(LEXER_ERROR, "Malformed float literal");
        } else if (isalpha(next_c) || next_c == '_') {
          // VALMEM: 1.5.floor, 1.5.toInt
          ungetc(next_c, file);
          enqueue_token(create_token(TOKEN_DOT, NULL));
          return create_token(TOKEN_NUM_LITERAL, &attribute_str);
        } else {
          dstring_free(&attribute_str);
          throw_error(LEXER_ERROR, "Wrong character after decimal point");
        }
      } else if (isalpha(c)) {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR, "Invalid suffix in number literal");
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
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR,
                    "Expecting digit or sign character after exponent");
      }
      break;

    case STATE_NUM_EXP_SIGN:
      if (isdigit(c)) {
        state = STATE_NUM_EXP;
        dstring_append_char(&attribute_str, c);
      } else {
        dstring_free(&attribute_str);
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
      } else if (c == '.') {
        int next_c = fgetc(file);
        if (next_c == 'e' || next_c == 'E' || next_c == 'd' || next_c == 'D' ||
            isdigit(next_c)) {
          dstring_free(&attribute_str);
          throw_error(LEXER_ERROR, "Malformed float literal");
        } else if (isalpha(next_c) || next_c == '_') {
          // VALMEM: 1e5.toInt
          ungetc(next_c, file);
          enqueue_token(create_token(TOKEN_DOT, NULL));
          return create_token(TOKEN_NUM_LITERAL, &attribute_str);
        } else {
          dstring_free(&attribute_str);
          throw_error(LEXER_ERROR, "Wrong character after decimal point");
        }
      } else if (isalpha(c)) {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR, "Invalid suffix in number literal");
      } else {
        ungetc(c, file);
        return create_token(TOKEN_NUM_LITERAL, &attribute_str);
      }
      break;

    case STATE_NUM_D:
      if (c == '.') {
        int next_c = fgetc(file);
        if (next_c == 'e' || next_c == 'E' || next_c == 'd' || next_c == 'D' ||
            isdigit(next_c)) {
          dstring_free(&attribute_str);
          throw_error(LEXER_ERROR, "Malformed float literal");
        } else if (isalpha(next_c) || next_c == '_') {
          // VALMEM: 5d.toInt
          ungetc(next_c, file);
          enqueue_token(create_token(TOKEN_DOT, NULL));
          return create_token(TOKEN_NUM_LITERAL, &attribute_str);
        } else {
          dstring_free(&attribute_str);
          throw_error(LEXER_ERROR, "Wrong character after decimal point");
        }
      } else if (isalnum(c) || c == '_') {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR, "Invalid character after double suffix");
      } else {
        ungetc(c, file);
        return create_token(TOKEN_NUM_LITERAL, &attribute_str);
      }
      break;

    case STATE_NUM_WHOLE_USC:
      if (isdigit(c)) {
        state = STATE_NUM_WHOLE; // continuing in whole part
        dstring_append_char(&attribute_str, c);
      } else {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR,
                    "Wrong syntax - underscores must be between digits");
      }
      break;
    case STATE_NUM_DEC_USC:
      if (isdigit(c)) {
        state = STATE_NUM_DECIMAL;
        dstring_append_char(&attribute_str, c);
      } else {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR,
                    "Wrong syntax - underscores must be between digits");
      }
      break;
    case STATE_NUM_EXP_USC:
      if (isdigit(c)) {
        state = STATE_NUM_EXP;
        dstring_append_char(&attribute_str, c);
      } else {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR,
                    "Wrong syntax - underscores must be between digits");
      }
      break;
    case STATE_STR_START:
      if (c == '"') {
        state = STATE_STR_TWO_QTS; // loaded ""
      } else {
        ungetc(c, file);
        state = STATE_STR; // normal string "abc"
      }
      break;
    case STATE_STR_TWO_QTS:
      if (c == '"') {
        state = STATE_STR_MULTI; // loaded """
      } else {
        ungetc(c, file);
        return create_token(TOKEN_STR_LITERAL, &attribute_str);
      }
      break;
    case STATE_STR:
      if (c == '"') {
        return create_token(TOKEN_STR_LITERAL, &attribute_str);
      } else if (c == '\\') {
        state = STATE_STR_ESCAPE; // starts escape sequence..
      } else if (c == '\n' || c == EOF) {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR,
                    "One line string contains newline character or EOF");
      } else {
        dstring_append_char(&attribute_str, c);
      }

      break;

    // EScape sequences (\n, \r, \t, \\)
    case STATE_STR_ESCAPE:
      if (c == '"')
        dstring_append_char(&attribute_str, '"');
      else if (c == 'n')
        dstring_append_char(&attribute_str, '\n');
      else if (c == 'r')
        dstring_append_char(&attribute_str, '\r');
      else if (c == 't')
        dstring_append_char(&attribute_str, '\t');
      else if (c == '\\')
        dstring_append_char(&attribute_str, '\\');
      else {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR, "Invalid escape sequence");
      }
      state = STATE_STR; // back to reading
      break;

    // Multiline string """.."""
    case STATE_STR_MULTI:
      if (c == '"') {
        state = STATE_STR_MULTI_Q1;
      } else if (c == EOF) {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR, "EOF before exiting multiline string literal");
      } else {
        dstring_append_char(&attribute_str, c);
      }
      break;
    case STATE_STR_MULTI_Q1:
      if (c == '"') {
        state = STATE_STR_MULTI_Q2; // part of end sequence ""
      } else if (c == EOF) {
        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR, "EOF before exiting multiline string literal");
      } else {
        dstring_append_char(&attribute_str, '"'); // keep the " character
        dstring_append_char(&attribute_str, c);   // and next character
        state = STATE_STR_MULTI;
      }
      break;
    case STATE_STR_MULTI_Q2:
      if (c == '"') {
        return create_token(TOKEN_STR_LITERAL,
                            &attribute_str); // """ loaded -> string ends
      } else if (c == EOF) {

        dstring_free(&attribute_str);
        throw_error(LEXER_ERROR, "EOF before exiting multiline string literal");
      } else {
        dstring_append_char(&attribute_str, '"');
        dstring_append_char(&attribute_str, '"'); // keep both ""
        dstring_append_char(&attribute_str, c);
        state = STATE_STR_MULTI;
      }
      break;
    }
  }
}
