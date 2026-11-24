/**
 * IFJ Compiler - Expression parser implementation
 * Author: David Pokorák (xpokord00)
 * Date: 15.10.2026
 **/

#include "expr_parser.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"

#include <stdlib.h>
#include <string.h>
/* Precedence table */

typedef enum {
  PS_U,   /* unary minus                */
  PS_MD,  /* * /                        */
  PS_AS,  /* + -                        */
  PS_REL, /* < <= > >=                  */
  PS_EQ,  /* == !=                      */
  PS_AND, /* &&                         */
  PS_OR,  /* ||                         */
  PS_LP,  /* (                          */
  PS_RP,  /* )                          */
  PS_I,   /* literal, identifier, call  */
  PS_END, /* end of expression          */
  PS_COUNT,
  PS_E /* nonterminal E              */
} prec_symbol;

/* '<' shift, '>' reduce, '=' shift, '#' error, 'A' accept.
 * Row = terminal on top of the stack, column = input symbol. */
static const char prec_table[PS_COUNT][PS_COUNT] = {
    /*          U    MD   AS   REL  EQ   AND  OR   (    )    i    $ */
    /* U   */ {'<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>'},
    /* MD  */ {'<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>'},
    /* AS  */ {'<', '<', '>', '>', '>', '>', '>', '<', '>', '<', '>'},
    /* REL */ {'<', '<', '<', '#', '>', '>', '>', '<', '>', '<', '>'},
    /* EQ  */ {'<', '<', '<', '<', '#', '>', '>', '<', '>', '<', '>'},
    /* AND */ {'<', '<', '<', '<', '<', '>', '>', '<', '>', '<', '>'},
    /* OR  */ {'<', '<', '<', '<', '<', '<', '>', '<', '>', '<', '>'},
    /* (   */ {'<', '<', '<', '<', '<', '<', '<', '<', '=', '<', '#'},
    /* )   */ {'#', '>', '>', '>', '>', '>', '>', '#', '>', '#', '>'},
    /* i   */ {'#', '>', '>', '>', '>', '>', '>', '#', '>', '#', '>'},
    /* $   */ {'<', '<', '<', '<', '<', '<', '<', '<', '#', '<', 'A'},
};

/* Tree generation functions */
static expr *make(expr_type type) {
  expr *e = ast_xcalloc(1, sizeof *e);
  e->e_type = type;
  e->d_type = TYPE_UNKNOWN;
  e->line = 0;
  return e;
}
static expr *make_unary(op_type op, expr *operand) {
  expr *e = make(EXPR_UNARY);
  e->as.unary.op = op;
  e->as.unary.expr = operand;
  return e;
}

static expr *make_binary(op_type op, expr *left, expr *right) {
  expr *e = make(EXPR_BINARY);
  e->as.binary.op = op;
  e->as.binary.left = left;
  e->as.binary.right = right;
  return e;
}

static expr *number_from_string(const char *s) {
  expr *e;
  if (strpbrk(s, ".eE") != NULL) {
    e = make(EXPR_DOUBLE);
    e->as.double_val = strtod(s, NULL);
  } else {
    e = make(EXPR_INT);
    e->as.int_val = strtoll(s, NULL, 10);
  }
  return e;
}

/* Terms */

/**
 * Parses the arguments of a call or of a member after '(' up to and including
 * the closing ')'. An argument may be any value (a lambda, an if expression).
 * A trailing comma is allowed.
 **/
static void parse_arg_list(TokenStream *ts, expr ***args, int *argc) {
  if (ts_peek(ts, 0)->type != TOKEN_RIGHT_PAREN) {
    for (;;) {
      expr *arg = parse_value(ts);
      APPEND(*args, *argc, arg);
      if (ts_peek(ts, 0)->type != TOKEN_COMMA) {
        break;
      }
      (void)ts_next(ts);
      if (ts_peek(ts, 0)->type == TOKEN_RIGHT_PAREN) { /* trailing comma */
        break;
      }
    }
  }
  if (ts_peek(ts, 0)->type != TOKEN_RIGHT_PAREN) {
    throw_error(SYNTAX_ERROR, "Expected ')' after call arguments.");
  }
  (void)ts_next(ts);
}

/**
 * Builds a call after its name: f() (the lexer merges "()"), or f(args).
 **/
static expr *build_call(TokenStream *ts, char *name) {
  expr *e = make(EXPR_CALL);
  e->as.call.name = name;
  token_type next = ts_peek(ts, 0)->type;
  if (next == TOKEN_UNIT_LITERAL) {
    (void)ts_next(ts);
  } else if (next == TOKEN_LEFT_PAREN) {
    (void)ts_next(ts);
    parse_arg_list(ts, &e->as.call.args, &e->as.call.argc);
  } else {
    throw_error(SYNTAX_ERROR, "Expected '(' after the name of a call.");
  }
  return e;
}

/**
 * LISTS: List(a, b, ...) and List.range(a, b [, step]). The second form is
 * a call named "List.range".
 **/
static expr *build_list_call(TokenStream *ts) {
  if (ts_peek(ts, 0)->type != TOKEN_DOT) {
    return build_call(ts, ast_xstrdup("List"));
  }
  (void)ts_next(ts);
  const Token *member = ts_next(ts);
  if (member->type != TOKEN_IDENTIFIER ||
      strcmp(member->attribute.str_val, "range") != 0) {
    throw_error(SYNTAX_ERROR, "Expected 'range' after 'List.'.");
  }
  return build_call(ts, ast_xstrdup("List.range"));
}

/**
 * INTERPOLATEDS: the tokens after TOKEN_INTERP_START are string chunks and
 * embedded expressions up to TOKEN_INTERP_END. Every chunk and every
 * expression becomes one part of the node.
 **/
static expr *build_interp(TokenStream *ts) {
  expr *e = make(EXPR_INTERP);
  for (;;) {
    const Token *t = ts_next(ts);
    expr *part;
    if (t->type == TOKEN_INTERP_END) {
      break;
    }
    if (t->type == TOKEN_STR_LITERAL) {
      part = make(EXPR_STRING);
      part->as.str_val = ast_xstrdup(t->attribute.str_val);
    } else if (t->type == TOKEN_INTERP_EXPR_START) {
      part = parse_expression(ts);
      if (ts_next(ts)->type != TOKEN_INTERP_EXPR_END) {
        throw_error(SYNTAX_ERROR, "Invalid expression in interpolated string.");
      }
    } else {
      throw_error(SYNTAX_ERROR, "Invalid interpolated string.");
    }
    APPEND(e->as.interp.parts, e->as.interp.count, part);
  }
  return e;
}

/**
 * VALMEM, LISTS, ALGEBDT: applies the chain of members .name or .name(args)
 * that follows a term to the expression base.
 **/
static expr *parse_postfix(TokenStream *ts, expr *base) {
  while (ts_peek(ts, 0)->type == TOKEN_DOT) {
    (void)ts_next(ts);
    const Token *name = ts_next(ts);
    if (name->type != TOKEN_IDENTIFIER) {
      throw_error(SYNTAX_ERROR, "Expected a member name after '.'.");
    }
    expr *m = make(EXPR_MEMBER);
    m->as.member.obj = base;
    m->as.member.name = ast_xstrdup(name->attribute.str_val);

    token_type next = ts_peek(ts, 0)->type;
    if (next == TOKEN_UNIT_LITERAL) {
      (void)ts_next(ts);
    } else if (next == TOKEN_LEFT_PAREN) {
      (void)ts_next(ts);
      parse_arg_list(ts, &m->as.member.args, &m->as.member.argc);
    }
    base = m;
  }
  return base;
}

static bool is_term_token(token_type t) {
  switch (t) {
  case TOKEN_NUM_LITERAL:
  case TOKEN_INT_LITERAL:
  case TOKEN_DOUBLE_LITERAL:
  case TOKEN_STR_LITERAL:
  case TOKEN_UNIT_LITERAL:
  case TOKEN_KW_TRUE:
  case TOKEN_KW_FALSE:
  case TOKEN_KW_LIST:
  case TOKEN_INTERP_START:
  case TOKEN_IDENTIFIER:
    return true;
  default:
    return false;
  }
}

static expr *build_primary(TokenStream *ts) {
  const Token *t = ts_next(ts);
  expr *e;

  switch (t->type) {
  case TOKEN_NUM_LITERAL:
    return number_from_string(t->attribute.str_val);
  case TOKEN_INT_LITERAL:
    e = make(EXPR_INT);
    e->as.int_val = t->attribute.int_val;
    return e;
  case TOKEN_DOUBLE_LITERAL:
    e = make(EXPR_DOUBLE);
    e->as.double_val = t->attribute.double_val;
    return e;
  case TOKEN_STR_LITERAL:
    e = make(EXPR_STRING);
    e->as.str_val = ast_xstrdup(t->attribute.str_val);
    return e;
  case TOKEN_UNIT_LITERAL:
    return make(EXPR_UNIT);
  case TOKEN_KW_TRUE:
  case TOKEN_KW_FALSE:
    e = make(EXPR_BOOL);
    e->as.bool_val = (t->type == TOKEN_KW_TRUE);
    return e;
  case TOKEN_KW_LIST:
    return build_list_call(ts);
  case TOKEN_INTERP_START:
    return build_interp(ts);
  case TOKEN_IDENTIFIER: {
    char *name = ast_xstrdup(t->attribute.str_val);
    token_type next = ts_peek(ts, 0)->type;
    if (next == TOKEN_UNIT_LITERAL || next == TOKEN_LEFT_PAREN) {
      return build_call(ts, name); /* f() - lexer merges "()" - or f(args) */
    }
    e = make(EXPR_VAR);
    e->as.name = name;
    return e;
  }
  default:
    throw_error(INTERNAL_ERROR, "build_term called on a non-term token.");
  }
}

/**
 * Builds a term together with the members that follow it (x.floor.toInt).
 **/
static expr *build_term(TokenStream *ts) {
  return parse_postfix(ts, build_primary(ts));
}

/* Stack functions */
typedef struct {
  prec_symbol sym;
  op_type op;
  expr *expr;
} item;

typedef struct {
  item *items;
  int count;
  int cap;
} stack;

static void stack_push(stack *st, item it) {
  if (st->count == st->cap) {
    st->cap = st->cap ? st->cap * 2 : 16;
    st->items = ast_xrealloc(st->items, (size_t)st->cap * sizeof(item));
  }
  st->items[st->count++] = it;
}

static item stack_pop(stack *st) {
  if (st->count == 0) {
    throw_error(SYNTAX_ERROR, "Invalid expression.");
  }
  return st->items[--st->count];
}
static prec_symbol stack_top_terminal(const stack *st) {
  for (int i = st->count - 1; i >= 0; i--) {
    if (st->items[i].sym != PS_E) {
      return st->items[i].sym;
    }
  }
  return PS_END;
}

typedef struct {
  prec_symbol sym;
  op_type op;
} input;

static input token_to_input(const Token *t, bool prev_operand, int depth) {
  if (is_term_token(t->type)) {
    if (prev_operand) {
      /* a word after an operand (to, until, by, ...) ends the expression */
      return (input){PS_END, OP_ADD};
    }
    return (input){PS_I, OP_ADD};
  }
  switch (t->type) {
  case TOKEN_PLUS:
    return (input){PS_AS, OP_ADD};
  case TOKEN_PLUS_PLUS:
    return (input){PS_AS, OP_CONCAT};
  case TOKEN_MINUS:
    if (prev_operand) {
      return (input){PS_AS, OP_SUB};
    }
    return (input){PS_U, OP_NEG};
  case TOKEN_ASTERISK:
    return (input){PS_MD, OP_MUL};
  case TOKEN_DIV:
    return (input){PS_MD, OP_DIV};
  case TOKEN_LESS:
    return (input){PS_REL, OP_LT};
  case TOKEN_LESS_EQ:
    return (input){PS_REL, OP_LE};
  case TOKEN_GREATER:
    return (input){PS_REL, OP_GT};
  case TOKEN_GREATER_EQ:
    return (input){PS_REL, OP_GE};
  case TOKEN_EQ:
    return (input){PS_EQ, OP_EQ};
  case TOKEN_NOT_EQ:
    return (input){PS_EQ, OP_NE};
  case TOKEN_NOT:
    return (input){PS_U, OP_NOT};
  case TOKEN_AND:
    return (input){PS_AND, OP_AND};
  case TOKEN_OR:
    return (input){PS_OR, OP_OR};
  case TOKEN_LEFT_PAREN:
    return (input){PS_LP, OP_ADD};
  case TOKEN_RIGHT_PAREN:
    if (depth > 0) {
      return (input){PS_RP, OP_ADD};
    }
    return (input){PS_END, OP_ADD};
  default:
    return (input){PS_END, OP_ADD};
  }
}

/* Reduction */
static void reduce(stack *st) {
  item x = stack_pop(st);

  switch (x.sym) {
  case PS_I: /* E -> i */
    x.sym = PS_E;
    stack_push(st, x);
    return;

  case PS_RP: { /* E -> ( E ) */
    item e = stack_pop(st);
    item lp = stack_pop(st);
    if (e.sym != PS_E || lp.sym != PS_LP) {
      throw_error(SYNTAX_ERROR,
                  "Invalid expression."); /* "( )" or unbalanced */
    }
    stack_push(st, e);
    return;
  }

  case PS_E: { /* E -> U E   or   E -> E op E */
    item op = stack_pop(st);
    if (op.sym == PS_U) {
      stack_push(st, (item){PS_E, op.op, make_unary(op.op, x.expr)});
      return;
    }
    if (op.sym == PS_MD || op.sym == PS_AS || op.sym == PS_REL ||
        op.sym == PS_EQ || op.sym == PS_AND || op.sym == PS_OR) {
      item left = stack_pop(st);
      if (left.sym != PS_E) {
        throw_error(
            SYNTAX_ERROR,
            "Invalid expression."); /* operator without a left operand */
      }
      stack_push(st,
                 (item){PS_E, op.op, make_binary(op.op, left.expr, x.expr)});
      return;
    }
    throw_error(SYNTAX_ERROR, "Invalid expression.");
  }

  default: /* operator on top without right operand, e.g. "1 +" */
    throw_error(SYNTAX_ERROR, "Invalid expression.");
  }
}

/**
 * Is the token the word "to" or "until" (CONTROL ranges)?
 **/
static bool is_range_word(const Token *t) {
  if (t->type != TOKEN_IDENTIFIER) {
    return false;
  }
  return strcmp(t->attribute.str_val, "to") == 0 ||
         strcmp(t->attribute.str_val, "until") == 0;
}

/**
 * CONTROL: a range (from to/until to [by step]) inside parentheses, used for
 * example as (1 to 5).toList. Everything before the word is reduced to one
 * expression E on top of the left parenthesis, then the rest of the range is
 * parsed recursively and E is replaced by the range node.
 **/
static void parse_range_rest(TokenStream *ts, stack *st) {
  while (stack_top_terminal(st) != PS_LP) {
    if (stack_top_terminal(st) == PS_END) {
      throw_error(SYNTAX_ERROR, "Invalid expression.");
    }
    reduce(st);
  }
  item *top = &st->items[st->count - 1];
  if (top->sym != PS_E) {
    throw_error(SYNTAX_ERROR, "Invalid expression.");
  }

  const Token *word = ts_next(ts);
  expr *range = make(EXPR_RANGE);
  range->as.range.from = top->expr;
  range->as.range.inclusive = (strcmp(word->attribute.str_val, "to") == 0);
  range->as.range.to = parse_expression(ts);

  const Token *by = ts_peek(ts, 0);
  if (by->type == TOKEN_IDENTIFIER &&
      strcmp(by->attribute.str_val, "by") == 0) {
    (void)ts_next(ts);
    range->as.range.step = parse_expression(ts);
  }
  top->expr = range;
}

expr *parse_expression(TokenStream *ts) {
  stack st = {NULL, 0, 0};
  bool prev_operand = false;
  int depth = 0;

  stack_push(&st, (item){PS_END, OP_ADD, NULL});

  while (true) {
    if (prev_operand && depth > 0 && is_range_word(ts_peek(ts, 0))) {
      parse_range_rest(ts, &st);
      continue;
    }
    input in = token_to_input(ts_peek(ts, 0), prev_operand, depth);
    prec_symbol top = stack_top_terminal(&st);

    switch (prec_table[top][in.sym]) {
    case '<':
    case '=': /* shift  */
      if (in.sym == PS_I) {
        stack_push(&st, (item){PS_I, OP_ADD, build_term(ts)});
      } else {
        (void)ts_next(ts);
        if (in.sym == PS_LP) {
          depth++;
        } else if (in.sym == PS_RP) {
          depth--;
        }
        stack_push(&st, (item){in.sym, in.op, NULL});
      }
      prev_operand = (in.sym == PS_I || in.sym == PS_RP);
      if (in.sym == PS_RP && ts_peek(ts, 0)->type == TOKEN_DOT) {
        /* (a + b).member - the ')' is reduced now and the members are
         * applied to the whole parenthesized expression */
        reduce(&st);
        item *top = &st.items[st.count - 1];
        top->expr = parse_postfix(ts, top->expr);
      }
      break;
    case '>': /* reduce */
      reduce(&st);
      break;
    case 'A': /* accept */
      if (st.count != 2 || st.items[1].sym != PS_E) {
        throw_error(SYNTAX_ERROR, "Expected an expression.");
      }
      expr *result = st.items[1].expr;
      free(st.items);
      return result;

    default: /* '#' */
      throw_error(SYNTAX_ERROR, "Invalid expression.");
    }
  }
}