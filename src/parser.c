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

/**
 * Parse function, params, return type and body, also check for @main
 **/
static func_def *parse_function(TokenStream *stream)
{
    bool main_flag = false;
    if (token_match(stream, TOKEN_AT))
    {
        const Token *id_token = token_expect(stream, TOKEN_IDENTIFIER, "'main' after '@'");
        if (strcmp(safe_str(id_token), "main") != 0)
        {
            parser_error(SYNTAX_ERROR, "Expected 'main' after '@'");
        }
        main_flag = true;
        skip_blank_lines(stream);
    }

    token_expect(stream, TOKEN_KW_DEF, "'def'");
    const Token *id_token = token_expect(stream, TOKEN_IDENTIFIER, "function name");

    func_def *funct = ast_xcalloc(1, sizeof(func_def));
    funct->name = ast_xstrdup(safe_str(id_token));
    (void)main_flag;
    funct->ret_type = TYPE_UNKNOWN;

    // The lexer returns "()" as one UNIT_LITERAL token, so no parameters
    if (!token_match(stream, TOKEN_UNIT_LITERAL))
    {
        token_expect(stream, TOKEN_LEFT_PAREN, "'('");
        if (!token_is(stream, TOKEN_RIGHT_PAREN))
        {
            do
            {
                if (token_is(stream, TOKEN_RIGHT_PAREN))
                    break;

                const Token *param_id = token_expect(stream, TOKEN_IDENTIFIER, "parameter name");
                token_expect(stream, TOKEN_COLON, "':' before parameter type");
                data_type param_type = parse_datatype(stream);

                funct->param_count++;
                funct->params = ast_xrealloc(funct->params, sizeof(param) * funct->param_count);
                funct->params[funct->param_count - 1] = (param){
                    .name = ast_xstrdup(safe_str(param_id)),
                    .type = param_type};

            } while (token_match(stream, TOKEN_COMMA));
        }
        token_expect(stream, TOKEN_RIGHT_PAREN, "')'");
    }

    // Return type is optional
    if (token_match(stream, TOKEN_COLON))
    {
        funct->ret_type = parse_datatype(stream);
    }

    token_expect(stream, TOKEN_ASSIGN, "'='");

    if (token_is(stream, TOKEN_NEWLINE))
    {
        // There is a newline after the function header, and the body must be indented block
        skip_blank_lines(stream);
        funct->body = parse_block(stream);
    }
    else
    {
        // Body on one line must be single expression
        expr *single_expr = parse_expression(stream);
        check_statement_end(stream);

        stmt *statement = ast_xcalloc(1, sizeof(stmt));
        statement->type = STMT_EXPR;
        statement->as.expr.expr = single_expr;

        funct->body.count = 1;
        funct->body.items = ast_xcalloc(1, sizeof(stmt *));
        funct->body.items[0] = statement;
    }

    return funct;
}

/**
 * Parse a data type
 **/
static data_type parse_datatype(TokenStream *stream)
{
    const Token *token = ts_peek(stream, 0);
    if (token)
    {
        switch (token->type)
        {
        case TOKEN_KW_INT:
            ts_next(stream);
            return TYPE_INT;
        case TOKEN_KW_DOUBLE:
            ts_next(stream);
            return TYPE_DOUBLE;
        case TOKEN_KW_STRING:
            ts_next(stream);
            return TYPE_STRING;
        case TOKEN_KW_UNIT:
            ts_next(stream);
            return TYPE_UNIT;
        default:
            break;
        }
    }

    parser_error(SYNTAX_ERROR, "Expected valid data type, but got %s",
                 token ? token_type_name(token->type) : "nothing");
    return TYPE_UNKNOWN;
}

/**
 * Parse indented block
 **/
static block parse_block(TokenStream *stream)
{
    block blck = {0};

    token_expect(stream, TOKEN_INDENT_START, "indent start");
    skip_blank_lines(stream);

    while (!token_is(stream, TOKEN_INDENT_END) && !token_is(stream, TOKEN_EOF))
    {
        stmt *statement = parse_stmt(stream);
        blck.count++;
        blck.items = ast_xrealloc(blck.items, sizeof(stmt *) * blck.count);
        blck.items[blck.count - 1] = statement;
        skip_blank_lines(stream);
    }

    // Blocks must have at least one statement
    if (blck.count == 0)
    {
        parser_error(SYNTAX_ERROR, "Block cannot be empty");
    }

    token_expect(stream, TOKEN_INDENT_END, "indent end");
    return blck;
}

// Placeholder
static stmt *parse_stmt(TokenStream *stream)
{
    return NULL;
}

// Placeholder
static expr *parse_expression(TokenStream *stream)
{
    return NULL;
}