/**
 * IFJ Compiler - Expression parser header file
 * Author: David Pokorák (xpokord00)
 * Date: 15.10.2026
 **/

#include "expr_parser.h"
#include "error.h"
#include "lexer.h"

#include <stdlib.h>
#include <string.h>
/* Precedence table */

typedef enum {
    PS_U,   /* unary minus                */
    PS_MD,  /* * /                        */
    PS_AS,  /* + -                        */
    PS_REL, /* < <= > >=                  */
    PS_EQ,  /* == !=                      */
    PS_LP,  /* (                          */
    PS_RP,  /* )                          */
    PS_I,   /* literal, identifier, call  */
    PS_END, /* end of expression          */
    PS_COUNT,
    PS_E    /* nonterminal E              */
} prec_symbol;

/* '<' shift, '>' reduce, '=' shift, '#' error, 'A' accept.
 * Row = terminal on top of the stack, column = input symbol. */
static const char prec_table[PS_COUNT][PS_COUNT] = {
    /*          U    MD   AS   REL  EQ   (    )    i    $  */
    /* U   */ {'<', '>', '>', '>', '>', '<', '>', '<', '>'},
    /* MD  */ {'<', '>', '>', '>', '>', '<', '>', '<', '>'},
    /* AS  */ {'<', '<', '>', '>', '>', '<', '>', '<', '>'},
    /* REL */ {'<', '<', '<', '#', '>', '<', '>', '<', '>'},
    /* EQ  */ {'<', '<', '<', '<', '#', '<', '>', '<', '>'},
    /* (   */ {'<', '<', '<', '<', '<', '<', '=', '<', '#'},
    /* )   */ {'#', '>', '>', '>', '>', '#', '>', '#', '>'},
    /* i   */ {'#', '>', '>', '>', '>', '#', '>', '#', '>'},
    /* $   */ {'<', '<', '<', '<', '<', '<', '#', '<', 'A'},
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
static void parse_call_args(TokenStream *ts, expr *call) {
    int cap = 0;
    if (ts_peek(ts, 0)->type != TOKEN_RIGHT_PAREN) {
        for (;;) {
            if (call->as.call.argc == cap) {
                cap = cap ? cap * 2 : 4;
                call->as.call.args =
                    ast_xrealloc(call->as.call.args, (size_t)cap * sizeof(expr *));
            }
            call->as.call.args[call->as.call.argc++] = parse_expression(ts);
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

static bool is_term_token(token_type t) {
    switch (t) {
    case TOKEN_NUM_LITERAL:
    case TOKEN_INT_LITERAL:
    case TOKEN_DOUBLE_LITERAL:
    case TOKEN_STR_LITERAL:
    case TOKEN_UNIT_LITERAL:
    case TOKEN_IDENTIFIER:
        return true;
    default:
        return false;
    }
}

static expr *build_term(TokenStream *ts) {
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
    case TOKEN_IDENTIFIER: {
        char *name = ast_xstrdup(t->attribute.str_val);
        token_type next = ts_peek(ts, 0)->type;
        if (next == TOKEN_UNIT_LITERAL) { /* f() - lexer merges "()" */
            (void)ts_next(ts);
            e = make(EXPR_CALL);
            e->as.call.name = name;
            return e;
        }
        if (next == TOKEN_LEFT_PAREN) { /* f(args) */
            (void)ts_next(ts);
            e = make(EXPR_CALL);
            e->as.call.name = name;
            parse_call_args(ts, e);
            return e;
        }
        e = make(EXPR_VAR);
        e->as.name = name;
        return e;
    }
    default:
        throw_error(INTERNAL_ERROR, "build_term called on a non-term token.");
    }
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
        return (input){PS_I, OP_ADD};
    }
    switch (t->type) {
    case TOKEN_PLUS:       return (input){PS_AS, OP_ADD};
    case TOKEN_MINUS:
        return prev_operand ? (input){PS_AS, OP_SUB} : (input){PS_U, OP_NEG};
    case TOKEN_ASTERISK:   return (input){PS_MD, OP_MUL};
    case TOKEN_DIV:        return (input){PS_MD, OP_DIV};
    case TOKEN_LESS:       return (input){PS_REL, OP_LT};
    case TOKEN_LESS_EQ:    return (input){PS_REL, OP_LE};
    case TOKEN_GREATER:    return (input){PS_REL, OP_GT};
    case TOKEN_GREATER_EQ: return (input){PS_REL, OP_GE};
    case TOKEN_EQ:         return (input){PS_EQ, OP_EQ};
    case TOKEN_NOT_EQ:     return (input){PS_EQ, OP_NE};
    case TOKEN_LEFT_PAREN: return (input){PS_LP, OP_ADD};
    case TOKEN_RIGHT_PAREN:
        return depth > 0 ? (input){PS_RP, OP_ADD} : (input){PS_END, OP_ADD};
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
            throw_error(SYNTAX_ERROR, "Invalid expression."); /* "( )" or unbalanced */
        }
        stack_push(st, e);
        return;
    }

    case PS_E: { /* E -> U E   or   E -> E op E */
        item op = stack_pop(st);
        if (op.sym == PS_U) {
            stack_push(st, (item){PS_E, OP_NEG, make_unary(OP_NEG, x.expr)});
            return;
        }
        if (op.sym == PS_MD || op.sym == PS_AS || op.sym == PS_REL || op.sym == PS_EQ) {
            item left = stack_pop(st);
            if (left.sym != PS_E) {
                throw_error(SYNTAX_ERROR, "Invalid expression."); /* operator without a left operand */
            }
            stack_push(st, (item){PS_E, op.op, make_binary(op.op, left.expr, x.expr)});
            return;
        }
        throw_error(SYNTAX_ERROR, "Invalid expression.");
    }

    default: /* operator on top without right operand, e.g. "1 +" */
        throw_error(SYNTAX_ERROR, "Invalid expression.");
    }
}

expr *parse_expression(TokenStream *ts) {
    stack st = {NULL, 0, 0};
    bool prev_operand = false;
    int depth = 0;

    stack_push(&st, (item){PS_END, OP_ADD, NULL});

    while(true){
        input in = token_to_input(ts_peek(ts, 0), prev_operand, depth);
        prec_symbol top = stack_top_terminal(&st);
       
        switch(prec_table[top][in.sym]){
        case '<':
        case '=': /* shift  */
            if(in.sym == PS_I){
                stack_push(&st, (item){PS_I, OP_ADD, build_term(ts)});
            } else {
                (void)ts_next(ts);
                if(in.sym == PS_LP){
                    depth++;
                } else if(in.sym == PS_RP){
                    depth--;
                }
                stack_push(&st, (item){in.sym, in.op, NULL});
            }
            prev_operand = (in.sym == PS_I || in.sym == PS_RP);
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
        
        default:  /* '#' */
            throw_error(SYNTAX_ERROR, "Invalid expression.");
        }
        

    }
}