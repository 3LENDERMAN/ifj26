/**
 * IFJ26 Compiler - Parser implementation
 * Author: Vít Kutěj (xkutejv00)
 * Date: 11.10.2026
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include "parser.h"

/**
 * Parser error helper function that format the message and call throw_error
 **/
static void parser_error(Error_code code, const char *fmt, ...)
{
    char msg[256];
    va_list arg_pointer;
    va_start(arg_pointer, fmt);
    vsnprintf(msg, sizeof(msg), fmt, arg_pointer);
    va_end(arg_pointer);
    throw_error(code, msg);
}

/**
 * Helper function to get string representation of token type
 **/
static const char *token_type_string(token_type type)
{
    switch (type)
    {
    case TOKEN_LEFT_PAREN:
        return "'('";
    case TOKEN_RIGHT_PAREN:
        return "')'";
    case TOKEN_DOT:
        return "'.'";
    case TOKEN_ASTERISK:
        return "'*'";
    case TOKEN_ASSIGN:
        return "'='";
    case TOKEN_COMMA:
        return "','";
    case TOKEN_SEMICOLON:
        return "';'";
    case TOKEN_NEWLINE:
        return "newline";
    case TOKEN_AT:
        return "'@'";
    case TOKEN_COLON:
        return "':'";
    case TOKEN_KW_IF:
        return "'if'";
    case TOKEN_KW_ELSE:
        return "'else'";
    case TOKEN_KW_WHILE:
        return "'while'";
    case TOKEN_KW_RETURN:
        return "'return'";
    case TOKEN_KW_DEF:
        return "'def'";
    case TOKEN_KW_DO:
        return "'do'";
    case TOKEN_KW_IMPORT:
        return "'import'";
    case TOKEN_KW_THEN:
        return "'then'";
    case TOKEN_KW_VAL:
        return "'val'";
    case TOKEN_KW_VAR:
        return "'var'";
    case TOKEN_KW_INT:
        return "'Int'";
    case TOKEN_KW_DOUBLE:
        return "'Double'";
    case TOKEN_KW_STRING:
        return "'String'";
    case TOKEN_KW_UNIT:
        return "'Unit'";
    case TOKEN_IDENTIFIER:
        return "identifier";
    case TOKEN_NUM_LITERAL:
        return "number literal";
    case TOKEN_INT_LITERAL:
        return "integer literal";
    case TOKEN_DOUBLE_LITERAL:
        return "double literal";
    case TOKEN_STR_LITERAL:
        return "string literal";
    case TOKEN_UNIT_LITERAL:
        return "'()'";
    case TOKEN_INDENT_START:
        return "indentation";
    case TOKEN_INDENT_END:
        return "dedentation";
    case TOKEN_EOF:
        return "end of file";
    default:
        return "other token";
    }
}

/**
 * Get type of the token at given offset
 **/
static token_type get_peek_type(TokenStream *stream, int offset)
{
    const Token *token = ts_peek(stream, offset);
    return token ? token->type : TOKEN_EOF;
}

/**
 * Check if current token has the given type
 **/
static bool token_is(TokenStream *stream, token_type type)
{
    return get_peek_type(stream, 0) == type;
}

/**
 * Consume the current token if the type matches, otherwise syntax error
 **/
static const Token *token_expect(TokenStream *stream, token_type type, const char *err_msg)
{
    const Token *token = ts_peek(stream, 0);
    if (!token || token->type != type)
    {
        parser_error(SYNTAX_ERROR, "Expected %s, but got %s",
                     err_msg, token ? token_type_string(token->type) : "nothing");
    }
    return ts_next(stream);
}

/**
 * Consume the current token only if the type matches, return true if consumed
 **/
static bool token_match(TokenStream *stream, token_type type)
{
    if (token_is(stream, type))
    {
        ts_next(stream);
        return true;
    }
    return false;
}

/**
 * Skip empty lines and comments
 **/
static void skip_blank_lines(TokenStream *stream)
{
    while (token_is(stream, TOKEN_NEWLINE))
    {
        ts_next(stream);
    }
}

/**
 * Check if statement have correct ending: newline, semicolon or end of block
 **/
static void check_statement_end(TokenStream *stream)
{
    if (token_match(stream, TOKEN_SEMICOLON))
        return;
    if (token_is(stream, TOKEN_NEWLINE))
    {
        ts_next(stream);
        return;
    }
    // Last statement in a block dont need a semicolon or newline, but EOF or TOKEN_INDENT_END is required
    if (token_is(stream, TOKEN_EOF) || token_is(stream, TOKEN_INDENT_END))
        return;

    parser_error(SYNTAX_ERROR, "Expected newline or end of block at end of statement, but got %s",
                 token_type_string(get_peek_type(stream, 0)));
}

/**
 * Check if the statement ends at the current token, or if there is a value after return
 **/
static bool at_statement_boundary(TokenStream *stream)
{
    return token_is(stream, TOKEN_NEWLINE) || token_is(stream, TOKEN_SEMICOLON) ||
           token_is(stream, TOKEN_EOF) || token_is(stream, TOKEN_INDENT_END);
}

/**
 * Get string value of a token, or empty string if it is NULL
 **/
static const char *safe_str(const Token *t)
{
    return t->attribute.str_val ? t->attribute.str_val : "";
}

static program *parse_program(TokenStream *stream);
static func_def *parse_function(TokenStream *stream);
static data_type parse_datatype(TokenStream *stream);
static block parse_block(TokenStream *stream);
static block parse_body(TokenStream *stream);
static stmt *parse_stmt(TokenStream *stream);
static stmt *parse_if_clause(TokenStream *stream);
static stmt *parse_while_loop(TokenStream *stream);

/**
 * Parse the whole program from the token stream and return the AST
 **/
program *parse(TokenStream *stream)
{
    return parse_program(stream);
}

/**
 * Parse the prolog and then all function definitions until end of file
 **/
static program *parse_program(TokenStream *stream)
{
    program *prog = ast_xcalloc(1, sizeof(program));

    skip_blank_lines(stream);

    token_expect(stream, TOKEN_KW_IMPORT, "'import'");
    const Token *pckg = token_expect(stream, TOKEN_IDENTIFIER, "package name 'ifj26'");
    if (strcmp(safe_str(pckg), "ifj26") != 0)
    {
        parser_error(SYNTAX_ERROR, "Expected package 'ifj26' in prolog");
    }
    token_expect(stream, TOKEN_DOT, "'.'");
    token_expect(stream, TOKEN_ASTERISK, "'*'");
    check_statement_end(stream);

    skip_blank_lines(stream);

    // Only function definitions are allowed on the top level
    while (!token_is(stream, TOKEN_EOF))
    {
        func_def *funct = parse_function(stream);

        prog->count++;
        prog->funcs = ast_xrealloc(prog->funcs, sizeof(func_def *) * prog->count);
        prog->funcs[prog->count - 1] = funct;

        skip_blank_lines(stream);
    }

    token_expect(stream, TOKEN_EOF, "end of file");
    return prog;
}

// Placeholder
static func_def *parse_function(TokenStream *stream)
{

    (void)stream;
    return NULL;
}