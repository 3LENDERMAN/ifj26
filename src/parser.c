/**
 * IFJ26 Compiler - Parser implementation
 * Author: Vít Kutěj (xkutejv00)
 * Date: 12.10.2026
 **/

#include "parser.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Parser error helper function that format the message and call throw_error
 **/
static void parser_error(Error_code code, const char *fmt, ...) {
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
static const char *token_type_string(token_type type) {
  switch (type) {
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
  case TOKEN_KW_BOOLEAN:
    return "'Boolean'";
  case TOKEN_KW_TRUE:
    return "'true'";
  case TOKEN_KW_FALSE:
    return "'false'";
  case TOKEN_NOT:
    return "'!'";
  case TOKEN_AND:
    return "'&&'";
  case TOKEN_OR:
    return "'||'";
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
  case TOKEN_KW_FOR:
    return "'for'";
  case TOKEN_KW_CASE:
    return "'case'";
  case TOKEN_KW_CLASS:
    return "'class'";
  case TOKEN_KW_TYPE:
    return "'type'";
  case TOKEN_KW_MATCH:
    return "'match'";
  case TOKEN_KW_LIST:
    return "'List'";
  case TOKEN_ARROW:
    return "'=>'";
  case TOKEN_LEFT_ARROW:
    return "'<-'";
  case TOKEN_PIPE:
    return "'|'";
  case TOKEN_UNDERSCORE:
    return "'_'";
  case TOKEN_LEFT_BRACKET:
    return "'['";
  case TOKEN_RIGHT_BRACKET:
    return "']'";
  default:
    return "other token";
  }
}

/**
 * Get type of the token at given offset
 **/
static token_type get_peek_type(TokenStream *stream, int offset) {
  const Token *token = ts_peek(stream, offset);
  return token ? token->type : TOKEN_EOF;
}

/**
 * Check if current token has the given type
 **/
static bool token_is(TokenStream *stream, token_type type) {
  return get_peek_type(stream, 0) == type;
}

/**
 * Consume the current token if the type matches, otherwise syntax error
 **/
static const Token *token_expect(TokenStream *stream, token_type type,
                                 const char *err_msg) {
  const Token *token = ts_peek(stream, 0);
  if (!token || token->type != type) {
    parser_error(SYNTAX_ERROR, "Expected %s, but got %s", err_msg,
                 token ? token_type_string(token->type) : "nothing");
  }
  return ts_next(stream);
}

/**
 * Consume the current token only if the type matches, return true if consumed
 **/
static bool token_match(TokenStream *stream, token_type type) {
  if (token_is(stream, type)) {
    ts_next(stream);
    return true;
  }
  return false;
}

/**
 * Skip empty lines and comments
 **/
static void skip_blank_lines(TokenStream *stream) {
  while (token_is(stream, TOKEN_NEWLINE)) {
    ts_next(stream);
  }
}

/**
 * Check if statement have correct ending: newline, semicolon or end of block
 **/
static void check_statement_end(TokenStream *stream) {
  if (token_match(stream, TOKEN_SEMICOLON) ||
      token_match(stream, TOKEN_NEWLINE)) {
    return;
  }
  // Last statement in a block dont need a semicolon or newline, but EOF or
  // TOKEN_INDENT_END is required
  // CONTROL: single-line branch followed by else (if c then a else b)
  if (token_is(stream, TOKEN_EOF) || token_is(stream, TOKEN_INDENT_END) ||
      token_is(stream, TOKEN_KW_ELSE)) {
    return;
  }
  parser_error(
      SYNTAX_ERROR,
      "Expected newline or end of block at end of statement, but got %s",
      token_type_string(get_peek_type(stream, 0)));
}

/**
 * Check if the statement ends at the current token, or if there is a value
 *after return
 **/
static bool at_statement_boundary(TokenStream *stream) {
  return token_is(stream, TOKEN_NEWLINE) || token_is(stream, TOKEN_SEMICOLON) ||
         token_is(stream, TOKEN_EOF) || token_is(stream, TOKEN_INDENT_END);
}

/**
 * Get string value of a token, or empty string if it is NULL
 **/
static const char *safe_str(const Token *t) {
  return t->attribute.str_val ? t->attribute.str_val : "";
}

static program *parse_program(TokenStream *stream);
static func_def *parse_function(TokenStream *stream);
static data_type parse_datatype(TokenStream *stream);
static type_info *parse_type(TokenStream *stream);
static type_info *parse_type_primary(TokenStream *stream);
static param *parse_param_list(TokenStream *stream, int *count);
static block parse_branch(TokenStream *stream);
static block parse_block(TokenStream *stream);
static block parse_body(TokenStream *stream);
static stmt *parse_stmt(TokenStream *stream);
static stmt *parse_if_clause(TokenStream *stream);
static stmt *parse_while_loop(TokenStream *stream);
static stmt *parse_for_loop(TokenStream *stream);
static expr *parse_if_expr(TokenStream *stream);
static expr *parse_lambda(TokenStream *stream);
static expr *parse_match(TokenStream *stream, expr *subject);
static type_def parse_type_def(TokenStream *stream);

// Last value ended by an indented block (lambda, if expression, match) that
// already consumed the line ending, so the statement end is not required
static bool value_ended_with_block = false;

static type_info *make_type(data_type kind) {
  type_info *type = ast_xcalloc(1, sizeof(type_info));
  type->kind = kind;
  return type;
}

static expr *make_expr(expr_type type) {
  expr *e = ast_xcalloc(1, sizeof(expr));
  e->e_type = type;
  e->d_type = TYPE_UNKNOWN;
  return e;
}

static stmt *make_stmt(stmt_type type) {
  stmt *statement = ast_xcalloc(1, sizeof(stmt));
  statement->type = type;
  return statement;
}

static block block_of(stmt *statement) {
  block blck = {.items = ast_xcalloc(1, sizeof(stmt *)), .count = 1};
  blck.items[0] = statement;
  return blck;
}

static block block_of_expr(expr *expression) {
  stmt *statement = make_stmt(STMT_EXPR);
  statement->as.expr.expr = expression;
  return block_of(statement);
}

// Statement end after a value, not required after an indented block
static void check_value_end(TokenStream *stream) {
  if (value_ended_with_block) {
    value_ended_with_block = false;
    return;
  }
  check_statement_end(stream);
}

// Consume else, also when it is on the next line, false if it is missing
static bool match_else(TokenStream *stream) {
  int offset = 0;
  while (offset < 7 && get_peek_type(stream, offset) == TOKEN_NEWLINE) {
    offset++;
  }
  if (get_peek_type(stream, offset) != TOKEN_KW_ELSE) {
    return false;
  }
  for (int i = 0; i <= offset; i++) {
    ts_next(stream);
  }
  return true;
}

// Beginning of a lambda: (id: or () =>
static bool at_lambda(TokenStream *stream) {
  if (token_is(stream, TOKEN_LEFT_PAREN)) {
    return get_peek_type(stream, 1) == TOKEN_IDENTIFIER &&
           get_peek_type(stream, 2) == TOKEN_COLON;
  }
  return token_is(stream, TOKEN_UNIT_LITERAL) &&
         get_peek_type(stream, 1) == TOKEN_ARROW;
}

static expr *number_literal(const char *text) {
  expr *e;
  if (strpbrk(text, ".eE") != NULL) {
    e = make_expr(EXPR_DOUBLE);
    e->as.double_val = strtod(text, NULL);
  } else {
    e = make_expr(EXPR_INT);
    e->as.int_val = strtoll(text, NULL, 10);
  }
  return e;
}

// If expression, lambda or expression [match], also for arguments of calls
expr *parse_value(TokenStream *stream) {
  value_ended_with_block = false;

  // Value on the next indented line, e.g. val f: (Int) => Int =
  //                                          (a: Int) => a + 1
  if (token_is(stream, TOKEN_NEWLINE)) {
    skip_blank_lines(stream);
    token_expect(stream, TOKEN_INDENT_START, "indented value");
    expr *inner = parse_value(stream);
    check_value_end(stream);
    skip_blank_lines(stream);
    token_expect(stream, TOKEN_INDENT_END, "indent end");
    value_ended_with_block = true;
    return inner;
  }

  if (token_is(stream, TOKEN_KW_IF)) {
    return parse_if_expr(stream);
  }
  if (at_lambda(stream)) {
    return parse_lambda(stream);
  }

  expr *value = parse_expression(stream);
  while (token_is(stream, TOKEN_KW_MATCH)) {
    value = parse_match(stream, value);
  }
  return value;
}

/**
 * Parse the whole program from the token stream and return the AST
 **/
program *parse(TokenStream *stream) { return parse_program(stream); }

/**
 * Parse the prolog and then all function definitions until end of file
 **/
static program *parse_program(TokenStream *stream) {
  program *prog = ast_xcalloc(1, sizeof(program));

  skip_blank_lines(stream);

  token_expect(stream, TOKEN_KW_IMPORT, "'import'");
  const Token *pckg =
      token_expect(stream, TOKEN_IDENTIFIER, "package name 'ifj26'");
  if (strcmp(safe_str(pckg), "ifj26") != 0) {
    parser_error(SYNTAX_ERROR, "Expected package 'ifj26' in prolog");
  }
  token_expect(stream, TOKEN_DOT, "'.'");
  token_expect(stream, TOKEN_ASTERISK, "'*'");
  check_statement_end(stream);

  skip_blank_lines(stream);

  // Only function and type definitions are allowed on the top level
  while (!token_is(stream, TOKEN_EOF)) {
    if (token_is(stream, TOKEN_KW_TYPE) || token_is(stream, TOKEN_KW_CASE)) {
      APPEND(prog->types, prog->type_count, parse_type_def(stream));
      skip_blank_lines(stream);
      continue;
    }

    APPEND(prog->funcs, prog->count, parse_function(stream));
    skip_blank_lines(stream);
  }

  token_expect(stream, TOKEN_EOF, "end of file");
  return prog;
}

/**
 * Parse function, params, return type and body, also check for @main
 **/
static func_def *parse_function(TokenStream *stream) {
  bool main_flag = false;
  if (token_match(stream, TOKEN_AT)) {
    const Token *id_token =
        token_expect(stream, TOKEN_IDENTIFIER, "'main' after '@'");
    if (strcmp(safe_str(id_token), "main") != 0) {
      parser_error(SYNTAX_ERROR, "Expected 'main' after '@'");
    }
    main_flag = true;
    skip_blank_lines(stream);
  }

  token_expect(stream, TOKEN_KW_DEF, "'def'");
  const Token *id_token =
      token_expect(stream, TOKEN_IDENTIFIER, "function name");

  func_def *funct = ast_xcalloc(1, sizeof(func_def));
  funct->name = ast_xstrdup(safe_str(id_token));
  funct->is_main = main_flag;
  funct->ret_type = TYPE_UNKNOWN;

  funct->params = parse_param_list(stream, &funct->param_count);

  // Return type is optional
  if (token_match(stream, TOKEN_COLON)) {
    funct->ret_full = parse_type(stream);
    funct->ret_type = funct->ret_full->kind;
  }

  token_expect(stream, TOKEN_ASSIGN, "'='");

  if (token_is(stream, TOKEN_NEWLINE)) {
    // There is a newline after the function header, and the body must be
    // indented block
    skip_blank_lines(stream);
    funct->body = parse_block(stream);
  } else {
    // Body on one line must be single expression
    funct->body = block_of_expr(parse_value(stream));
    check_value_end(stream);
  }

  return funct;
}

/**
 * Parse a data type
 **/
static data_type parse_datatype(TokenStream *stream) {
  static const struct {
    token_type token;
    data_type type;
  } types[] = {{TOKEN_KW_INT, TYPE_INT},
               {TOKEN_KW_DOUBLE, TYPE_DOUBLE},
               {TOKEN_KW_STRING, TYPE_STRING},
               {TOKEN_KW_UNIT, TYPE_UNIT},
               {TOKEN_KW_BOOLEAN, TYPE_BOOL}};
  for (size_t i = 0; i < sizeof(types) / sizeof(*types); i++) {
    if (token_match(stream, types[i].token)) {
      return types[i].type;
    }
  }

  const Token *token = ts_peek(stream, 0);
  parser_error(SYNTAX_ERROR, "Expected valid data type, but got %s",
               token ? token_type_string(token->type) : "nothing");
  return TYPE_UNKNOWN;
}

/**
 * Parse indented block
 **/
static block parse_block(TokenStream *stream) {
  block blck = {0};

  token_expect(stream, TOKEN_INDENT_START, "indent start");
  skip_blank_lines(stream);

  while (!token_is(stream, TOKEN_INDENT_END) && !token_is(stream, TOKEN_EOF)) {
    APPEND(blck.items, blck.count, parse_stmt(stream));
    skip_blank_lines(stream);
  }

  // Blocks must have at least one statement
  if (blck.count == 0) {
    parser_error(SYNTAX_ERROR, "Block cannot be empty");
  }

  token_expect(stream, TOKEN_INDENT_END, "indent end");
  return blck;
}

/**
 * Parse statement, var, val, if, while, return, assignment or expression
 **/
static stmt *parse_stmt(TokenStream *stream) {
  if (token_is(stream, TOKEN_KW_VAL) || token_is(stream, TOKEN_KW_VAR)) {
    stmt *statement =
        make_stmt(token_is(stream, TOKEN_KW_VAL) ? STMT_VAL : STMT_VAR);
    ts_next(stream);

    const Token *var_token = token_expect(stream, TOKEN_IDENTIFIER, "var name");
    statement->as.decl.name = ast_xstrdup(safe_str(var_token));
    statement->as.decl.declared = TYPE_UNKNOWN;
    if (token_match(stream, TOKEN_COLON)) {
      statement->as.decl.declared_full = parse_type(stream);
      statement->as.decl.declared = statement->as.decl.declared_full->kind;
    }

    token_expect(stream, TOKEN_ASSIGN, "'='");
    statement->as.decl.init = parse_value(stream);
    check_value_end(stream);
    return statement;
  }

  if (token_is(stream, TOKEN_KW_IF)) {
    return parse_if_clause(stream);
  }

  if (token_is(stream, TOKEN_KW_WHILE)) {
    return parse_while_loop(stream);
  }

  if (token_is(stream, TOKEN_KW_FOR)) {
    return parse_for_loop(stream);
  }

  // Return with optional expression
  if (token_match(stream, TOKEN_KW_RETURN)) {
    stmt *statement = make_stmt(STMT_RETURN);
    if (!at_statement_boundary(stream)) {
      statement->as.expr.expr = parse_value(stream);
    }
    check_value_end(stream);
    return statement;
  }

  // Lookahead to avoid confusion between assignment and expression
  if (get_peek_type(stream, 0) == TOKEN_IDENTIFIER &&
      get_peek_type(stream, 1) == TOKEN_ASSIGN) {
    stmt *statement = make_stmt(STMT_ASSIGN);
    statement->as.assign.name = ast_xstrdup(safe_str(ts_next(stream)));
    ts_next(stream);
    statement->as.assign.value = parse_value(stream);
    check_value_end(stream);
    return statement;
  }

  // Expression statement or function call
  stmt *statement = make_stmt(STMT_EXPR);
  statement->as.expr.expr = parse_value(stream);
  check_value_end(stream);
  return statement;
}

/**
 * Parse body after then, else or do
 **/
static block parse_body(TokenStream *stream) {
  if (!token_is(stream, TOKEN_NEWLINE)) {
    // CONTROL: single-line body instead of the indented block
    if (token_is(stream, TOKEN_INDENT_START) || token_is(stream, TOKEN_EOF)) {
      parser_error(SYNTAX_ERROR, "Expected newline and indented block");
    }
    return block_of(parse_stmt(stream));
  }
  skip_blank_lines(stream);
  return parse_block(stream);
}

/**
 * Parse if statement
 **/
static stmt *parse_if_clause(TokenStream *stream) {
  stmt *statement = make_stmt(STMT_IF);
  token_expect(stream, TOKEN_KW_IF, "'if'");
  statement->as.if_.cond = parse_expression(stream);
  token_expect(stream, TOKEN_KW_THEN, "'then'");
  statement->as.if_.then_b = parse_body(stream);

  if (match_else(stream)) {
    statement->as.if_.else_b = token_is(stream, TOKEN_KW_IF)
                                   ? block_of(parse_if_clause(stream))
                                   : parse_body(stream);
  }
  return statement;
}

/**
 * Parse while loop
 **/
static stmt *parse_while_loop(TokenStream *stream) {
  stmt *statement = make_stmt(STMT_WHILE);
  token_expect(stream, TOKEN_KW_WHILE, "'while'");
  statement->as.while_.cond = parse_expression(stream);
  token_expect(stream, TOKEN_KW_DO, "'do'");
  statement->as.while_.body = parse_body(stream);
  return statement;
}

// Type without |: primitive, List[T], (A, B) => R, alias or case class name
static type_info *parse_type_primary(TokenStream *stream) {
  const Token *token = ts_peek(stream, 0);

  if (token) {
    switch (token->type) {
    case TOKEN_KW_LIST: {
      ts_next(stream);
      token_expect(stream, TOKEN_LEFT_BRACKET, "'[' after 'List'");
      type_info *elem = parse_type(stream);
      token_expect(stream, TOKEN_RIGHT_BRACKET, "']'");
      type_info *list = make_type(TYPE_LIST);
      list->as.elem_type = elem;
      return list;
    }
    case TOKEN_IDENTIFIER: {
      ts_next(stream);
      type_info *named = make_type(TYPE_CASE_CLASS);
      named->as.class_name = ast_xstrdup(safe_str(token));
      return named;
    }
    case TOKEN_UNIT_LITERAL: {
      // () => R
      ts_next(stream);
      token_expect(stream, TOKEN_ARROW, "'=>' in function type");
      type_info *func = make_type(TYPE_FUNC);
      func->as.func_t.ret_type = parse_type(stream);
      return func;
    }
    case TOKEN_LEFT_PAREN: {
      // (A, B) => R
      ts_next(stream);
      type_info *func = make_type(TYPE_FUNC);
      do {
        if (token_is(stream, TOKEN_RIGHT_PAREN))
          break;
        APPEND(func->as.func_t.param_types, func->as.func_t.param_count,
               parse_type(stream));
      } while (token_match(stream, TOKEN_COMMA));
      token_expect(stream, TOKEN_RIGHT_PAREN, "')'");
      token_expect(stream, TOKEN_ARROW, "'=>' in function type");
      func->as.func_t.ret_type = parse_type(stream);
      return func;
    }
    default:
      break;
    }
  }

  return make_type(parse_datatype(stream));
}

// Type including unions (A | B | C)
static type_info *parse_type(TokenStream *stream) {
  type_info *first = parse_type_primary(stream);
  if (!token_is(stream, TOKEN_PIPE)) {
    return first;
  }

  type_info *uni = make_type(TYPE_UNION);
  APPEND(uni->as.union_t.members, uni->as.union_t.count, first);
  while (token_match(stream, TOKEN_PIPE)) {
    APPEND(uni->as.union_t.members, uni->as.union_t.count,
           parse_type_primary(stream));
  }
  return uni;
}

// Parameters (id: Type, ...) or () of functions, lambdas and case classes
static param *parse_param_list(TokenStream *stream, int *count) {
  param *params = NULL;
  *count = 0;

  // The lexer returns "()" as one UNIT_LITERAL token, so no parameters
  if (token_match(stream, TOKEN_UNIT_LITERAL)) {
    return NULL;
  }

  token_expect(stream, TOKEN_LEFT_PAREN, "'('");
  do {
    if (token_is(stream, TOKEN_RIGHT_PAREN))
      break;

    const Token *param_id =
        token_expect(stream, TOKEN_IDENTIFIER, "parameter name");
    token_expect(stream, TOKEN_COLON, "':' before parameter type");
    type_info *param_full = parse_type(stream);
    APPEND(params, *count,
           ((param){.name = ast_xstrdup(safe_str(param_id)),
                    .type = param_full->kind,
                    .full_type = param_full}));
  } while (token_match(stream, TOKEN_COMMA));
  token_expect(stream, TOKEN_RIGHT_PAREN, "')'");
  return params;
}

// ALGEBDT: definition of a type alias or a case class
static type_def parse_type_def(TokenStream *stream) {
  type_def def = {0};

  if (token_match(stream, TOKEN_KW_TYPE)) {
    const Token *id_token = token_expect(stream, TOKEN_IDENTIFIER, "type name");
    token_expect(stream, TOKEN_ASSIGN, "'='");
    def.name = ast_xstrdup(safe_str(id_token));
    def.is_class = false;
    def.alias = parse_type(stream);
  } else {
    token_expect(stream, TOKEN_KW_CASE, "'case'");
    token_expect(stream, TOKEN_KW_CLASS, "'class'");
    const Token *id_token =
        token_expect(stream, TOKEN_IDENTIFIER, "case class name");
    def.name = ast_xstrdup(safe_str(id_token));
    def.is_class = true;
    def.fields = parse_param_list(stream, &def.field_count);
  }

  check_statement_end(stream);
  return def;
}

// Branch of if expression: indented block or single expression
static block parse_branch(TokenStream *stream) {
  if (token_is(stream, TOKEN_NEWLINE)) {
    block blck = parse_body(stream);
    value_ended_with_block = true;
    return blck;
  }
  expr *value = parse_value(stream);
  return block_of_expr(value);
}

// CONTROL: if expression, the else branch is mandatory
static expr *parse_if_expr(TokenStream *stream) {
  expr *e = make_expr(EXPR_IF);

  token_expect(stream, TOKEN_KW_IF, "'if'");
  e->as.if_.cond = parse_expression(stream);
  token_expect(stream, TOKEN_KW_THEN, "'then'");
  e->as.if_.then_b = parse_branch(stream);

  if (!match_else(stream)) {
    skip_blank_lines(stream);
    parser_error(SYNTAX_ERROR, "Expected 'else' in 'if' expression, but got %s",
                 token_type_string(get_peek_type(stream, 0)));
  }
  e->as.if_.else_b = token_is(stream, TOKEN_KW_IF)
                         ? block_of_expr(parse_if_expr(stream))
                         : parse_branch(stream);
  return e;
}

// LAMBDAS: (a: Int, b: Int) => body
static expr *parse_lambda(TokenStream *stream) {
  expr *e = make_expr(EXPR_LAMBDA);

  e->as.lambda.params = parse_param_list(stream, &e->as.lambda.param_count);
  token_expect(stream, TOKEN_ARROW, "'=>'");

  if (token_is(stream, TOKEN_NEWLINE)) {
    e->as.lambda.body = parse_body(stream);
    value_ended_with_block = true;
  } else {
    e->as.lambda.body = block_of_expr(parse_value(stream));
  }
  return e;
}

// ALGEBDT: case pattern [if guard] => body
static match_case parse_case(TokenStream *stream) {
  match_case branch = {0};
  token_expect(stream, TOKEN_KW_CASE, "'case'");

  const Token *token = ts_peek(stream, 0);
  if (!token) {
    parser_error(SYNTAX_ERROR, "Expected pattern, but got nothing");
  }

  if (token->type == TOKEN_UNDERSCORE) {
    ts_next(stream);
    branch.pat = PAT_WILDCARD;
  } else if (token->type == TOKEN_IDENTIFIER &&
             get_peek_type(stream, 1) == TOKEN_COLON) {
    // id: Type
    ts_next(stream);
    ts_next(stream);
    branch.pat = PAT_TYPED;
    branch.bind_name = ast_xstrdup(safe_str(token));
    branch.bind_type = parse_type_primary(stream);
  } else if (token->type == TOKEN_IDENTIFIER &&
             (get_peek_type(stream, 1) == TOKEN_LEFT_PAREN ||
              get_peek_type(stream, 1) == TOKEN_UNIT_LITERAL)) {
    // Point(a, b)
    ts_next(stream);
    branch.pat = PAT_CTOR;
    branch.class_name = ast_xstrdup(safe_str(token));
    if (!token_match(stream, TOKEN_UNIT_LITERAL)) {
      token_expect(stream, TOKEN_LEFT_PAREN, "'('");
      do {
        if (token_is(stream, TOKEN_RIGHT_PAREN))
          break;
        const Token *field = ts_peek(stream, 0);
        if (!field || (field->type != TOKEN_IDENTIFIER &&
                       field->type != TOKEN_UNDERSCORE)) {
          parser_error(SYNTAX_ERROR,
                       "Expected name or '_' in pattern, but got %s",
                       field ? token_type_string(field->type) : "nothing");
        }
        ts_next(stream);
        APPEND(branch.fields, branch.field_count,
               ast_xstrdup(field->type == TOKEN_UNDERSCORE ? "_"
                                                           : safe_str(field)));
      } while (token_match(stream, TOKEN_COMMA));
      token_expect(stream, TOKEN_RIGHT_PAREN, "')'");
    }
  } else {
    // literal
    branch.pat = PAT_LITERAL;
    bool negative = token_match(stream, TOKEN_MINUS);
    const Token *lit = ts_next(stream);
    expr *value = NULL;

    switch (lit->type) {
    case TOKEN_NUM_LITERAL:
      value = number_literal(safe_str(lit));
      break;
    case TOKEN_INT_LITERAL:
      value = make_expr(EXPR_INT);
      value->as.int_val = lit->attribute.int_val;
      break;
    case TOKEN_DOUBLE_LITERAL:
      value = make_expr(EXPR_DOUBLE);
      value->as.double_val = lit->attribute.double_val;
      break;
    case TOKEN_STR_LITERAL:
      value = make_expr(EXPR_STRING);
      value->as.str_val = ast_xstrdup(safe_str(lit));
      break;
    case TOKEN_UNIT_LITERAL:
      value = make_expr(EXPR_UNIT);
      break;
    case TOKEN_KW_TRUE:
    case TOKEN_KW_FALSE:
      value = make_expr(EXPR_BOOL);
      value->as.bool_val = (lit->type == TOKEN_KW_TRUE);
      break;
    default:
      parser_error(SYNTAX_ERROR, "Invalid pattern, got %s",
                   token_type_string(lit->type));
    }

    if (negative) {
      if (value->e_type == EXPR_INT) {
        value->as.int_val = -value->as.int_val;
      } else if (value->e_type == EXPR_DOUBLE) {
        value->as.double_val = -value->as.double_val;
      } else {
        parser_error(SYNTAX_ERROR, "Only numbers can be negated in a pattern");
      }
    }
    branch.literal = value;
  }

  if (token_match(stream, TOKEN_KW_IF)) {
    branch.guard = parse_expression(stream);
  }

  token_expect(stream, TOKEN_ARROW, "'=>'");
  branch.body = parse_body(stream);
  return branch;
}

// ALGEBDT: subject match + case branches
static expr *parse_match(TokenStream *stream, expr *subject) {
  expr *e = make_expr(EXPR_MATCH);
  e->as.match_.subject = subject;

  token_expect(stream, TOKEN_KW_MATCH, "'match'");
  if (!token_is(stream, TOKEN_NEWLINE)) {
    parser_error(SYNTAX_ERROR, "Expected newline after 'match'");
  }
  skip_blank_lines(stream);

  // The branches can be indented, or aligned with the subject (like in Scala)
  bool indented = token_match(stream, TOKEN_INDENT_START);
  skip_blank_lines(stream);

  while (token_is(stream, TOKEN_KW_CASE)) {
    APPEND(e->as.match_.cases, e->as.match_.case_count, parse_case(stream));
    skip_blank_lines(stream);
  }

  if (e->as.match_.case_count == 0) {
    parser_error(SYNTAX_ERROR, "Expected at least one 'case' branch");
  }
  if (indented) {
    token_expect(stream, TOKEN_INDENT_END, "indent end");
  }

  value_ended_with_block = true;
  return e;
}

// Generator source: from to/until to [by step], or a list
static expr *parse_for_source(TokenStream *stream) {
  expr *from = parse_expression(stream);

  if (!token_is(stream, TOKEN_IDENTIFIER)) {
    return from;
  }

  const char *word = safe_str(ts_peek(stream, 0));
  bool inclusive;
  if (strcmp(word, "to") == 0) {
    inclusive = true;
  } else if (strcmp(word, "until") == 0) {
    inclusive = false;
  } else {
    parser_error(SYNTAX_ERROR,
                 "Expected 'to', 'until' or 'do' in 'for', but got '%s'", word);
    return from;
  }
  ts_next(stream);

  expr *range = make_expr(EXPR_RANGE);
  range->as.range.from = from;
  range->as.range.inclusive = inclusive;
  range->as.range.to = parse_expression(stream);

  if (token_is(stream, TOKEN_IDENTIFIER) &&
      strcmp(safe_str(ts_peek(stream, 0)), "by") == 0) {
    ts_next(stream);
    range->as.range.step = parse_expression(stream);
  }
  return range;
}

// CONTROL: for loop with one generator and any number of guards
static stmt *parse_for_loop(TokenStream *stream) {
  stmt *statement = make_stmt(STMT_FOR);

  token_expect(stream, TOKEN_KW_FOR, "'for'");

  // Multiline header: generator and guards on separate indented lines
  bool multiline = token_is(stream, TOKEN_NEWLINE);
  if (multiline) {
    skip_blank_lines(stream);
    token_expect(stream, TOKEN_INDENT_START, "indent start");
    skip_blank_lines(stream);
  }

  const Token *var_token =
      token_expect(stream, TOKEN_IDENTIFIER, "loop variable");
  token_expect(stream, TOKEN_LEFT_ARROW, "'<-'");
  statement->as.for_.var = ast_xstrdup(safe_str(var_token));
  statement->as.for_.source = parse_for_source(stream);

  if (multiline) {
    check_statement_end(stream);
    skip_blank_lines(stream);
  }

  while (token_is(stream, TOKEN_KW_IF)) {
    ts_next(stream);
    APPEND(statement->as.for_.guards, statement->as.for_.guard_count,
           parse_expression(stream));
    if (multiline) {
      check_statement_end(stream);
      skip_blank_lines(stream);
    }
  }

  if (multiline) {
    token_expect(stream, TOKEN_INDENT_END, "indent end");
  }

  token_expect(stream, TOKEN_KW_DO, "'do'");
  statement->as.for_.body = parse_body(stream);
  return statement;
}